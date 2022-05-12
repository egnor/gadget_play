#!/usr/bin/env python3

import argparse
import os
import platform
import serial.tools.miniterm
import sys
from pathlib import Path
from subprocess import run, PIPE

source_dir = Path(__file__).resolve().parent
default_sketch_dir = source_dir / "uwbtalk"
tool_dir = source_dir.parent / "external" / "arduino"
cli_bin = tool_dir / f"arduino-cli-{platform.system()}-{platform.machine()}"

work_dir = source_dir / "work"
os.environ["ARDUINO_DIRECTORIES_DATA"] = str(work_dir / "data")
os.environ["ARDUINO_DIRECTORIES_DOWNLOADS"] = str(work_dir / "downloads")
os.environ["ARDUINO_DIRECTORIES_USER"] = str(work_dir / "user")

parser = argparse.ArgumentParser()
parser.add_argument("--baud", default=115200)
parser.add_argument("--fqbn", default="arduino:avr:uno")
parser.add_argument("--port")

parser.add_argument("--build", action="store_true")
parser.add_argument("--setup", action="store_true")
parser.add_argument("--terminal", action="store_true")
parser.add_argument("--upload", action="store_true")

parser.add_argument("extra", nargs="*")

args = parser.parse_args()
if not (args.build or args.setup or args.terminal or args.upload):
    if not args.extra:
        parser.print_help()
        sys.exit(1)
    run([cli_bin] + args.extra)

if args.setup:
    run([cli_bin, "update"], check=True)
    run([cli_bin, "upgrade"], check=True)
    run([cli_bin, "core", "install", "arduino:avr"] + args.extra, check=True)

if args.build or args.terminal or args.upload:
    port = args.port
    if not port:
        print("Scanning ports...")
        command = [cli_bin, "board", "list"]
        output = run(command, stdout=PIPE, check=True).stdout.decode()
        match = [l.strip() for l in output.split("\n") if args.fqbn in l]
        if len(match) != 1:
            print(f"*** No unique board:\n{output.strip()}\n")
            sys.exit(1)
        port = match[0].split()[0]

if args.build or args.upload:
    print(f"Building{' and uploading' if args.upload else ''} (port={port})...")
    command = [
        cli_bin,
        "compile",
        f"--build-cache-path={work_dir / 'build_cache'}",
        f"--build-path={work_dir / 'build'}",
        f"--fqbn={args.fqbn}",
        f"--output-dir={work_dir / 'build_output'}",
        f"--port={port}",
        f"--warnings=all",
    ]
    if args.upload:
        command.append("--upload")
    run(command + (args.extra or [default_sketch_dir]), check=True)

if args.terminal:
    sys.argv = ['']
    serial.tools.miniterm.main(default_port=port, default_baudrate=args.baud)

print("=== Done! ===\n")