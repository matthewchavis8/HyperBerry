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

rpi5:
  cmake -B {{ RPI_PATH }} -DBOARD=rpi5
  @echo "[LOG] build directory created at {{QEMU_PATH}}"
  cmake --build {{ RPI_PATH }}
  @echo "[LOG] HyperBerry Image has been created"
  cmake --build {{ RPI_PATH }} --target run
  @echo "[LOG] Physical Raspberry PI5 has succesfully been built and flash"

clean:
  rm -rf {{ QEMU_PATH }}
  rm -rf {{ RPI_PATH }}
