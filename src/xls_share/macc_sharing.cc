/*
 * Adapted from Yosys passes/opt/share.cc (28ba3cb9), share_macc_ports/share_macc.
 * Copyright (C) 2012 Claire Xenia Wolf <claire@yosyshq.com>
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "resource_sharing.h"
#include "kernel/macc.h"
#include <set>
USING_YOSYS_NAMESPACE
namespace xls_share {

static SigSpec select_input(Module *module, SigSpec a, SigSpec b, SigSpec active,
                            std::vector<Cell *> &muxes)
{
    auto output = module->addWire(NEW_ID, GetSize(a));
    muxes.push_back(module->addMux(NEW_ID, a, b, active, output));
    return output;
}

// Every selector comes from a different, pairwise-exclusive source in this group.
static void parallelize_inputs(Module *module, const std::vector<Cell *> &muxes)
{
    dict<SigSpec, Cell *> drivers;
    for (auto mux : muxes)
        drivers[mux->getPort(ID::Y)] = mux;
    for (auto it = muxes.rbegin(); it != muxes.rend(); ++it) {
        auto root = *it;
        SigSpec data = root->getPort(ID::B), select = root->getPort(ID::S);
        SigSpec fallback = root->getPort(ID::A);
        while (drivers.count(fallback)) {
            auto inner = drivers.at(fallback);
            data.append(inner->getPort(ID::B));
            select.append(inner->getPort(ID::S));
            fallback = inner->getPort(ID::A);
        }
        if (GetSize(select) < 2)
            continue;
        auto output = root->getPort(ID::Y);
        module->addPmux(NEW_ID, fallback, data, select, output);
        drivers.erase(output);
        module->remove(root);
    }
}

static int pair_terms(const Macc::term_t &a, const Macc::term_t &b, int wa, int wb,
                      Module *module, SigSpec active, Macc *merged, std::vector<Cell *> *muxes)
{
    if (a.do_subtract != b.do_subtract || a.in_b.empty() != b.in_b.empty())
        return -1;
    bool sign = false, unsign = false;
    for (auto item : {std::make_pair(&a, wa), std::make_pair(&b, wb)}) {
        auto &t = *item.first;
        if ((!t.in_a.empty() && GetSize(t.in_a) < item.second) ||
            (!t.in_b.empty() && GetSize(t.in_b) < item.second)) {
            sign |= t.is_signed;
            unsign |= !t.is_signed;
        }
    }
    if (sign && unsign)
        return -1;
    if (merged) {
        Macc::term_t term;
        term.is_signed = sign;
        term.do_subtract = a.do_subtract;
        for (int port = 0; port < 2; ++port) {
            SigSpec x = port ? a.in_b : a.in_a, y = port ? b.in_b : b.in_a;
            int width = std::max(GetSize(x), GetSize(y));
            x.extend_u0(width, a.is_signed);
            y.extend_u0(width, b.is_signed);
            SigSpec value = x;
            if (x != y)
                value = select_input(module, x, y, active, *muxes);
            (port ? term.in_b : term.in_a) = value;
        }
        merged->terms.push_back(term);
    }
    int score = 1000 + abs(GetSize(a.in_a) - GetSize(b.in_a)) *
                      std::max(abs(GetSize(a.in_b) - GetSize(b.in_b)), 1);
    for (int port = 0; port < 2; ++port) {
        auto &x = port ? a.in_b : a.in_a;
        auto &y = port ? b.in_b : b.in_a;
        for (int i = 0; i < std::min(GetSize(x), GetSize(y)); ++i)
            if (x[i] == y[i] && score > 0)
                --score;
    }
    return score;
}

static Macc merge_terms(Module *module, const Macc &a, const Macc &b,
                        int wa, int wb, SigSpec active, std::vector<Cell *> &muxes)
{
    std::set<int> left, right;
    for (int i = 0; i < GetSize(a.terms); ++i) left.insert(i);
    for (int i = 0; i < GetSize(b.terms); ++i) right.insert(i);
    Macc merged;
    while (!left.empty() && !right.empty()) {
        int ai = -1, bi = -1, best = 0;
        Macc::term_t chosen;
        for (int i : left)
            for (int j : right)
                for (int flip = 0; flip < (b.terms[j].in_b.empty() ? 1 : 2); ++flip) {
                    auto term = b.terms[j];
                    if (flip)
                        std::swap(term.in_a, term.in_b);
                    int score = pair_terms(a.terms[i], term, wa, wb, nullptr, {}, nullptr, nullptr);
                    if (score >= 0 && (ai < 0 || score < best)) {
                        ai = i, bi = j, best = score;
                        chosen = term;
                    }
                }
        if (ai < 0)
            break;
        pair_terms(a.terms[ai], chosen, wa, wb, module, active, &merged, &muxes);
        left.erase(ai);
        right.erase(bi);
    }
    for (int side = 0; side < 2; ++side)
        for (int i : side ? right : left) {
            auto term = (side ? b : a).terms[i];
            for (auto sig : {&term.in_a, &term.in_b})
                if (!sig->empty()) {
                    SigSpec zero(State::S0, GetSize(*sig));
                    *sig = side ? select_input(module, zero, *sig, active, muxes)
                                : select_input(module, *sig, zero, active, muxes);
                }
            merged.terms.push_back(term);
        }
    return merged;
}

Macc operation_macc(Cell *op, const SigMap &sigmap)
{
    Macc input;
    if (op->type == ID($macc_v2)) {
        input.from_cell(op);
    } else {
        bool sign = op->getParam(ID::A_SIGNED).as_bool() &&
            op->getParam(ID::B_SIGNED).as_bool();
        if (op->type == ID($mul)) {
            input.terms.push_back({op->getPort(ID::A), op->getPort(ID::B), sign, false});
        } else {
            bool sub = op->type == ID($sub) ||
                       (op->type == ID($alu) && op->getPort(ID::BI).as_bool());
            input.terms.push_back({op->getPort(ID::A), {}, sign, false});
            input.terms.push_back({op->getPort(ID::B), {}, sign, sub});
        }
    }
    int w = GetSize(op->getPort(ID::Y));
    input.optimize(w);
    for (auto &term : input.terms) {
        sigmap.apply(term.in_a);
        sigmap.apply(term.in_b);
    }
    return input;
}

Cell *share_arithmetic(Module *module, Visibility &visibility, const std::vector<Cell*> &ops)
{
    Macc merged;
    std::vector<Cell *> muxes;
    int width = 0;
    auto routing = visibility.routing(ops);
    for (int i = 0; i < GetSize(ops); ++i) {
        auto op = ops[i];
        auto input = operation_macc(op, visibility.walker.sigmap);
        int w = GetSize(op->getPort(ID::Y));
        if (i == 0) {
            merged = input;
        } else {
            merged = merge_terms(module, merged, input, width, w,
                                 visibility.materialize(module, routing[i]), muxes);
        }
        width = std::max(width, w);
    }
    auto shared = module->addCell(NEW_ID, ID($macc_v2));
    shared->setPort(ID::Y, module->addWire(NEW_ID, width));
    shared->setParam(ID::Y_WIDTH, width);
    merged.optimize(width);
    merged.to_cell(shared);
    parallelize_inputs(module, muxes);
    return shared;
}
}
