#include "resource_sharing.h"
#include <set>
USING_YOSYS_NAMESPACE
namespace xls_share {

static const char *pair_reason(Cell *a, Cell *b, Visibility &visibility)
{
    auto &walker = visibility.walker;
    if (!compatible(a, b))
        return "parameters";
    SigSpec outputs = a->getPort(ID::Y);
    outputs.append(b->getPort(ID::Y));
    SigSpec inputs;
    for (auto cell : {a, b}) {
        for (auto &port : cell->connections())
            if (walker.ct.cell_input(cell->type, port.first))
                inputs.append(port.second);
        inputs.append(visibility.support(visibility.of_cell(cell)));
    }
    if (depends_on(walker, inputs, outputs))
        return "dependency";
    if (!visibility.exclusive(a, b))
        return "simultaneous-use";
    return nullptr;
}

std::vector<Group> candidates(Module *module, Visibility &visibility, bool report)
{
    std::vector<Cell *> ops;
    for (auto cell : module->selected_cells())
        if (eligible(cell, visibility.walker))
            ops.push_back(cell);
    std::sort(ops.begin(), ops.end(), [](Cell *a, Cell *b) {
        int aw = GetSize(a->getPort(ID::Y)), bw = GetSize(b->getPort(ID::Y));
        return aw != bw ? aw > bw : a->name < b->name;
    });
    std::vector<std::vector<bool>> adjacent(ops.size(), std::vector<bool>(ops.size()));
    for (int i = 0; i < GetSize(ops); ++i)
        for (int j = i + 1; j < GetSize(ops); ++j) {
            const char *reason = pair_reason(ops[i], ops[j], visibility);
            adjacent[i][j] = adjacent[j][i] = reason == nullptr;
            if (report)
                log("XLS pair %s %s: %s\n", log_id(ops[i]), log_id(ops[j]),
                    reason ? reason : "legal");
        }
    // First-fit batches follow XLS's n-ary folding construction.
    std::set<Group> unique;
    for (int i = 0; i < GetSize(ops); ++i) {
        std::vector<std::vector<int>> batches;
        for (int j = 0; j < GetSize(ops); ++j) {
            if (!adjacent[i][j])
                continue;
            Group pair{ops[i]->name, ops[j]->name};
            std::sort(pair.begin(), pair.end());
            unique.insert(std::move(pair));

            auto batch = std::find_if(batches.begin(), batches.end(), [&](const std::vector<int> &members) {
                return std::all_of(members.begin(), members.end(), [&](int k) { return adjacent[k][j]; });
            });
            if (batch == batches.end()) {
                batches.push_back({j});
                continue;
            }
            batch->push_back(j);
            Group group{ops[i]->name};
            for (int k : *batch)
                group.push_back(ops[k]->name);
            std::sort(group.begin(), group.end());
            unique.insert(std::move(group));
        }
    }
    return {unique.begin(), unique.end()};
}

std::unique_ptr<Rewrite> make_share(Module *module, Visibility &visibility, const Group &group)
{
    auto rewrite = std::make_unique<Rewrite>("share");
    for (auto name : group)
        rewrite->removed.push_back(module->cell(name));
    auto shared = share_arithmetic(&rewrite->patch, visibility, rewrite->removed);
    visibility.emitted.clear();
    auto result = shared->getPort(ID::Y);
    for (auto op : rewrite->removed) {
        const auto &output = op->getPort(ID::Y);
        rewrite->original.append(output);
        rewrite->replacement.append(result.extract(0, GetSize(output)));
    }
    return rewrite;
}
}
