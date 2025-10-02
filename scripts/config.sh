#!/usr/bin/bash

set -e

script_path=$(readlink -f "${BASH_SOURCE[0]}")
export scripts_dir=$(dirname "${script_path}")
export workspace_dir=$(readlink -f "$scripts_dir/..")
export cache_dir=$workspace_dir/cache

export llvm_version=21.1.2
export llvm_os_arch=linux-X64
export llvm_package_name=LLVM-${llvm_version}-${llvm_os_arch}
export llvm_archive_name=$llvm_package_name.tar.xz
export llvm_archive_path=$cache_dir/$llvm_archive_name
export llvm_wget_link="https://github.com/llvm/llvm-project/releases/download/llvmorg-${llvm_version}/$llvm_package_name.tar.xz"
export llvm_dir=$workspace_dir/llvm

export dawn_root_dir=$workspace_dir/dawn
export dawn_src_dir=$dawn_root_dir/repo
export dawn_build_dir=$dawn_root_dir/build
export dawn_install_dir=$dawn_root_dir/install

export project_build_dir=$workspace_dir/build

export cmake_toolchain_file=$workspace_dir/cmake/llvm-toolchain.cmake

export emsdk_dir=$workspace_dir/emsdk
export wasm_build_dir=$workspace_dir/build_wasm

if [[ ! -d $cache_dir ]]; then
    mkdir -p $cache_dir
fi
