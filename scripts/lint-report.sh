#!/usr/bin/env bash
#
# Generates the style-adoption report in build/lint/:
#
#   format.diff  full clang-format diff, unapplied
#   tidy.txt     raw clang-tidy output across the tree
#   SUMMARY.md   the readable rollup
#
# Nothing under version control is modified.
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

OUT="build/lint"
DB="build/debug"
SRC_DIRS=(boot bsp core drivers lib tests)

mkdir -p "$OUT"

if [ ! -f "$DB/compile_commands.json" ]; then
  echo "[LOG] $DB/compile_commands.json missing, configuring the debug preset"
  cmake --preset debug >/dev/null || {
    echo "[ERR] cmake configure failed; cannot run clang-tidy" >&2
    exit 1
  }
fi

FILES=(); while IFS= read -r line; do FILES+=("$line"); done < <(git ls-files "*.cpp" "*.h")
echo "[LOG] ${#FILES[@]} source files"

echo "[LOG] collecting clang-format diff"
: > "$OUT/format.diff"
: > "$OUT/format-churn.txt"
for f in "${FILES[@]}"; do
  d=$(clang-format --style=file "$f" | diff -u --label "a/$f" --label "b/$f" "$f" -)
  if [ -n "$d" ]; then
    printf '%s\n' "$d" >> "$OUT/format.diff"
    printf '%s\t%s\n' "$(printf '%s\n' "$d" | grep -c '^[+-][^+-]')" "$f" >> "$OUT/format-churn.txt"
  fi
done
sort -rn -o "$OUT/format-churn.txt" "$OUT/format-churn.txt"

echo "[LOG] running clang-tidy (this takes a minute)"
run-clang-tidy -p "$DB" -quiet "$(IFS='|'; echo "^$ROOT/(${SRC_DIRS[*]})/.*\.(cpp)$")" \
  > "$OUT/tidy.txt" 2>/dev/null

echo "[LOG] building SUMMARY.md"
python3 scripts/lint_summary.py "$OUT" "$ROOT"

echo "[LOG] report written to $OUT/SUMMARY.md"
