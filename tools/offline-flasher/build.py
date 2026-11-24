#!/usr/bin/env python3
"""Embed the pinned browser tools and released CYD images into one HTML file."""

import argparse
import base64
import hashlib
import html
import json
import struct
import tarfile
from pathlib import Path


HERE = Path(__file__).resolve().parent
FIRMWARE = HERE.parents[1] / "firmware"
PACKAGES = {
    "esptool-js-0.7.1": ("2fec38259eaeb65aacfac87806625c2da6d2296da59c6dfaee7debba9f63f6b9", "LICENSE"),
    "pako-2.1.0": ("49fedc8866b4abfc8e71dc7fe75ad4ef1ff1ac9601b0642cff88ee5bf2338709", "LICENSE"),
    "atob-lite-2.0.0": ("c4b3fb3c6d9bf906c7dce92c6306951a01ebe09ded5a871bf2e0b326443abc5a", "LICENSE.md"),
    "spark-md5-3.0.2": ("a9934737094fc9479f73e9fbe651b1f93da7d1af8f0f126dee028d62b92ae360", "LICENSE"),
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_package(folder, name, member):
    path = folder / f"{name}.tgz"
    if digest(path.read_bytes()) != PACKAGES[name][0]:
        raise ValueError(f"Unexpected checksum for {path.name}")
    with tarfile.open(path) as archive:
        return archive.extractfile(f"package/{member}").read().decode("utf-8")


def read_release(folder, name):
    checksums = dict(reversed(line.split(maxsplit=1)) for line in (folder / "SHA256SUMS.txt").read_text().splitlines())
    data = (folder / name).read_bytes()
    if digest(data) != checksums.get(name):
        raise ValueError(f"Release checksum mismatch for {name}")
    return data


def check_layout(bootloader, partitions, app):
    for image in (bootloader, app):
        if image[0] != 0xE9 or struct.unpack_from("<H", image, 12)[0] != 23 or image[2:4] != b"\x02\x4f":
            raise ValueError("Expected an ESP32-C5 image with DIO, 80 MHz and 16 MB flash in its header")
    if len(bootloader) > 0x6000 or len(partitions) > 0x1000 or len(app) > 0x200000:
        raise ValueError("A release image exceeds its flash region")
    records = [struct.unpack_from("<HBBII", partitions, offset) for offset in (0, 32, 64)]
    if records != [(0x50AA, 1, 2, 0x9000, 0x6000), (0x50AA, 1, 1, 0x11000, 0x1000),
                   (0x50AA, 0, 0, 0x20000, 0x200000)]:
        raise ValueError("Expected the standalone Scout partition map")


def build(packages, app_path, output):
    bootloader = read_release(FIRMWARE, "bootloader/bootloader.bin")
    partitions = read_release(FIRMWARE, "partition_table/partition-table.bin")
    app = read_release(app_path.parent, app_path.name)
    check_layout(bootloader, partitions, app)
    version = app[48:80].split(b"\0", 1)[0].decode("ascii")
    sections = [("Bootloader", 0x2000, bootloader), ("Partition table", 0x8000, partitions),
                ("FPV Scout", 0x20000, app)]
    manifest = {"version": version, "images": [
        {"name": name, "address": address, "size": len(data), "sha256": digest(data),
         "base64": base64.b64encode(data).decode("ascii")}
        for name, address, data in sections
    ]}
    replacements = {
        "VERSION": html.escape(version),
        "SOURCE_TAG": html.escape(f"v{version}"),
        "IMAGES": "\n".join(f"<tr><td>{name}</td><td><code>0x{address:X}</code></td></tr>" for name, address, _ in sections),
        "MANIFEST": json.dumps(manifest, separators=(",", ":")),
        "ESPTOOL": read_package(packages, "esptool-js-0.7.1", "bundle.js"),
        "MD5": read_package(packages, "spark-md5-3.0.2", "spark-md5.min.js"),
        "LICENSES": html.escape("\n\n".join(
            f"{name}\n\n{read_package(packages, name, license_name)}"
            for name, (_, license_name) in PACKAGES.items()
        )),
    }
    page = (HERE / "installer.html.in").read_text(encoding="utf-8")
    for key, value in replacements.items():
        token = f"@@{key}@@"
        if page.count(token) != 1 or "</script" in value.lower():
            raise ValueError(f"Can't safely embed {key}")
        page = page.replace(token, value)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(page, encoding="utf-8", newline="\n")
    print(f"{output}  {digest(output.read_bytes())}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("packages", type=Path, help="directory containing the pinned npm tarballs")
    parser.add_argument("output", type=Path, help="output HTML file")
    parser.add_argument("--app", type=Path, default=FIRMWARE / "fpv_scout.bin",
                        help="path to a released app with a SHA256SUMS.txt in the same folder")
    args = parser.parse_args()
    build(args.packages, args.app, args.output)
