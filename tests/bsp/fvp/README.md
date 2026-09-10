# tests/bsp/fvp

Board-specific integration test sources for `fvp`.

A file here whose basename matches one in `tests/integration/` replaces it for
this board only; any other file is added to the `fvp` test image. Used when a
suite needs a genuinely different implementation rather than a different
address — a different address should be passed in from the host side instead.
