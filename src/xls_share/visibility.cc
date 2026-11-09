#include "visibility.h"

USING_YOSYS_NAMESPACE
namespace xls_share {
Visibility::Visibility(ModWalker &walker) : walker(walker), sat(walker) {}

int Visibility::control(SigBit bit)
{
    bit = walker.sigmap(bit);
    int token = sat.importSigBit(bit);
    controls[token] = bit;
    return token;
}

// This proof instance models 0/1/X with defined primary inputs.
std::optional<int> Visibility::defined_bit(SigBit bit)
{
    if (!undef_domain_valid)
        return std::nullopt;
    bit = walker.sigmap(bit);
    if (!bit.wire) {
        if (bit == State::S0 || bit == State::S1)
            return control(bit);
        return std::nullopt;
    }
    if (walker.signal_inputs.count(bit))
        return control(bit);
    if (!sat.satgen.model_undef) {
        sat.satgen.model_undef = true;
        for (auto input : walker.signal_inputs)
            sat.ez->assume(sat.ez->NOT(sat.satgen.importUndefSigBit(input)));
    }
    int guard = control(bit);
    if (defined_bits.count(guard))
        return guard;
    int previous = GetSize(sat.imported_cells);
    sat.prepare();
    int imported = GetSize(sat.imported_cells) - previous;
    // Other constant states are outside SatGen's undef domain.
    for (int i = 0; i < imported; ++i) {
        auto driver = *sat.imported_cells.element(i);
        for (const auto &port : driver->connections())
            if (driver->input(port.first))
                for (auto input : walker.sigmap(port.second))
                    if (!input.wire && input != State::S0 && input != State::S1 && input != State::Sx) {
                        undef_domain_valid = false;
                        return std::nullopt;
                    }
    }
    if (sat.ez->solve(sat.satgen.importUndefSigBit(bit)))
        return std::nullopt;
    defined_bits.insert(guard);
    return guard;
}

int Visibility::of_bit(SigBit bit)
{
    bit = walker.sigmap(bit);
    if (!bit.wire)
        return ezSAT::CONST_FALSE;
    if (walker.signal_outputs.count(bit))
        return ezSAT::CONST_TRUE;
    if (uses.count(bit))
        return uses.at(bit);
    std::vector<int> edges;
    if (walker.signal_consumers.count(bit))
        for (auto use : walker.signal_consumers.at(bit))
            edges.push_back(on_edge(use.cell, use.port, use.offset));
    return uses[bit] = sat.ez->expression(ezSAT::OpOr, edges);
}

int Visibility::on_edge(Cell *cell, IdString port, int offset)
{
    if (cell->type.in(ID($mux), ID($pmux)) && port.in(ID::A, ID::B)) {
        auto y = cell->getPort(ID::Y), select = cell->getPort(ID::S);
        int condition = of_bit(y[offset % GetSize(y)]);
        int selected;
        if (port == ID::A) {
            std::vector<int> bits;
            for (auto s : select)
                bits.push_back(control(s));
            selected = sat.ez->NOT(sat.ez->expression(ezSAT::OpOr, bits));
        } else {
            selected = control(select[offset / GetSize(y)]);
        }
        return sat.ez->AND(condition, selected);
    }
    if (cell->type.in(ID($and), ID($or)) && port.in(ID::A, ID::B)) {
        auto y = cell->getPort(ID::Y);
        auto mask = walker.sigmap(cell->getPort(port == ID::A ? ID::B : ID::A));
        bool broadcast = std::all_of(mask.begin(), mask.end(), [&](SigBit bit) {
            return bit == mask[0];
        });
        bool sign = cell->getParam(ID::A_SIGNED).as_bool() &&
                    cell->getParam(ID::B_SIGNED).as_bool();
        mask.extend_u0(GetSize(y), sign);
        int end = offset + 1;
        if (sign && end == GetSize(cell->getPort(port)))
            end = GetSize(y);
        std::vector<int> positions;
        for (int pos = offset; pos < std::min(end, GetSize(y)); ++pos) {
            int observed = of_bit(y[pos]);
            // Recognize broadcast control masks and constant bits. A data word
            // from another arithmetic branch is kept as an ordinary consumer.
            if ((mask[pos].wire && broadcast) || mask[pos] == State::S0 || mask[pos] == State::S1) {
                int mask_bit = control(mask[pos]);
                if (cell->type == ID($or))
                    mask_bit = sat.ez->NOT(mask_bit);
                observed = sat.ez->AND(observed, mask_bit);
            }
            positions.push_back(observed);
        }
        return sat.ez->expression(ezSAT::OpOr, positions);
    }
    std::vector<int> outputs;
    for (auto out : walker.cell_outputs.at(cell))
        outputs.push_back(of_bit(out));
    return sat.ez->expression(ezSAT::OpOr, outputs);
}

int Visibility::of_cell(Cell *cell)
{
    std::vector<int> bits;
    for (auto bit : cell->getPort(ID::Y))
        bits.push_back(of_bit(bit));
    return sat.ez->expression(ezSAT::OpOr, bits);
}

bool Visibility::disjoint(int a, int b)
{
    int both = sat.ez->AND(a, b);
    if (both == ezSAT::CONST_FALSE)
        return true;
    sat.prepare();
    return !sat.ez->solve(both);
}

bool Visibility::exclusive(Cell *a, Cell *b)
{
    return disjoint(of_cell(a), of_cell(b));
}

const pool<int> &Visibility::required_literals(int expression)
{
    if (required.count(expression))
        return required.at(expression);

    pool<int> result;
    if (expression != ezSAT::CONST_TRUE && expression != ezSAT::CONST_FALSE) {
        if (controls.count(expression)) {
            result.insert(expression);
        } else {
            ezSAT::OpId op;
            auto args = sat.ez->lookup_expression(expression, op);
            if (op == ezSAT::OpNot && controls.count(args[0])) {
                result.insert(expression);
            } else if (op == ezSAT::OpAnd || op == ezSAT::OpOr) {
                bool first = true;
                for (int arg : args) {
                    const auto &literals = required_literals(arg);
                    if (op == ezSAT::OpAnd) {
                        for (int literal : literals)
                            result.insert(literal);
                    } else if (first) {
                        result = literals;
                    } else {
                        pool<int> common;
                        for (int literal : result)
                            if (literals.count(literal)) common.insert(literal);
                        result.swap(common);
                    }
                    first = false;
                }
            }
        }
    }
    return required[expression] = std::move(result);
}

std::vector<int> Visibility::routing(const std::vector<Cell *> &ops)
{
    std::vector<int> chosen;
    for (auto op : ops)
        chosen.push_back(of_cell(op));
    std::vector<int> suffix(ops.size()+1, ezSAT::CONST_FALSE);
    for (int i = GetSize(ops)-1; i >= 0; --i)
        suffix[i] = sat.ez->OR(chosen[i], suffix[i+1]);

    int occupied = chosen[0]; // The first operation supplies the default data.
    for (int i = 1; i < GetSize(ops); ++i) {
        int used = chosen[i];
        int others = sat.ez->OR(occupied, suffix[i+1]);
        const auto &required = required_literals(used);
        std::vector<int> literals(required.begin(), required.end());
        std::sort(literals.begin(), literals.end(), [&](int a, int b) {
            return std::make_pair(!controls.count(a), a) < std::make_pair(!controls.count(b), b);
        });
        // Each required literal covers the original use. Exclude the remaining
        // original uses and earlier enlarged guards to keep the routing one-hot.
        for (int literal : literals)
            if (literal != used && disjoint(literal, others)) {
                chosen[i] = literal;
                break;
            }
        occupied = sat.ez->OR(occupied, chosen[i]);
    }
    return chosen;
}

SigSpec Visibility::support(int expression) const
{
    SigSpec result;
    pool<int> visited;
    std::vector<int> pending{expression};
    while (!pending.empty()) {
        int token = pending.back();
        pending.pop_back();
        if (!visited.insert(token).second)
            continue;
        if (controls.count(token)) {
            result.append(controls.at(token));
        } else if (token < 0) {
            ezSAT::OpId op;
            for (int arg : sat.ez->lookup_expression(token, op))
                pending.push_back(arg);
        }
    }
    return result;
}

SigBit Visibility::materialize(Module *module, int expression)
{
    if (expression == ezSAT::CONST_TRUE)
        return State::S1;
    if (expression == ezSAT::CONST_FALSE)
        return State::S0;
    if (controls.count(expression))
        return controls.at(expression);
    if (emitted.count(expression))
        return emitted.at(expression);
    ezSAT::OpId op;
    auto args = sat.ez->lookup_expression(expression, op);
    auto y = module->addWire(NEW_ID, 1);
    if (op == ezSAT::OpNot) {
        module->addNot(NEW_ID, materialize(module, args[0]), y);
    } else {
        log_assert(op == ezSAT::OpAnd || op == ezSAT::OpOr);
        SigSpec inputs;
        for (auto arg : args)
            inputs.append(materialize(module, arg));
        if (op == ezSAT::OpAnd)
            module->addReduceAnd(NEW_ID, inputs, y);
        else
            module->addReduceOr(NEW_ID, inputs, y);
    }
    return emitted[expression] = SigBit(y);
}

bool specialize_once(Design *design, Module *module)
{
    ModWalker walker(design, module);
    Visibility visibility(walker);
    pool<SigBit> selectors;
    for (auto cell : module->cells())
        if (cell->type.in(ID($mux), ID($pmux)))
            for (auto bit : walker.sigmap(cell->getPort(ID::S)))
                selectors.insert(bit);

    dict<std::pair<int, int>, State> implied;
    auto known = [&](int condition, SigBit bit) {
        bit = walker.sigmap(bit);
        if (!bit.wire)
            return bit.data;
        int token = visibility.control(bit);
        auto key = std::make_pair(condition, token);
        if (!implied.count(key)) {
            State value = State::Sx;
            if (visibility.disjoint(condition, visibility.sat.ez->NOT(token)))
                value = State::S1;
            else if (visibility.disjoint(condition, token))
                value = State::S0;
            implied[key] = value;
        }
        return implied.at(key);
    };

    dict<std::pair<Cell *, int>, std::optional<int>> branches;
    auto choose_branch = [&](Cell *mux, int condition) -> std::optional<int> {
        auto key = std::make_pair(mux, condition);
        if (branches.count(key))
            return branches.at(key);
        auto select = mux->getPort(ID::S);
        int branch = 0;
        for (int i = 0; i < GetSize(select); ++i) {
            auto state = known(condition, select[i]);
            if (state == State::S0)
                continue;
            if (state != State::S1 || branch != 0)
                return branches[key] = std::nullopt;
            branch = i+1;
        }
        return branches[key] = branch;
    };

    for (auto cell : module->selected_cells()) {
        if (cell->get_bool_attribute(ID::keep))
            continue;
        for (auto &port : cell->connections()) {
            if (!walker.ct.cell_input(cell->type, port.first))
                continue;
            auto inputs = walker.sigmap(port.second);
            auto replacement = port.second;
            int changed = 0;
            for (int offset = 0; offset < GetSize(inputs); ++offset) {
                auto bit = inputs[offset];
                if (!bit.wire)
                    continue;
                pool<ModWalker::PortBit> drivers;
                walker.get_drivers(drivers, bit);
                bool from_select = GetSize(drivers) == 1 && drivers.begin()->port == ID::Y &&
                                   drivers.begin()->cell->type.in(ID($mux), ID($pmux));
                bool control_as_data = cell->type.in(ID($mux), ID($pmux)) &&
                                       port.first.in(ID::A, ID::B) && selectors.count(bit);
                if (!from_select && !control_as_data)
                    continue;
                int condition = visibility.on_edge(cell, port.first, offset);
                auto value = bit;
                if (from_select) {
                    auto source = *drivers.begin();
                    auto branch = choose_branch(source.cell, condition);
                    if (branch) {
                        if (*branch == 0) {
                            value = source.cell->getPort(ID::A)[source.offset];
                        } else {
                            int width = GetSize(source.cell->getPort(ID::Y));
                            value = source.cell->getPort(ID::B)[(*branch-1)*width + source.offset];
                        }
                    }
                }
                if (value == bit && control_as_data && condition != ezSAT::CONST_TRUE) {
                    auto state = known(condition, bit);
                    if (state == State::S0 || state == State::S1)
                        value = state;
                }
                if (walker.sigmap(value) != bit) {
                    replacement[offset] = value;
                    ++changed;
                }
            }
            if (changed) {
                log("XLS condition specialize %s.%s: %d bits.\n",
                    log_id(cell), log_id(port.first), changed);
                cell->setPort(port.first, replacement);
                design->scratchpad_set_bool("opt.did_something", true);
                return true;
            }
        }
    }
    return false;
}
}
