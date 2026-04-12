FROM ubuntu:24.04@sha256:e21f810fa78c09944446ec02048605eb3ab1e4e2e261c387ecc7456b38400d79

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    libc6-dev-arm64-cross \
    clang \
    lld \
    llvm \
    qemu-system-arm \
    qemu-user \
    ipxe-qemu \
    make \
    cmake \
    ninja-build \
    python3 \
    git \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

RUN curl -LsSf https://github.com/casey/just/releases/download/1.36.0/just-1.36.0-x86_64-unknown-linux-musl.tar.gz -o /tmp/just.tar.gz \
    && echo "bc7c9f377944f8de9cd0418b11d2955adebfa25a488c0b5e3dd2d2c0e9d732da  /tmp/just.tar.gz" | sha256sum -c \
    && tar -xz -C /usr/local/bin just -f /tmp/just.tar.gz \
    && rm /tmp/just.tar.gz
