#!/bin/bash
# Compile les mods natifs de ce depot pour Windows 64 bits (le module des jeux depuis l'etape 6) : lib/windows-x86_64/<mod>.dll
# dans chaque dossier. Sans bibliotheque C (-nostdlib) : les mods de base n'en ont pas besoin. Ils ne connaissent que sdk/nds_mod.h.
#   ./build.sh            tous les mods qui ont un dossier src/
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
for src in "$HERE"/*/src/*.c; do
  mod="$(dirname "$(dirname "$src")")"; name="$(basename "$src" .c)"
  out="$mod/lib/windows-x86_64"; mkdir -p "$out"
  gcc -m64 -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -fno-builtin -fno-stack-protector -mno-stack-arg-probe -shared -nostdlib -Wl,-e,DllMainCRTStartup "$src" -o "$out/$name.dll"
  echo "$(basename "$mod")/lib/windows-x86_64/$name.dll $(stat -c %s "$out/$name.dll") octets"
done
