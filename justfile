# Quick Commands

default:
  @just --list

qemu MODE="debug":
  cmake --preset {{ MODE }} -DBOARD=qemu
  @echo "[LOG] build {{ MODE }} directory"
  cmake --build --preset {{ MODE }}
  @echo "[LOG] HyperBerry Image has been created"

  @echo "[LOG] Virtual Raspberry PI5 has succesfully been built and flash"
  cmake --build --preset {{ MODE }} --target run

rpi5 MODE="release" SD_DEV="/dev/sdb1":
  cmake --preset {{ MODE }} -DBOARD=rpi5 -DSD_MOUNT=/mnt/sdcard
  @echo "[LOG] build {{ MODE }} directory"

  cmake --build --preset {{ MODE }}
  @echo "[LOG] HyperBerry Image has been created"

  @echo "[LOG] Mounting SD Card for flashing"
  sudo mkdir -p /mnt/sdcard
  sudo mount -o uid=$(id -u),gid=$(id -g) {{ SD_DEV }} /mnt/sdcard
  @echo "[LOG] Succesfully mounted SD Card for flashing"

  cmake --build --preset {{ MODE }} --target run
  sync
  sudo umount /mnt/sdcard
  @echo "[LOG] SD card flashing is done unmounting SD card"
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

test-integration BOARD="qemu" SD_DEV="/dev/sdb1":
  cmake --preset integration-test -DBOARD={{ BOARD }} {{ if BOARD == "rpi5" { "-DSD_MOUNT=/mnt/sdcard" } else { "" } }}
  cmake --build --preset integration-test

  {{ if BOARD == "rpi5" { "sudo mkdir -p /mnt/sdcard" } else { "" } }}
  {{ if BOARD == "rpi5" { "sudo mount -o uid=$(id -u),gid=$(id -g) " + SD_DEV + " /mnt/sdcard" } else { "" } }}
  cmake --build --preset integration-test --target run 2>&1 | tee build/integration-test/qemu.log
  {{ if BOARD == "rpi5" { "sync" } else { "" } }}
  {{ if BOARD == "rpi5" { "sudo umount /mnt/sdcard" } else { "" } }}

test-unit:
  cmake --preset unit-tests
  cmake --build --preset unit-tests
  ctest --preset unit-tests

clean:
  rm -rf build/
