#!/bin/bash

# Exit on error
set -e

echo "--- Cleaning old build files ---"
rm -rf build
mkdir build
cd build

echo "--- Configuring and Building ---"
cmake ..
make -j$(nproc)

echo "--- Build Complete ---"
echo "To run the exchange, execute: ./build/exchange_main"