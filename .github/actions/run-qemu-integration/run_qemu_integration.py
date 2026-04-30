#!/usr/bin/env python3

import argparse
import subprocess
import sys
import time
from pathlib import Path


PASS_MARKER = "TESTS PASSED"
FAIL_MARKER = "TESTS FAILED"


def terminate(proc: subprocess.Popen[str]) -> None:
    if proc.poll() is not None:
        return

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run HyperBerry integration tests in QEMU and stop on verdict."
    )
    parser.add_argument("--kernel", required=True, help="Path to kernel8.img")
    parser.add_argument(
        "--initrd",
        default="",
        help="Optional initrd/guest boot package to pass to QEMU",
    )
    parser.add_argument("--log", required=True, help="Path to the QEMU log file")
    parser.add_argument(
        "--timeout",
        type=int,
        default=120,
        help="Seconds to wait for a PASS/FAIL verdict before failing",
    )
    args = parser.parse_args()

    log_path = Path(args.log)
    log_path.parent.mkdir(parents=True, exist_ok=True)

    command = [
        "qemu-system-aarch64",
        "-machine",
        "virt,virtualization=on,gic-version=2",
        "-cpu",
        "cortex-a76",
        "-m",
        "4G",
        "-nographic",
        "-kernel",
        args.kernel,
    ]
    if args.initrd:
        command.extend(["-initrd", args.initrd])

    start = time.monotonic()
    verdict = None

    with log_path.open("w", encoding="utf-8") as log_file:
        proc = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        assert proc.stdout is not None

        try:
            for line in proc.stdout:
                sys.stdout.write(line)
                sys.stdout.flush()
                log_file.write(line)
                log_file.flush()

                if PASS_MARKER in line:
                    verdict = True
                    terminate(proc)
                    break

                if FAIL_MARKER in line:
                    verdict = False
                    terminate(proc)
                    break

                if time.monotonic() - start > args.timeout:
                    log_file.write(
                        f"\n[ERROR] Timed out after {args.timeout}s waiting for test verdict.\n"
                    )
                    log_file.flush()
                    print(
                        f"[ERROR] Timed out after {args.timeout}s waiting for test verdict.",
                        file=sys.stderr,
                    )
                    terminate(proc)
                    return 124
        finally:
            proc.stdout.close()

        return_code = proc.wait()

    if verdict is True:
        return 0

    if verdict is False:
        return 1

    if return_code != 0:
        print(
            f"[ERROR] QEMU exited before a test verdict was emitted (code {return_code}).",
            file=sys.stderr,
        )
        return return_code

    print("[ERROR] QEMU exited without emitting TESTS PASSED or TESTS FAILED.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
