#!/usr/bin/bash

set -e

script_path=$(readlink -f "${BASH_SOURCE[0]}")
export script_dir=$(dirname "${script_path}")
source "$script_dir/get_llvm.sh"

if [[ ! -d $dawn_src_dir ]]; then
    git clone https://dawn.googlesource.com/dawn $dawn_src_dir
fi