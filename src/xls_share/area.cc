#include "resource_sharing.h"
#include "area_model.h"
#include <set>
#include <tuple>
USING_YOSYS_NAMESPACE
namespace xls_share {

static double library_area(Area area)
{
    return double(area) / characterized::scale;
}

// The same token identifies a wire, simplified selector output or arithmetic
// input. A gate contributes area only if a retained root needs it.
struct BitCost {
    struct Node {
        Area area = 0;
        int begin = 0, end = 0;
    };
    dict<SigBit, SigBit> aliases;
    dict<SigBit, int> inputs;
    std::map<std::tuple<int, int, int, int>, int> gates;
    std::vector<Node> nodes{{}, {}};
    std::vector<int> dependencies;

    SigBit resolve(SigBit bit) const {
        auto it = aliases.find(bit);
        return it == aliases.end() ? bit : it->second;
    }
    int input(SigBit bit) {
        bit = resolve(bit);
        if (!bit.wire) return bit == State::S1 ? 1 : 0;
        if (!inputs.count(bit)) inputs[bit] = fresh();
        return inputs.at(bit);
    }
    void bind(SigBit bit, int value) {
        if (bit.wire) inputs[bit] = value;
    }
    int fresh() {
        int id = GetSize(nodes);
        nodes.emplace_back();
        return id;
    }
    template <typename Inputs>
    int node(Area area, const Inputs &operands) {
        int id = GetSize(nodes), begin = GetSize(dependencies);
        dependencies.insert(dependencies.end(), operands.begin(), operands.end());
        nodes.push_back({area, begin, GetSize(dependencies)});
        return id;
    }
    int make(int op, int a, int b, int s, Area area) {
        auto key = std::make_tuple(op, a, b, s);
        auto it = gates.find(key);
        if (it != gates.end()) return it->second;
        return gates[key] = node(area, std::array<int, 3>{a, b, s});
    }
    Area measure(std::vector<int> pending) const {
        std::vector<bool> counted(nodes.size());
        Area area = 0;
        while (!pending.empty()) {
            int id = pending.back();
            pending.pop_back();
            if (id < 2 || counted[id]) continue;
            counted[id] = true;
            auto &node = nodes[id];
            area += node.area;
            pending.insert(pending.end(), dependencies.begin() + node.begin,
                           dependencies.begin() + node.end);
        }
        return area;
    }
    int inv(int a) {
        return a < 2 ? 1-a : make(0, a, 0, 0, characterized::inv);
    }
    int both(int a, int b) {
        if (a > b) std::swap(a, b);
        if (a == 0) return 0;
        if (a == 1 || a == b) return b;
        return make(1, a, b, 0, characterized::and2);
    }
    int either(int a, int b) {
        if (a > b) std::swap(a, b);
        if (a == 1) return 1;
        if (a == 0 || a == b) return b;
        return make(2, a, b, 0, characterized::or2);
    }
    int reduce(std::vector<int> values, bool is_and) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        int result = is_and ? 1 : 0;
        for (int value : values)
            result = is_and ? both(result, value) : either(result, value);
        return result;
    }
    int mux(int a, int b, int s) {
        if (s < 2) return s ? b : a;
        if (a == b) return a;
        if (a == 0) return both(b, s);
        if (b == 0) return both(a, inv(s));
        if (a == 1) return either(b, inv(s));
        if (b == 1) return either(a, s);
        return make(3, a, b, s, characterized::mux2);
    }
};

