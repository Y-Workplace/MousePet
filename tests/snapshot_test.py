#!/usr/bin/env python3
"""
Test runner for MousePet snapshot / headless render test.
Generates snapshot images for each accessory and validates the JSON output.
"""
import argparse
import json
import os
import pathlib
import subprocess
import sys

def main():
    parser = argparse.ArgumentParser(description="Test MousePet snapshot generation")
    parser.add_argument("--binary", required=True, type=pathlib.Path, help="Path to mousepet binary")
    parser.add_argument("--out", required=True, type=pathlib.Path, help="Output directory for snapshot pngs")
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    loader = "/lib64/ld-linux-x86-64.so.2" if os.path.exists("/lib64/ld-linux-x86-64.so.2") else None

    # Get list of accessories
    cmd = [loader, str(args.binary), "list"] if loader else [str(args.binary), "list"]
    res = subprocess.run(cmd, capture_output=True, text=True, check=True)
    accessories = [line.split("\t")[0] for line in res.stdout.strip().split("\n") if line.strip()]

    print(f"Testing snapshots for accessories: {accessories}")

    for acc in accessories:
        png_path = args.out / f"{acc}.png"
        cmd = [loader, str(args.binary)] if loader else [str(args.binary)]
        cmd += ["snapshot", str(png_path), "--accessory", acc, "--steps", "120", "--burst-at", "60", "--motion", "circle"]
        
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"FAIL: Snapshot failed for {acc}: {proc.stderr}")
            sys.exit(1)

        try:
            data = json.loads(proc.stdout.strip())
        except json.JSONDecodeError as e:
            print(f"FAIL: Invalid JSON output for {acc}: {proc.stdout}")
            sys.exit(1)

        assert data["accessory"] == acc
        assert data["state"] in ["Floating", "Following", "Burst"]
        assert data["error"] == ""
        assert png_path.exists() and png_path.stat().st_size > 0
        print(f"OK: {acc} -> {png_path.name} (size: {png_path.stat().st_size} bytes)")

    print("All snapshot tests passed successfully!")

if __name__ == "__main__":
    main()
