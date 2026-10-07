#!/bin/bash
set -eu
cd "$(dirname "$0")"

auto_compile_flags=""
for arg in "$@"; do declare $arg='1'; done
if [[ "$#" == "0" ]]; then raddbg='1'; fi
if [[ "$#" == "1" && "${release:-0}" == "1" ]]; then raddbg='1'; fi
if [[ "${asan:-0}" == "1" ]]; then
  echo "[asan enabled]"
  auto_compile_flags="$auto_compile_flags -fsanitize=address"
fi

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  auto_compile_flags="$auto_compile_flags -DBUILD_GIT_HASH=\"$(git describe --always --dirty)\" -DBUILD_GIT_HASH_FULL=$(git rev-parse HEAD)"
fi

cc_cflags_clang="-fdiagnostics-absolute-paths -Wno-for-loop-analysis -Wno-incompatible-pointer-types-discards-qualifiers -Wno-initializer-overrides -Wno-compare-distinct-pointer-types -Wno-single-bit-bitfield-constant-conversion -Wno-deprecated-declarations -Wno-writable-strings -Wno-unknown-warning-option -Wno-deprecated-register -Wno-unused-local-typedef"
cc_common="${auto_compile_flags} -I../src/ -I../local/ -g -Wall -Wno-missing-braces -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value -D_USE_MATH_DEFINES -Dgnu_printf=printf"
cc_debug="-g -O0 -DBUILD_DEBUG=1 ${cc_common}"
cc_release="-g -O2 -DBUILD_DEBUG=0 ${cc_common}"

cc_objc="-x objective-c"
cc_font_provider="-framework CoreText"
cc_os_gfx="-framework Cocoa"
cc_render="-framework Metal -framework QuartzCore"

compiler="xcrun clang $cc_cflags_clang"
if   [[ "${release:-0}" == "1" ]]; then echo "[release mode]"; compile="$compiler $cc_release";
elif [[ "${debug:-1}"   == "1" ]]; then echo "[debug mode]";   compile="$compiler $cc_debug";
fi

mkdir -p build local

if [[ "${meta:-0}" == "1" ]]
then
  echo "[building metagen]"
  cd build
  $compiler $cc_debug ../src/metagen/metagen_main.c -o metagen
  ./metagen
  cd ..
  didbuild=1
fi

cd build
if [[ "${raddbg:-0}"               == "1" ]]; then didbuild=1 && $compile $cc_objc ../src/raddbg/raddbg_main.c $cc_os_gfx $cc_render $cc_font_provider -o raddbg; fi
if [[ "${raddbg_non_graphical:-0}" == "1" ]]; then didbuild=1 && $compile $cc_objc ../src/raddbg/raddbg_main.c -DWM_STUB=1 -DR_BACKEND=R_BACKEND_STUB $cc_os_gfx $cc_font_provider -o raddbg_non_graphical; fi
if [[ "${radbin:-0}"               == "1" ]]; then didbuild=1 && $compile ../src/radbin/radbin_main.c -o radbin; fi
if [[ "${metal_window_rad:-0}"     == "1" ]]; then didbuild=1 && $compile $cc_objc ../src/scratch/metal_window/rad_main.c $cc_os_gfx $cc_render $cc_font_provider -o metal_window_rad; fi
cd ..

if [[ "${didbuild:-0}" == "0" ]]
then
  echo "[WARNING] no valid build target specified; must use build target names as arguments to this script, like \`./build_mac.sh raddbg\` or \`./build_mac.sh radbin\`."
  exit 1
fi