// Source model: maccmap.cc partial products and bit columns, followed by 3:2
// compression and a characterized carry-propagate adder. Compression topology
// and cross-cell ABC rewriting are deliberately outside this additive model.
static Area arithmetic_area(Cell *cell, const SigMap &sigmap, BitCost &cost, std::vector<int> &roots)
{
    auto macc = operation_macc(cell, sigmap);
    int width = GetSize(cell->getPort(ID::Y));
    log_assert(width <= characterized::max_width);
    Area area = 0;
    std::vector<std::set<int>> columns(width);
    auto bits = [&](SigSpec sig) {
        std::vector<int> value;
        for (auto bit : sig) value.push_back(cost.input(bit));
        return value;
    };
    auto add_bit = [&](int bit, int position) {
        while (position < width && bit != 0) {
            if (columns[position].insert(bit).second) break;
            columns[position++].erase(bit);
        }
    };
    auto add_row = [&](std::vector<int> value, bool sign, bool subtract) {
        value.resize(width, sign ? value.back() : 0);
        if (subtract) add_bit(1, 0);
        for (int i = 0; i < width; ++i)
            add_bit(subtract ? cost.inv(value[i]) : value[i], i);
    };
    for (auto &term : macc.terms) {
        auto a = bits(term.in_a), b = bits(term.in_b);
        if (b.empty()) {
            add_row(a, term.is_signed, term.do_subtract);
            continue;
        }
        if (a.size() < b.size()) std::swap(a, b);
        a.resize(width, term.is_signed ? a.back() : 0);
        if (GetSize(b) > width) b.resize(width);
        for (int i = 0; i < GetSize(b); ++i) {
            bool negative_row = term.is_signed && i+1 == GetSize(b);
            std::vector<int> row(width, 0);
            for (int j = i; j < width; ++j)
                if (b[i] != 0)
                    row[j] = cost.both(negative_row ? cost.inv(a[j]) : a[j], b[i]);
            add_row(row, false, term.do_subtract);
            if (negative_row) {
                std::vector<int> correction(width, 0);
                correction[i] = b[i];
                add_row(correction, false, term.do_subtract);
            }
            for (int j = width-1; j > 0; --j) a[j] = a[j-1];
            a[0] = 0;
        }
    }
    // A duplicate bit in one column carries to the next column (x+x=2x).
    // Three remaining bits require one compressor; top carry is truncated.
    for (const auto &column : columns)
        roots.insert(roots.end(), column.begin(), column.end());
    for (int i = 0; i < width; ++i) {
        auto &column = columns[i];
        while (column.size() > 2) {
            bool has_one = *column.begin() == 1;
            for (int k = 0; k < 3; ++k) column.erase(column.begin());
            column.insert(cost.fresh());
            if (i+1 < width) {
                columns[i+1].insert(cost.fresh());
                // Y=(a^b)^1 is XNOR, X=ab|(a^b) is OR. The library's
                // XOR2X1 and XNOR2X1 have the same area.
                area += has_one ? characterized::xor2 + characterized::or2
                                     : characterized::fa;
            } else {
                area += (has_one ? 1 : 2) * characterized::xor2;
            }
        }
    }
    int low = width, high = -1;
    for (int i = 0; i < width; ++i) {
        if (columns[i].size() == 2) low = std::min(low, i);
        if (!columns[i].empty()) high = i;
    }
    if (low < width)
        area += characterized::add[std::min(width, high+2) - low];
    return area;
}

static void account_logic(Cell *cell, const SigMap &sigmap, BitCost &cost)
{
    auto port = [&](IdString name) {
        std::vector<int> value;
        for (auto bit : sigmap(cell->getPort(name))) value.push_back(cost.input(bit));
        return value;
    };
    auto a = port(ID::A);
    auto y = sigmap(cell->getPort(ID::Y));
    if (cell->type == ID($not)) {
        for (int i = 0; i < GetSize(a); ++i)
            cost.bind(y[i], cost.inv(a[i]));
    } else if (cell->type.in(ID($reduce_and), ID($reduce_or))) {
        cost.bind(y[0], cost.reduce(a, cell->type == ID($reduce_and)));
    } else {
        log_assert(cell->type.in(ID($mux), ID($pmux)));
        auto b = port(ID::B), s = port(ID::S);
        int width = GetSize(a);
        for (int i = 0; i < width; ++i) {
            if (cell->type == ID($mux)) {
                cost.bind(y[i], cost.mux(a[i], b[i], s[0]));
                continue;
            }
            std::map<int, std::vector<int>> groups;
            std::vector<int> enabled;
            for (int j = 0; j < GetSize(s); ++j) {
                int data = b[j*width+i];
                if (data == a[i] || s[j] == 0)
                    continue;
                groups[data].push_back(s[j]);
                if (a[i] != 0)
                    enabled.push_back(s[j]);
            }
            int result = a[i];
            if (groups.size() == 1) {
                const auto &entry = *groups.begin();
                result = cost.mux(a[i], entry.first, cost.reduce(entry.second, false));
            } else if (!groups.empty()) {
                std::vector<int> terms;
                for (const auto &entry : groups)
                    terms.push_back(cost.both(entry.first, cost.reduce(entry.second, false)));
                int value = cost.reduce(std::move(terms), false);
                result = a[i] == 0 ? value : cost.mux(a[i], value, cost.reduce(std::move(enabled), false));
            }
            cost.bind(y[i], result);
        }
    }
}

