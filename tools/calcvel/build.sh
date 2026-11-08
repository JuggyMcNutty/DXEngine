#!/bin/sh
# Builds calcvel_test into <out> (a 32-bit Linux program: gcc and g++ with
# -m32), DXEngine's calcVelocity taken from its source as it stands:
#   tools/calcvel/build.sh <out>
# then <out>/calcvel_test <gamefiles/System> [cases] [-single]
set -e
here=$(cd "$(dirname "$0")" && pwd)
engine=$(cd "$here/../.." && pwd)/DXEngine
out=${1:?usage: build.sh <out>}
mkdir -p "$out"
python3 -I "$here/extract.py" "$engine/Packages/Engine/Actors/UActor_PhysWalking.cpp" "$out/calcvel_body.inc"
gcc -m32 -O1 -Wall -c "$here/pe_loader.c" -o "$out/pe_loader.o"
gcc -m32 -O1 -Wall -I "$here" -c "$here/calcvel_test.c" -o "$out/calcvel_test.o"
g++ -m32 -O2 -std=c++20 -msse2 -mfpmath=sse -ffp-contract=off -Wall -I "$out" -I "$engine" -c "$here/calcvel_shim.cpp" -o "$out/calcvel_shim.o"
g++ -m32 -o "$out/calcvel_test" "$out/pe_loader.o" "$out/calcvel_test.o" "$out/calcvel_shim.o" -lm
