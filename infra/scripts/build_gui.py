#!/usr/bin/env python3
from __future__ import annotations

from infra.scripts.kinal_gui import launch_gui


def main() -> int:
    launch_gui()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())