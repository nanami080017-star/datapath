#pragma once
#include "kernel/modtools.h"

namespace xls_share {
bool eligible(Yosys::RTLIL::Cell *cell, const Yosys::ModWalker &walker);
bool compatible(Yosys::RTLIL::Cell *a, Yosys::RTLIL::Cell *b);
bool depends_on(const Yosys::ModWalker &walker, Yosys::RTLIL::SigSpec signal,
                Yosys::RTLIL::SigSpec ancestor);
}
