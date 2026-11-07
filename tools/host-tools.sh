#!/usr/bin/env bash
# Tools for engine work on an x86_64 Linux host, unpacked into the deps/ that
# Port Ex Machina's repositories share -- nothing is installed on the machine.
# Pinned Arch Linux packages, like the sysroot's headers; docs/ENGINE.md
# says how they are used. The repositories' parent folder is $DX_ROOT, or the
# one this clone is in; the workspace is its port-ex-machina/.
#
#   deps/perf           Linux perf and the four libraries it needs
#   deps/vulkan-layers  the Khronos validation layer, with a manifest in
#                       layers/ that points at it by absolute path -- run
#                       this again after the parent folder moves
set -euo pipefail
DX_ROOT="${DX_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
[ -f "$DX_ROOT/port-ex-machina/scripts/lib/common.sh" ] ||
    { echo "error: no Port Ex Machina workspace at $DX_ROOT/port-ex-machina (set DX_ROOT)" >&2; exit 1; }
. "$DX_ROOT/port-ex-machina/scripts/lib/common.sh"

perf="$DX_DEPS/perf"
say "perf -> $perf"
dx_fetch_arch_pkg "$perf" perf          7.2.7-1               x86_64 8d50898372942241a12443b86ae9c5708619dbc718dfd5fcf39a03351c49dce3 usr/bin/perf
dx_fetch_arch_pkg "$perf" slang         2.3.3-4               x86_64 272e4afe2bff678a590d28abbeba8eda95b2041c5d0063e01a8a0f457b13a177 usr/lib
dx_fetch_arch_pkg "$perf" numactl       2.0.19-1              x86_64 699bce74daee11fa49de93999ba4e023f132bc51a7768dec845b2979d54f367d usr/lib
dx_fetch_arch_pkg "$perf" libpfm        4.13.0+r83+g91970fe-1 x86_64 d1f7e9d7bb42749d688bf356a90b8227ea4576cc438713b2a7fdfb7a8c2cf2ba usr/lib
dx_fetch_arch_pkg "$perf" libtraceevent 1:1.9.0-1             x86_64 40bd5d4fd4c3d4a0c8be3cd2af9cbc570ec3dfe8b5047cf4854d742c22cfcd2e usr/lib
LD_LIBRARY_PATH="$perf/usr/lib" "$perf/usr/bin/perf" --version >/dev/null ||
    die "perf does not run -- a library it needs is missing from $perf/usr/lib"

layers="$DX_DEPS/vulkan-layers"
say "validation layer -> $layers"
dx_fetch_arch_pkg "$layers" vulkan-validation-layers 1.4.357.0-1 x86_64 6f24c667b61e374b88789950f15e819cf4b32e9a1297a525520eed53d4077cf9 usr/lib usr/share/vulkan
# The packaged manifest names the library without a path, for a system install.
mkdir -p "$layers/layers"
sed "s|\"libVkLayer_khronos_validation.so\"|\"$layers/usr/lib/libVkLayer_khronos_validation.so\"|" \
    "$layers/usr/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json" \
    > "$layers/layers/VkLayer_khronos_validation.json"

say "perf:       LD_LIBRARY_PATH=$perf/usr/lib $perf/usr/bin/perf ..."
say "validation: VK_LAYER_PATH=$layers/layers VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true <engine> ..."
