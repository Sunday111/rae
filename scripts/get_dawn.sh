#!/usr/bin/bash

set -e

script_path=$(readlink -f "${BASH_SOURCE[0]}")
export script_dir=$(dirname "${script_path}")
source "$script_dir/get_llvm.sh"

if [[ ! -d $dawn_src_dir ]]; then
    git clone https://dawn.googlesource.com/dawn $dawn_src_dir
fi

# if [[ ! -d $dawn_install_dir ]]; then
#     cmake                                           \
#         -G Ninja                                    \
#         -S $dawn_src_dir                            \
#         -B $dawn_build_dir                          \
#         -DCMAKE_BUILD_TYPE=Release                  \
#         -DCMAKE_INSTALL_PREFIX=$dawn_install_dir    \
#         -DDAWN_FETCH_DEPENDENCIES=ON                \
#         -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC      \
#         -DDAWN_BUILD_SAMPLES=OFF

#     cmake --build $dawn_build_dir
#     cmake --install $dawn_build_dir --prefix $dawn_install_dir
# fi
