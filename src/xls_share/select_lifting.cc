#include "resource_sharing.h"
#include "kernel/macc.h"

USING_YOSYS_NAMESPACE
namespace xls_share {

static bool has_output(const ModWalker &walker, const SigSpec &bits)
{
    for (auto bit : bits)
        if (walker.signal_outputs.count(bit))
            return true;
    return false;
}

bool eligible(Cell *cell, const ModWalker &walker)
{
    if (cell->get_bool_attribute(ID::keep))
        return false;
    if (cell->type.in(ID($add), ID($sub), ID($mul)))
        return true;
    if (cell->type == ID($macc_v2))
        return true;
    if (cell->type == ID($alu)) {
        auto ci = cell->getPort(ID::CI), bi = cell->getPort(ID::BI);
        if (!ci.is_fully_def() || !bi.is_fully_def() || ci != bi)
            return false;
        for (auto port : {ID::X, ID::CO}) {
            auto bits = walker.sigmap(cell->getPort(port));
            pool<ModWalker::PortBit> consumers;
            if (has_output(walker, bits) || walker.get_consumers(consumers, bits))
                return false;
        }
        return true;
    }
    return false;
}

bool compatible(Cell *a, Cell *b)
{
    if (a->type == ID($macc_v2) && b->type == ID($macc_v2))
        return true;
    auto additive = [](Cell *c) {
        return c->type.in(ID($add), ID($sub), ID($alu)) ||
               (c->type == ID($macc_v2) && c->getParam(ID::NPRODUCTS).as_int() == 0);
    };
    auto product = [](Cell *c) {
        return c->type == ID($mul) ||
               (c->type == ID($macc_v2) && c->getParam(ID::NPRODUCTS).as_int() == 1 &&
                c->getParam(ID::NADDENDS).as_int() == 0);
    };
    return (additive(a) && additive(b)) || (product(a) && product(b));
}

bool depends_on(const ModWalker &walker, SigSpec signal, SigSpec ancestor)
{
    pool<SigBit> target, visited;
    for (auto bit : walker.sigmap(ancestor))
        if (bit.wire)
            target.insert(bit);
    std::vector<SigBit> pending = walker.sigmap(signal).to_sigbit_vector();
    while (!pending.empty()) {
        auto bit = pending.back();
        pending.pop_back();
        if (target.count(bit))
            return true;
        if (!bit.wire || !visited.insert(bit).second)
            continue;
        if (walker.signal_drivers.count(bit))
            for (auto driver : walker.signal_drivers.at(bit))
                for (auto &port : driver.cell->connections())
                    if (walker.ct.cell_input(driver.cell->type, port.first))
                        for (auto input : walker.sigmap(port.second))
                            pending.push_back(input);
    }
    return false;
}

void lift_candidates(Design *design, Module *module, Visibility &proof, RewriteSearch &search)
{
    auto &walker = proof.walker;
    auto &map = walker.sigmap;
    for (auto mux : module->selected_cells()) {
        if (!mux->type.in(ID($mux), ID($pmux)) || mux->get_bool_attribute(ID::keep))
            continue;
        int width = GetSize(mux->getPort(ID::Y));
        std::vector<SigSpec> arms{map(mux->getPort(ID::A))};
        auto branches = map(mux->getPort(ID::B));
        for (int i = 0; i < GetSize(branches); i += width)
            arms.push_back(branches.extract(i, width));
        std::vector<Cell *> ops;
        std::vector<Cell *> bases;
        for (auto arm : arms) {
            pool<ModWalker::PortBit> drivers;
            walker.get_drivers(drivers, arm[0]);
            Cell *cell = GetSize(drivers) == 1 ? drivers.begin()->cell : nullptr;
            if (cell && (!design->selected(module, cell) || !eligible(cell, walker) ||
                         map(cell->getPort(ID::Y)) != arm))
                cell = nullptr;
            if (cell && cell->type == ID($macc_v2) &&
                (cell->getParam(ID::NPRODUCTS).as_int() != 1 ||
                 cell->getParam(ID::NADDENDS).as_int() != 0))
                cell = nullptr;
            ops.push_back(cell);
            if (cell)
                bases.push_back(cell);
        }
        for (auto base : bases) {
        bool add = base->type == ID($add) ||
                   (base->type == ID($alu) && !base->getPort(ID::BI).as_bool());
        bool sub = base->type == ID($sub) ||
                   (base->type == ID($alu) && base->getPort(ID::BI).as_bool());
        bool commutative = add || base->type.in(ID($mul), ID($macc_v2));
        for (auto common_port : {ID::A, ID::B}) {
            auto other_port = common_port == ID::A ? ID::B : ID::A;
            auto common = map(base->getPort(common_port));
            int other_width = GetSize(base->getPort(other_port));
            SigSpec identity = common;
            bool signed_op = base->getParam(ID::A_SIGNED).as_bool() &&
                             base->getParam(ID::B_SIGNED).as_bool();
            identity.extend_u0(width, signed_op);
            std::vector<SigSpec> inputs;
            pool<Cell *> matched;
            bool has_bypass = false;
            for (int i = 0; i < GetSize(arms); ++i) {
                auto op = ops[i];
                if ((add || (sub && common_port == ID::A)) && arms[i] == identity) {
                    inputs.push_back(SigSpec(State::S0, other_width));
                    has_bypass = true;
                } else if (op && op->type == base->type && op->parameters == base->parameters &&
                    (op->type != ID($alu) || (op->getPort(ID::CI) == base->getPort(ID::CI) &&
                                             op->getPort(ID::BI) == base->getPort(ID::BI)))) {
                    if (map(op->getPort(common_port)) == common)
                        inputs.push_back(op->getPort(other_port));
                    else if (commutative && GetSize(common) == other_width &&
                             map(op->getPort(other_port)) == common)
                        inputs.push_back(op->getPort(common_port));
                    else
                        break;
                    matched.insert(op);
                } else {
                    break;
                }
            }
            if (inputs.size() != arms.size())
                continue;
            auto required = mux->getPort(ID::S);
            // The bypass becomes an arithmetic expression after lifting.
            if (has_bypass)
                required.append(common);
            bool defined = true;
            for (auto bit : required)
                if (!proof.defined_bit(bit)) {
                    defined = false;
                    break;
                }
            if (!defined)
                continue;
            auto rewrite = std::make_unique<Rewrite>("lift");
            rewrite->removed.push_back(mux);
            for (auto op : matched) {
                auto bits = map(op->getPort(ID::Y));
                bool live = has_output(walker, bits);
                pool<ModWalker::PortBit> consumers;
                walker.get_consumers(consumers, bits);
                for (auto use : consumers)
                    live |= use.cell != mux;
                if (!live)
                    rewrite->removed.push_back(op);
            }
            auto &patch = rewrite->patch;
            auto shared = patch.addCell(NEW_ID, base);
            if (shared->type == ID($alu))
                for (auto port : {ID::X, ID::CO})
                    shared->setPort(port, patch.addWire(NEW_ID, GetSize(base->getPort(port))));
            auto selected = patch.addWire(NEW_ID, other_width);
            SigSpec input_cases;
            for (int i = 1; i < GetSize(inputs); ++i)
                input_cases.append(inputs[i]);
            if (mux->type == ID($mux))
                patch.addMux(NEW_ID, inputs[0], input_cases, mux->getPort(ID::S), selected);
            else
                patch.addPmux(NEW_ID, inputs[0], input_cases, mux->getPort(ID::S), selected);
            rewrite->original = mux->getPort(ID::Y);
            rewrite->replacement = patch.addWire(NEW_ID, width);
            shared->setPort(other_port, selected);
            shared->setPort(ID::Y, rewrite->replacement);
            search.offer(std::move(rewrite));
        }
        }
    }
}

void collapse_candidates(Design *design, Module *module, Visibility &proof, RewriteSearch &search)
{
    auto &walker = proof.walker;
    auto &map = walker.sigmap;
    auto &sat = proof.sat;

    auto chainable = [&](Cell *cell) {
        return cell->type.in(ID($mux), ID($pmux)) &&
               design->selected(module, cell) && !cell->get_bool_attribute(ID::keep);
    };
    auto child_at = [&](Cell *parent, IdString port) -> Cell * {
        auto value = map(parent->getPort(port));
        pool<ModWalker::PortBit> drivers;
        walker.get_drivers(drivers, value[0]);
        if (GetSize(drivers) != 1)
            return nullptr;
        auto child = drivers.begin()->cell;
        if (!chainable(child) || map(child->getPort(ID::Y)) != value)
            return nullptr;
        if (has_output(walker, value))
            return nullptr;
        pool<ModWalker::PortBit> consumers;
        walker.get_consumers(consumers, value);
        for (const auto &use : consumers)
            if (use.cell != parent || use.port != port)
                return nullptr;
        return child;
    };

    for (auto root : module->selected_cells()) {
        if (!chainable(root))
            continue;
        for (auto first_port : {ID::A, ID::B}) {
            if ((first_port == ID::B && root->type == ID($pmux)) ||
                !child_at(root, first_port))
                continue;

            SigSpec data, fallback;
            std::vector<int> guards;
            std::vector<Cell *> chain;
            int used = ezSAT::CONST_FALSE;
            auto cell = root;
            auto continuation = first_port;
            while (cell) {
                int prefix = used;
                std::vector<int> next_guards;
                auto bits = map(cell->getPort(ID::S));
                for (auto bit : bits) {
                    auto defined = proof.defined_bit(bit);
                    if (!defined)
                        break;
                    int guard = continuation == ID::B ? sat.ez->NOT(*defined) : *defined;
                    if (!proof.disjoint(prefix, guard))
                        break;
                    next_guards.push_back(guard);
                    prefix = sat.ez->OR(prefix, guard);
                }
                if (GetSize(next_guards) != GetSize(bits))
                    break;
                used = prefix;
                guards.insert(guards.end(), next_guards.begin(), next_guards.end());
                data.append(map(cell->getPort(continuation == ID::A ? ID::B : ID::A)));
                fallback = map(cell->getPort(continuation));
                chain.push_back(cell);
                cell = child_at(cell, continuation);
                continuation = ID::A;
                if (cell && cell->type == ID($mux) &&
                    !child_at(cell, ID::A) && child_at(cell, ID::B))
                    continuation = ID::B;
            }
            if (GetSize(chain) < 2)
                continue;

            auto rewrite = std::make_unique<Rewrite>("collapse");
            rewrite->removed = std::move(chain);
            rewrite->original = root->getPort(ID::Y);
            rewrite->replacement = rewrite->patch.addWire(NEW_ID, GetSize(fallback));
            SigSpec select;
            for (int guard : guards)
                select.append(proof.materialize(&rewrite->patch, guard));
            rewrite->patch.addPmux(NEW_ID, fallback, data, select, rewrite->replacement);
            proof.emitted.clear();
            search.offer(std::move(rewrite));
        }
    }
}

}
