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
  cmake --build --preset debug --target run-{{ BOARD }}-test

# Point clangd at the build compile database.
compile-db:
  cmake --preset debug
  ln -sf build/debug/compile_commands.json compile_commands.json
  @echo "[LOG] compile_commands.json linked"

test-unit:
  cmake --preset unit-tests
  cmake --build --preset unit-tests
  ctest --preset unit-tests

cpio ROOT OUT:
  python3 tools/cpio/archive.py --root "{{ ROOT }}" --out "{{ OUT }}"

clean:
  rm -rf build/
