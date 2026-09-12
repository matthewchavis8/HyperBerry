# Quick Commands

default:
  @just --list

# Build every board's image (one configure, one build).
build MODE="debug":
  cmake --preset {{ MODE }}
  cmake --build --preset {{ MODE }}
  @echo "[LOG] images under build/{{ MODE }}/<board>/kernel8.img"

qemu MODE="debug":
  cmake --preset {{ MODE }}
  cmake --build --preset {{ MODE }} --target run-qemu

fvp MODE="debug":
  cmake --preset {{ MODE }}
  cmake --build --preset {{ MODE }} --target run-fvp

rpi5 MODE="release" SD_DEV="/dev/sdd1":
  cmake --preset {{ MODE }}
  cmake --build --preset {{ MODE }} --target hyperberry-rpi5

  @echo "[LOG] Mounting SD Card for flashing"
  sudo mkdir -p /mnt/sdcard
  sudo mount -o uid=$(id -u),gid=$(id -g) {{ SD_DEV }} /mnt/sdcard

  cmake --build --preset {{ MODE }} --target flash-rpi5
  sync
  sudo umount /mnt/sdcard
  @echo "[LOG] Physical Raspberry PI5 has succesfully been built and flash"

docs:
  doxygen docs/sphinx/Doxyfile
  docs/.venv/bin/sphinx-build -b html docs/sphinx docs/_build/html
  @echo "[LOG] Sphinx docs generated at docs/_build/html/index.html"
  @echo "[LOG] Serving docs at http://localhost:8000 (Ctrl+C to stop)"
  python3 -m http.server 8000 --directory docs/_build/html

docs-clean:
  rm -rf docs/_doxygen
  rm -rf docs/_build

test-integration BOARD="qemu":
  cmake --preset debug
  cmake --build --preset debug --target hyperberry-{{ BOARD }}-test
  cmake --build --preset debug --target run-{{ BOARD }}-test 2>&1 \
    | tee build/debug/{{ BOARD }}/integration/qemu.log

# Point clangd at the build compile database.
compile-db:
  cmake --preset debug
  ln -sf build/debug/compile_commands.json compile_commands.json
  @echo "[LOG] compile_commands.json linked"

test-unit:
  cmake --preset unit-tests
  cmake --build --preset unit-tests
  ctest --preset unit-tests

guestpkg KERNEL DTB OUT="boot/profiles/guest-qemu.hvgbp" INITRD="" BUILD_ID="":
  cargo run --manifest-path tools/mkguestpkg/Cargo.toml -- \
    --kernel {{ KERNEL }} \
    --dtb {{ DTB }} \
    --out {{ OUT }} \
    {{ if INITRD != "" { "--initrd " + INITRD } else { "" } }} \
    {{ if BUILD_ID != "" { "--build-id " + BUILD_ID } else { "" } }}

clean:
  rm -rf build/

# --- Style ---

# Format every tracked C++ source in place.
fmt:
  git ls-files '*.cpp' '*.h' | xargs clang-format --style=file -i
  @echo "[LOG] clang-format applied"

# Fail if anything is not formatted. This is the CI gate.
fmt-check:
  git ls-files '*.cpp' '*.h' | xargs clang-format --style=file --dry-run -Werror
  @echo "[LOG] formatting is clean"

# Run clang-tidy over the whole tree using the debug compile database.
tidy:
  cmake --preset debug
  run-clang-tidy -p build/debug -quiet '^.*/(boot|bsp|core|drivers|lib|tests)/.*\.cpp$'

# Run clang-tidy only over lines changed against main.
tidy-diff BASE="origin/main":
  #!/usr/bin/env bash
  set -euo pipefail
  diff_py=$(ls /opt/homebrew/opt/llvm/share/clang/clang-tidy-diff.py \
               /usr/lib/llvm-*/share/clang/clang-tidy-diff.py 2>/dev/null | head -1)
  test -n "$diff_py" || { echo "[ERR] clang-tidy-diff.py not found" >&2; exit 1; }
  git diff -U0 "{{ BASE }}"...HEAD | python3 "$diff_py" -p1 -path build/debug -quiet

# Write the full format + tidy violation report to build/lint/.
lint-report:
  scripts/lint-report.sh
