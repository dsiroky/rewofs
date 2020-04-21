#!/bin/bash
set -e

cd "$( dirname "${BASH_SOURCE[0]}" )"
mkdir -p build
cd build
conan install --build missing ..
