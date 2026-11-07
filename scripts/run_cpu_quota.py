"""Run the canonical emulator launcher in an optional user CPU-quota scope.

Linux/systemd only. No boost, governor or persistent service changes.
CPU percentages cover the aggregate time of all workers and children.
Copyright 2026 Brandon Gates; MIT.
"""
import argparse
from decimal import Decimal, InvalidOperation
import os
from pathlib import Path
import re
import sys
import uuid


def quota(value):
    if value == "max":
        return value
    try:
        n = Decimal(value.removesuffix("%"))
        if not n.is_finite() or n <= 0:
            raise ValueError
    except (InvalidOperation, ValueError):
        raise argparse.ArgumentTypeError("quota must be positive CPU percent, or max")
    return format(n, "f") + "%"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--quota", type=quota, required=True)
    parser.add_argument("--period-ms", type=int, default=10)
    parser.add_argument("--unit", default="flexe-quota-" + uuid.uuid4().hex)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("CPU quota requires Linux and a systemd user manager")
    if not 1 <= args.period_ms <= 1000:
        parser.error("period must be between 1 and 1000 milliseconds")
    if not re.fullmatch(r"flexe-[a-zA-Z0-9_-]+", args.unit):
        parser.error("unit must start with flexe- and contain letters, digits, _ or -")
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command or Path(command[0]).name not in {"fab-agon-emulator", "flexe-agon-emulator"}:
        parser.error("invoke the profile-local ./flexe-agon-emulator or ./fab-agon-emulator launcher after --")
    path = Path(command[0])
    try:
        launcher = path.resolve(strict=True)
    except OSError as error:
        parser.error(str(error))
    if launcher.parent != Path.cwd() or path.is_symlink():
        parser.error("run from the canonical launcher directory")
    marker = Path.cwd() / ".bespoke-vdp-profile"
    if marker.is_file() and "flexe" in marker.read_text().lower() and launcher.name != "flexe-agon-emulator":
        parser.error("Flexe profiles require ./flexe-agon-emulator; Fab aliases are forbidden")
    call = ["systemd-run", "--user", "--scope", "--quiet", "--collect",
            "--unit=" + args.unit, "--property=CPUWeight=100"]
    # Explicit default weight activates the CPU controller in BOTH arms.
    # Otherwise an unlimited scope may lack cpu.max until the first quota run.
    if args.quota != "max":
        call += ["--property=CPUQuota=" + args.quota,
                 "--property=CPUQuotaPeriodSec=" + str(args.period_ms) + "ms"]
    # exec preserves systemd-run's exit status. The scope owns worker coverage
    # and cleanup; a caller cancelling a run must stop this named scope.
    os.execvp(call[0], call + ["--", str(launcher), *command[1:]])


if __name__ == "__main__":
    main()