// Arithmetic outputs have distinct identities and share one cost node.
static Area network_area(const std::vector<Cell *> &cells, const SigMap &sigmap,
                         const pool<SigBit> &outputs, BitCost &cost)
{
    dict<SigBit, Cell *> drivers;
    for (auto cell : cells)
        for (auto bit : sigmap(cell->getPort(ID::Y)))
            if (bit.wire) drivers[bit] = cell;
    pool<Cell *> visited;
    std::function<void(Cell *)> visit = [&](Cell *cell) {
        if (!visited.insert(cell).second) return;
        for (const auto &port : cell->connections())
            if (cell->input(port.first))
                for (auto bit : sigmap(port.second)) {
                    auto driver = drivers.find(cost.resolve(bit));
                    if (driver != drivers.end()) visit(driver->second);
                }

        if (cell->type.in(ID($mux), ID($pmux), ID($not), ID($reduce_and), ID($reduce_or))) {
            account_logic(cell, sigmap, cost);
        } else {
            std::vector<int> inputs;
            Area area = arithmetic_area(cell, sigmap, cost, inputs);
            int operation = cost.node(area, inputs);
            for (auto bit : sigmap(cell->getPort(ID::Y)))
                cost.bind(bit, cost.node(0, std::array<int, 1>{operation}));
        }
    };
    for (auto cell : cells) visit(cell);
    std::vector<int> roots;
    for (auto bit : outputs) roots.push_back(cost.input(bit));
    return cost.measure(std::move(roots));
}

static std::pair<Area, Area> estimate(Rewrite &rewrite, const ModWalker &walker)
{
    const auto &sigmap = walker.sigmap;
    BitCost before, after;
    auto old_outputs = sigmap(rewrite.original);
    auto new_outputs = sigmap(rewrite.replacement);
    for (int i = 0; i < GetSize(old_outputs); ++i)
        if (old_outputs[i].wire)
            after.aliases[old_outputs[i]] = new_outputs[i];

    pool<Cell *> region(rewrite.removed.begin(), rewrite.removed.end());
    std::vector<Cell *> old_cells = rewrite.removed, new_cells;
    std::vector<SigBit> pending = old_outputs.to_sigbit_vector();
    while (!pending.empty()) {
        auto bit = pending.back();
        pending.pop_back();
        auto uses = walker.signal_consumers.find(bit);
        if (uses == walker.signal_consumers.end()) continue;
        for (auto use : uses->second) {
            auto cell = use.cell;
            if (!cell->type.in(ID($mux), ID($pmux)) || !region.insert(cell).second) continue;
            old_cells.push_back(cell);
            new_cells.push_back(cell);
            for (auto out : sigmap(cell->getPort(ID::Y))) pending.push_back(out);
        }
    }
    for (auto cell : rewrite.patch.cells()) new_cells.push_back(cell);

    pool<SigBit> roots;
    for (auto cell : old_cells)
        for (auto bit : sigmap(cell->getPort(ID::Y))) {
            if (walker.signal_outputs.count(bit))
                roots.insert(bit);
            auto uses = walker.signal_consumers.find(bit);
            if (uses != walker.signal_consumers.end())
                for (auto use : uses->second)
                    if (!region.count(use.cell)) roots.insert(bit);
        }
    return {network_area(old_cells, sigmap, roots, before),
            network_area(new_cells, sigmap, roots, after)};
}

void Rewrite::apply(Design *design, Module *module)
{
    dict<Wire *, Wire *> wires;
    for (auto wire : patch.wires())
        wires[wire] = module->addWire(wire->name, wire);
    auto translate = [&](SigSpec signal) {
        signal.rewrite_wires([&](Wire *&wire) {
            auto it = wires.find(wire);
            if (it != wires.end()) wire = it->second;
        });
        return signal;
    };
    for (auto cell : patch.cells()) {
        auto added = module->addCell(cell->name, cell);
        for (const auto &port : cell->connections())
            added->setPort(port.first, translate(port.second));
    }
    log("XLS apply %s:", kind);
    for (auto cell : removed) {
        log(" %s", log_id(cell));
        module->remove(cell);
    }
    log("\n");
    module->connect(original, translate(replacement));
    design->scratchpad_set_bool("opt.did_something", true);
}

void RewriteSearch::offer(std::unique_ptr<Rewrite> rewrite)
{
    auto [before, after] = estimate(*rewrite, walker);
    Area change = after - before;
    log("XLS estimate candidate: index=%d size=%d removed=%.9f added=%.9f delta=%.9f kind=%s.\n",
        count, GetSize(rewrite->removed), library_area(before), library_area(after),
        library_area(change), rewrite->kind);
    if (!best || change < delta) {
        delta = change;
        best_index = count;
        best = std::move(rewrite);
    }
    ++count;
}

bool RewriteSearch::apply(Design *design, Module *module)
{
    if (!best)
        return false;
    log("XLS area selected=%d delta=%.9f kind=%s index=%d.\n",
        delta < 0, library_area(delta), best->kind, best_index);
    if (delta >= 0)
        return false;
    best->apply(design, module);
    return true;
}
}
