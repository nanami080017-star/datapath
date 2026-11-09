#pragma once
#include "operations.h"
#include "kernel/qcsat.h"

namespace xls_share {
struct Visibility {
    Yosys::ModWalker &walker;
    Yosys::QuickConeSat sat;
    Yosys::dict<Yosys::SigBit, int> uses;
    Yosys::dict<int, Yosys::SigBit> controls;
    Yosys::dict<int, Yosys::SigBit> emitted;
    Yosys::dict<int, Yosys::pool<int>> required;

    Yosys::pool<int> defined_bits;
    bool undef_domain_valid = true;

    explicit Visibility(Yosys::ModWalker &walker);
    int control(Yosys::SigBit bit);
    std::optional<int> defined_bit(Yosys::SigBit bit);
    int of_bit(Yosys::SigBit bit);
    int on_edge(Yosys::Cell *cell, Yosys::IdString port, int offset);
    int of_cell(Yosys::Cell *cell);
    bool disjoint(int a, int b);
    bool exclusive(Yosys::Cell *a, Yosys::Cell *b);
    const Yosys::pool<int> &required_literals(int expression);
    std::vector<int> routing(const std::vector<Yosys::Cell *> &ops);
    Yosys::SigSpec support(int expression) const;
    Yosys::SigBit materialize(Yosys::Module *module, int expression);
};
bool specialize_once(Yosys::Design*, Yosys::Module*);
}
