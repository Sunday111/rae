#!/usr/bin/bash

set -e

script_path=$(readlink -f "${BASH_SOURCE[0]}")
export script_dir=$(dirname "${script_path}")
source "$script_dir/get_emsdk.sh"

# $emsdk_dir/emsdk activate latest
# source $emsdk_dir/emsdk_env.h

$emsdk_dir/upstream/emscripten/emrun \
    --no_browser --port 8080 $wasm_build_dir
