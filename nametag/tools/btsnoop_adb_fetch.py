#!/usr/bin/env python3

import argparse
import shutil
import subprocess
import sys
import tempfile
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("save_file", help="Log to write")
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix="bugreport.") as temp_dir:
    zip_basename = f"{temp_dir}/bugreport"
    print(f"=== adb bugreport {zip_basename} ===")
    subprocess.run(["adb", "bugreport", zip_basename], check=True)
    print()

    zip_path = f"{zip_basename}.zip"
    print(f"=== opening {zip_path} ===")
    with zipfile.ZipFile(zip_path) as zip_file:
        for zip_info in zip_file.infolist():
            if zip_info.filename.endswith("/btsnoop_hci.log"):
                print(f"Unzip: {zip_info.filename} => {args.save_file}")
                with zip_file.open(zip_info.filename, "r") as zip_log_file:
                    with open(args.save_file, "xb") as save_log_file:
                        shutil.copyfileobj(zip_log_file, save_log_file)
                print()
                break
        else:
            print()
            raise FileNotFoundError(f"No 'btsnoop_hci.log' in {zip_path}")
