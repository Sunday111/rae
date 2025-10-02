#!/usr/bin/bash

set -e

script_path=$(readlink -f "${BASH_SOURCE[0]}")
export script_dir=$(dirname "${script_path}")
source "$script_dir/config.sh"

if [[ ! -d $llvm_dir ]]; then
    if [[ ! -f $llvm_archive_path ]]; then
        wget $llvm_wget_link -O $llvm_archive_path
    fi

    mkdir -p $llvm_dir
    tar -C $llvm_dir -xf $llvm_archive_path --strip-components=1
fi