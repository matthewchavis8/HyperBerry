#!/usr/bin/env bash

set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
image=hyperberry-toolchain:local
tty=

if [[ "${1:-}" == "--tty" ]]; then
  tty=-t
  shift
fi

if [[ $# -eq 0 ]]; then
  echo "usage: $0 [--tty] command [args...]" >&2
  exit 2
fi

if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then
  exec "$@"
fi

for preset in debug release unit-tests; do
  build="$root/build/$preset"
  if [[ -d "$build" ]] && find "$build" -type f -name CMakeCache.txt \
      -exec grep -EL '^CMAKE_HOME_DIRECTORY:INTERNAL=/workspace(/|$)' {} + | grep -q .; then
    find "$build" -type f -name CMakeCache.txt -delete
    find "$build" -type d -name CMakeFiles -prune -exec rm -rf {} +
  fi
done

docker build --tag "$image" --file "$root/Dockerfile" "$root"

exec docker run --rm -i ${tty:+"$tty"} \
  --user "$(id -u):$(id -g)" \
  --env HOME=/tmp \
  --volume "$root:/workspace" \
  --workdir /workspace \
  "$image" "$@"
