#!/usr/bin/env bash
source /usr/local/Modules/init/bash
module load compilers/gcc-13.1.0
datapath_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PATH="$datapath_root/tools/local/bin:$PATH"
