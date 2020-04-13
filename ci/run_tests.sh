#!/bin/bash
set -ex

scons debug=1
./build/last/tests/unit/run

scons debug=0
./build/last/tests/unit/run

scons debug=0 run_e2e_tests
