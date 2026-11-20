#!/bin/bash
# Desktop build for development and tests: ./build_host.sh  ->  build-host/gorillas
set -e
cd "$(dirname "$0")"
mkdir -p build-host
gcc -O2 -g -Wall -Wno-unused-result -Isrc $(sdl2-config --cflags) src/*.c -o build-host/gorillas $(sdl2-config --libs) -lm
echo built build-host/gorillas
