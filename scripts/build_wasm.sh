#!/usr/bin/bash

set -e

script_path=$(readlink -f "${BASH_SOURCE[0]}")
export script_dir=$(dirname "${script_path}")
source "$script_dir/get_emsdk.sh"

# source $emsdk_dir/emsdk_env.h

$emsdk_dir/upstream/emscripten/emcmake \
    cmake \
        -S $workspace_dir \
        -B $wasm_build_dir \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
    && \
    cmake \
        --build $wasm_build_dir
