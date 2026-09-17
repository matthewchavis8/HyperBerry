# Quick Commands

default:
  @just --list

# Build every board's image (one configure, one build).
build MODE="debug":
  scripts/container.sh cmake --preset {{ MODE }}
  scripts/container.sh cmake --build --preset {{ MODE }}
  @echo "[LOG] images under build/{{ MODE }}/<board>/kernel8.img"

qemu MODE="debug":
  scripts/container.sh --tty cmake --preset {{ MODE }}
  scripts/container.sh --tty cmake --build --preset {{ MODE }} --target run-qemu

rpi5 MODE="release" SD_DEV="/dev/sdd1":
  scripts/container.sh cmake --preset {{ MODE }}
  scripts/container.sh cmake --build --preset {{ MODE }} --target hyperberry-rpi5
  scripts/flash-rpi5.sh build/{{ MODE }}/rpi5/kernel8.img build/{{ MODE }}/rpi5/guest.cpio {{ SD_DEV }}
  @echo "[LOG] Physical Raspberry Pi 5 has been built and flashed"

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
  scripts/container.sh cmake --preset debug
  scripts/container.sh cmake --build --preset debug --target hyperberry-{{ BOARD }}-test
  scripts/container.sh cmake --build --preset debug --target run-{{ BOARD }}-test

# Point clangd at the build compile database.
compile-db:
  scripts/container.sh cmake --preset debug
  scripts/container.sh ln -sf build/debug/compile_commands.json compile_commands.json
  @echo "[LOG] compile_commands.json linked"

test-unit:
  scripts/container.sh cmake --preset unit-tests
  scripts/container.sh cmake --build --preset unit-tests
  scripts/container.sh ctest --preset unit-tests

cpio ROOT OUT:
  scripts/container.sh python3 tools/cpio/archive.py --root "{{ ROOT }}" --out "{{ OUT }}"

clean:
  rm -rf build/
