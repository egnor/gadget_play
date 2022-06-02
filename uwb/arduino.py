#!/usr/bin/env python3

import argparse
import os
import platform
import serial.tools.list_ports
import serial.tools.miniterm
import signal
import sys
import time
from pathlib import Path
from subprocess import run, PIPE

signal.signal(signal.SIGINT, signal.SIG_DFL)  # sane ^C behavior
source_dir = Path(__file__).resolve().parent
default_sketch_dir = source_dir / "uwbtest"
tool_dir = source_dir.parent / "external" / "arduino"
cli_bin = tool_dir / f"arduino-cli-{platform.system()}-{platform.machine()}"

build_dir = source_dir / "build"
os.environ["ARDUINO_DIRECTORIES_DATA"] = str(build_dir / "data")
os.environ["ARDUINO_DIRECTORIES_DOWNLOADS"] = str(build_dir / "downloads")
os.environ["ARDUINO_DIRECTORIES_USER"] = str(build_dir / "user")
os.environ["ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS"] = "https://adafruit.github.io/arduino-board-index/package_adafruit_index.json"

parser = argparse.ArgumentParser()
parser.add_argument("--baud", default=115200)
parser.add_argument("--fqbn", default="adafruit:samd:adafruit_metro_m4")
parser.add_argument("--port", default="Metro M4")

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

port = Path("/dev", args.port)
if not port.is_char_device():
    matching = list(serial.tools.list_ports.grep(args.port))
    if len(matching) != 1:
        print(f'{len(matching)} ports match "{args.port}"')
        print("See: python -m serial.tools.list_ports -v")
        sys.exit(1)
    port = Path(matching[0].device)

if args.setup:
    board_prefix = ":".join(args.fqbn.split(":")[:2])
    run([cli_bin, "update"], check=True)
    run([cli_bin, "upgrade"], check=True)
    run([cli_bin, "core", "install", board_prefix] + args.extra, check=True)

if args.build or args.upload:
    command = [
        cli_bin,
        "compile",
        f"--build-cache-path={build_dir / 'build_cache'}",
        f"--build-path={build_dir / 'build'}",
        f"--build-property=build.flags.optimize=-O3",
        f"--fqbn={args.fqbn}",
        f"--output-dir={build_dir / 'build_output'}",
        "--warnings=all",
    ]
    if args.upload:
        print(f"Building and uploading ({port})...")
        command.append(f"--port={port}")
        command.append("--upload")
    else:
        print("Building...")
    run(command + (args.extra or [default_sketch_dir]), check=True)

if args.terminal:
    while True:
        time.sleep(0.5)
        if port.is_char_device() and os.access(port, os.W_OK): break
        print(f"Waiting for port ({port})...")
    sys.argv = [""]
    serial.tools.miniterm.main(
        default_port=str(port),
        default_baudrate=args.baud
    )

print("=== Done! ===\n")
