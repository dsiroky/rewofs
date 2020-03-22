#!/bin/bash
set -ex

cd /tmp

wget https://github.com/google/flatbuffers/archive/v1.11.0.tar.gz
tar -xpf v1.11.0.tar.gz &&
cd flatbuffers-1.11.0
mkdir build
cd build
cmake -DFLATBUFFERS_BUILD_TESTS=OFF -DFLATBUFFERS_BUILD_FLATHASH=OFF \
    ..
make -j$(nproc)
sudo make install
