#pragma once
#include "visibility.h"
#include "kernel/macc.h"
#include <cstdint>
#include <memory>

namespace xls_share {
using Group = std::vector<Yosys::IdString>;
using Area = std::int64_t;

struct Rewrite {
    const char *kind;
    Yosys::Module patch;
    std::vector<Yosys::Cell *> removed;
    Yosys::SigSpec original, replacement;

    explicit Rewrite(const char *kind) : kind(kind) {}
    void apply(Yosys::Design *, Yosys::Module *);
};

struct RewriteSearch {
    const Yosys::ModWalker &walker;
    std::unique_ptr<Rewrite> best;
    Area delta = 0;
    int count = 0, best_index = 0;

    explicit RewriteSearch(const Yosys::ModWalker &walker) : walker(walker) {}
    void offer(std::unique_ptr<Rewrite>);
    bool apply(Yosys::Design *, Yosys::Module *);
};

std::vector<Group> candidates(Yosys::Module *, Visibility &, bool report);
std::unique_ptr<Rewrite> make_share(Yosys::Module *, Visibility &, const Group &);
Yosys::Macc operation_macc(Yosys::Cell *, const Yosys::SigMap &);
Yosys::Cell *share_arithmetic(Yosys::Module *, Visibility &, const std::vector<Yosys::Cell *> &);
void collapse_candidates(Yosys::Design *, Yosys::Module *, Visibility &, RewriteSearch &);
void lift_candidates(Yosys::Design *, Yosys::Module *, Visibility &, RewriteSearch &);
}
