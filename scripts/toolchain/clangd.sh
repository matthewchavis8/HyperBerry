#!/usr/bin/env bash

set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
image=hyperberry-toolchain:local

if ! docker image inspect "$image" >/dev/null 2>&1; then
  echo "hyperberry-toolchain:local is missing; run: just build" >&2
  exit 1
fi

exec docker run --rm -i \
  --user "$(id -u):$(id -g)" \
  --env HOME=/tmp \
  --volume "$root:/workspace" \
  --workdir /workspace \
  "$image" clangd-22 "$@"
