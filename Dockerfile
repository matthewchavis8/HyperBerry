FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    cpio \
    curl \
    device-tree-compiler \
    git \
    g++-aarch64-linux-gnu \
    gnupg \
    ipxe-qemu \
    just \
    lsb-release \
    make \
    ninja-build \
    python3 \
    qemu-system-arm \
    qemu-user \
    software-properties-common \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN curl -L --fail --retry 3 https://apt.llvm.org/llvm.sh -o /tmp/llvm.sh \
    && chmod +x /tmp/llvm.sh \
    && /tmp/llvm.sh 22 all \
    && rm /tmp/llvm.sh \
    && apt-get update \
    && apt-get install -y --no-install-recommends clang-22 lld-22 llvm-22 \
    && rm -rf /var/lib/apt/lists/*

RUN curl -L --fail --retry 3 \
    https://github.com/ARM-software/LLVM-embedded-toolchain-for-Arm/releases/download/release-19.1.5/LLVM-ET-Arm-newlib-overlay-19.1.5.tar.xz \
    -o /tmp/newlib.tar.xz \
    && echo 'f900d878a9e149d476cdd99f61b9de72e964a5e767e1de3fcd4fb3c59deaa94c  /tmp/newlib.tar.xz' | sha256sum -c - \
    && mkdir -p /opt/hyperberry \
    && tar -C /opt/hyperberry -xf /tmp/newlib.tar.xz \
    && rm /tmp/newlib.tar.xz

ENV HB_LLVM_SYSROOT=/opt/hyperberry/lib/clang-runtimes/newlib/aarch64-none-elf/aarch64a
ENV CC=clang-22
ENV CXX=clang++-22
