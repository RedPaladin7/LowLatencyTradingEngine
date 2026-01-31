#!/bin/bash

CMAKE=$(which cmake)
NINJA=$(which ninja)

mkdir -p cmake-build-release

$CMAKE -DCMAKE_BUILD_TYPE=Release -G Ninja -S . -B cmake-build-release

$CMAKE --build cmake-build-release --target socket_example -j 4

if [ $? -eq 0 ]; then
    echo -e "\n--- Launching socket_example ---\n"
    ./cmake-build-release/socket_example
fi