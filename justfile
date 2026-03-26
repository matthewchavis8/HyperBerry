# Quick Commands

QEMU_PATH := "build/qemu"
RPI_PATH  := "build/rpi5"

default:
  @just --list

qemu:
  cmake -B {{ QEMU_PATH }} -DBOARD=qemu
  @echo "[LOG] build directory created at {{QEMU_PATH}}"

  cmake --build {{ QEMU_PATH }}
  @echo "[LOG] HyperBerry Image has been created"

  @echo "[LOG] Virtual Raspberry PI5 has succesfully been built and flash"
  cmake --build {{ QEMU_PATH }} --target run

rpi5 SD_DEV="/dev/sda1":
  cmake -B {{ RPI_PATH }} -DBOARD=rpi5 -DSD_MOUNT=/mnt/sdcard
  @echo "[LOG] build directory created at {{QEMU_PATH}}"

  cmake --build {{ RPI_PATH }}
  @echo "[LOG] HyperBerry Image has been created"

  sudo mkdir -p /mnt/sdcard
  sudo mount -o uid=$(id -u),gid=$(id -g) {{ SD_DEV }} /mnt/sdcard
  @echo "[LOG] Mounting SD Card for flashing"

  cmake --build {{ RPI_PATH }} --target run
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

clean:
  rm -rf {{ QEMU_PATH }}
  rm -rf {{ RPI_PATH }}
