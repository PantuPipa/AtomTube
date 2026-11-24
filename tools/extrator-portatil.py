"""Build an onedir layout from the pinned Vista x86 PyInstaller distribution."""
from pathlib import Path
import struct
import sys

from PyInstaller.archive.readers import CArchiveReader


def unpack(source, destination):
    archive = CArchiveReader(str(source))
    original = source.read_bytes()
    base = archive._start_offset
    cookie = struct.Struct(archive._COOKIE_FORMAT)
    magic, _, toc_offset, toc_length, version, python = cookie.unpack_from(original, archive._end_offset - cookie.size)
    entry = struct.Struct(archive._TOC_ENTRY_FORMAT)
    index = base + toc_offset
    end = index + toc_length
    body, toc = bytearray(), bytearray()
    internal = destination / "_internal"
    while index < end:
        size, offset, length, raw, compressed, kind = entry.unpack_from(original, index)
        name_bytes = original[index + entry.size:index + size]
        name = name_bytes.rstrip(b"\0").decode("utf-8")
        index += size
        if kind == b"b":
            path = internal / name
            if not path.resolve().is_relative_to(internal.resolve()):
                raise ValueError(name)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(archive.extract(name))
        elif kind in {b"o", b"m", b"s", b"z"}:
            toc.extend(entry.pack(size, len(body), length, raw, compressed, kind) + name_bytes)
            body.extend(original[base + offset:base + offset + length])
        else:
            raise ValueError(f"Unsupported entry {kind!r}")
    footer = cookie.pack(magic, len(body) + len(toc) + cookie.size, len(body), len(toc), version, python)
    (destination / "yt-dlp.exe").write_bytes(original[:base] + body + toc + footer)


if __name__ == "__main__":
    unpack(Path(sys.argv[1]), Path(sys.argv[2]))

