#!/usr/bin/bash

set -e

script_path=$(readlink -f "${BASH_SOURCE[0]}")
export script_dir=$(dirname "${script_path}")
source "$script_dir/config.sh"

if [[ ! -d $emsdk_dir ]]; then
    git clone https://github.com/emscripten-core/emsdk.git $emsdk_dir
fi


$emsdk_dir/emsdk install latest
