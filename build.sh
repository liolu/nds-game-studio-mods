#!/bin/bash
# Compile les mods natifs de ce depot pour Windows 32 bits (le module des jeux) : lib/windows-x86/<mod>.dll dans chaque dossier.
# Sans bibliotheque C : gcc -m32 de WinLibs et ld, comme le runtime. Les mods ne connaissent que sdk/nds_mod.h.
#   ./build.sh            tous les mods qui ont un dossier src/
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
for src in "$HERE"/*/src/*.c; do
  mod="$(dirname "$(dirname "$src")")"; name="$(basename "$src" .c)"
  out="$mod/lib/windows-x86"; mkdir -p "$out"
  gcc -m32 -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -fno-builtin -fno-stack-protector -mno-stack-arg-probe -c "$src" -o "$out/$name.o"
  ld -m i386pe --dll -e _DllMainCRTStartup@12 --enable-reloc-section --dynamicbase "$out/$name.o" -o "$out/$name.dll"
  rm "$out/$name.o"
  echo "$(basename "$mod")/lib/windows-x86/$name.dll $(stat -c %s "$out/$name.dll") octets"
done
