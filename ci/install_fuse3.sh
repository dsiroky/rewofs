#!/bin/bash
set -ex

cd /tmp

wget https://github.com/libfuse/libfuse/archive/fuse-3.9.1.tar.gz
tar -xpf fuse-3.9.1.tar.gz
cd libfuse-fuse-3.9.1
mkdir build
cd build
meson ..
ninja -j$(nproc)
sudo ninja install
