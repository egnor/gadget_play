#!/usr/bin/env python3

import json
import zipfile
from pathlib import Path

import PIL.Image  # type: ignore

translate = {
    "亲亲鱼": "kiss_fish",
    "烟花动画": "fireworks",
    "心跳动画": "heart_beat",
    "脚板": "footprints",
    "一箭穿心": "heart_arrow",
    "线条": "line",
    "爆闪灯": "flashing",
    "线条1": "line1",
    "箭头": "arrow",
    "心碎": "heart_broken",
    "吃豆子": "pacman",
}

top_dir = Path(__file__).parent.parent
apk_path = top_dir / "apk" / "base.apk"
print(f"=== Opening {apk_path} ===")
with zipfile.ZipFile(apk_path) as apk_zip:
    with apk_zip.open("assets/data1248.json") as json_file:
        json_data = json.load(json_file)
        json_entries = json_data["data"]
        print("Loaded json: {len(json_entries)} entries")
        for entry in json_entries:
            name = entry["describe"]
            translated = translate[name]
            save_dir = top_dir / "captures" / "app_animations" / translated
            save_dir.mkdir(parents=True, exist_ok=True)

            assert entry["sendDataType"] == 4
            data = entry["sendData"]
            assert all(b == 0 for b in data[:24])
            assert data[24] == len(data[27:]) // 96
            for i, f in enumerate(range(27, len(data), 96)):
                image_data = bytes(data[f : f + 96])
                image = PIL.Image.frombytes("1", (12, 48), image_data)
                image = image.transpose(PIL.Image.TRANSPOSE)
                image.save(save_dir / f"frame.{i:02d}.png")

            print(f"{data[24]:2d} frames: {translated} ({name})")
