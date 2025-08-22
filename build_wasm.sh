#!/bin/bash

# Ensure Emscripten is activated
source ./emsdk/emsdk_env.sh

# Create build directory
mkdir -p build_wasm
cd build_wasm

# Configure with Emscripten
emcmake cmake .. -DEMSCRIPTEN=ON -DCMAKE_BUILD_TYPE=Release

# Build
emmake make -j4

# Copy output files to a web directory
mkdir -p ../web
cp CountVonCountWasm.js ../web/
cp CountVonCountWasm.wasm ../web/