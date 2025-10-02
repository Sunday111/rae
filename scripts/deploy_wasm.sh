#!/usr/bin/bash

set -e

script_path=$(readlink -f "${BASH_SOURCE[0]}")
export script_dir=$(dirname "${script_path}")
source "$script_dir/build_wasm.sh"

rm -rf $wasm_deploy_dir
mkdir $wasm_deploy_dir
cp $wasm_build_dir/index.html $wasm_deploy_dir
cp $wasm_build_dir/index.js $wasm_deploy_dir
cp $wasm_build_dir/index.wasm $wasm_deploy_dir
