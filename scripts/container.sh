#!/usr/bin/env bash

set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
image=hyperberry-toolchain:local
tty=()

if [[ "${1:-}" == "--tty" ]]; then
  tty=(-t)
  shift
fi

if [[ $# -eq 0 ]]; then
  echo "usage: $0 [--tty] command [args...]" >&2
  exit 2
fi

for preset in debug release unit-tests; do
  cache="$root/build/$preset/CMakeCache.txt"
  if [[ -f "$cache" ]] && ! grep -qx 'CMAKE_HOME_DIRECTORY:INTERNAL=/workspace' "$cache"; then
    rm -rf "$root/build/$preset/CMakeCache.txt" "$root/build/$preset/CMakeFiles"
  fi
done

docker build --tag "$image" --file "$root/Dockerfile" "$root"

exec docker run --rm -i "${tty[@]}" \
  --user "$(id -u):$(id -g)" \
  --env HOME=/tmp \
  --volume "$root:/workspace" \
  --workdir /workspace \
  "$image" "$@"
