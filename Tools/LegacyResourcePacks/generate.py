"""Build the legacy block resource packs from a local Java 1.12.2 client JAR."""

import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import tempfile
from typing import NamedTuple
from urllib.parse import urlsplit
from zipfile import BadZipFile, ZIP_DEFLATED, ZipFile, ZipInfo


MAPPINGS = Path(__file__).resolve().parents[2] / "src/Protocol/LegacyPackBlocks.inc"
ASSETS = "assets/minecraft"
PACK_FORMATS = {47: 1, 107: 2, 108: 2, 109: 2, 110: 2}
MAPPING_ROW = re.compile(
    r"LEGACY_PACK_BLOCK\(\w+, (\d+), \d+, (\w+), (\w+), (\w+), \w+, \w+\)"
)


class PackBlock(NamedTuple):
    first_native_protocol: int
    carrier_blockstate: str
    vanilla_model: str
    texture: str


def read_mappings():
    mappings = []
    for line in MAPPINGS.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("//"):
            continue
        match = MAPPING_ROW.fullmatch(line)
        if not match:
            raise ValueError(f"Invalid legacy pack mapping: {line}")
        protocol, blockstate, vanilla_model, texture = match.groups()
        mappings.append(PackBlock(int(protocol), blockstate, vanilla_model, texture))
    if not mappings:
        raise ValueError("No legacy pack mappings found")
    return mappings


def build_pack(client, pack_format, mappings):
    files = {
        "pack.mcmeta": json.dumps({
            "pack": {"pack_format": pack_format, "description": "Cuberite legacy blocks"}
        })
    }
    for block in mappings:
        model = f"cuberite_legacy/{block.texture}"
        files[f"{ASSETS}/blockstates/{block.carrier_blockstate}.json"] = json.dumps({
            "variants": {
                "normal": {"model": block.vanilla_model},
                "all": {"model": model},
            }
        })
        files[f"{ASSETS}/models/block/{model}.json"] = json.dumps({
            "parent": "block/cube_all",
            "textures": {"all": f"blocks/{model}"},
        })
        texture = f"{ASSETS}/textures/blocks/{block.texture}.png"
        target = f"{ASSETS}/textures/blocks/{model}.png"
        files[target] = client.read(texture)
        if f"{texture}.mcmeta" in client.NameToInfo:
            files[f"{target}.mcmeta"] = client.read(f"{texture}.mcmeta")

    data = io.BytesIO()
    with ZipFile(data, "w") as pack:
        for name, content in sorted(files.items()):
            # A fixed timestamp keeps the ZIP and its download checksum stable across runs.
            pack.writestr(ZipInfo(name), content, compress_type=ZIP_DEFLATED)
    return data.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client_jar", type=Path, help="path to the installed 1.12.2.jar")
    parser.add_argument("--output", type=Path, required=True, help="directory served by your HTTP server")
    parser.add_argument("--base-url", required=True, help="public HTTP(S) URL of the output directory")
    args = parser.parse_args()

    try:
        url = urlsplit(args.base_url)
        if (
            url.scheme not in ("http", "https") or not url.hostname or url.username
            or url.query or url.fragment or any(char.isspace() for char in args.base_url)
        ):
            raise ValueError("--base-url must be an HTTP(S) directory URL without credentials, query or fragment")

        mappings = read_mappings()
        packs = {}
        offers = []
        with ZipFile(args.client_jar) as client:
            for protocol, pack_format in PACK_FORMATS.items():
                blocks = tuple(block for block in mappings if protocol < block.first_native_protocol)
                if not blocks:
                    continue
                key = pack_format, blocks
                if key not in packs:
                    data = build_pack(client, pack_format, blocks)
                    digest = hashlib.sha1(data).hexdigest()
                    packs[key] = data, digest
                _, digest = packs[key]
                offers.append(f"{protocol}={args.base_url.rstrip('/')}/cuberite-legacy-{digest}.zip {digest}")

        args.output.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=args.output) as staging:
            for data, digest in packs.values():
                filename = f"cuberite-legacy-{digest}.zip"
                staged = Path(staging) / filename
                staged.write_bytes(data)
                staged.replace(args.output / filename)
    except (OSError, BadZipFile, KeyError, ValueError) as error:
        parser.exit(1, f"Could not generate legacy resource packs: {error}\n")

    print("[ResourcePacks]")
    print("\n".join(offers))


if __name__ == "__main__":
    main()
