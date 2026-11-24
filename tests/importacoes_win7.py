"""Confere o diretório youtube de um kit extraído, inclusive o Python no yt-dlp.

Para o PC de desenvolvimento: python -m pip install pefile pyinstaller
Uso: python tests/importacoes_win7.py caminho/para/youtube
"""
from pathlib import Path
import sys

import pefile
from PyInstaller.archive.readers import CArchiveReader


def check_image(name, data):
    issues = []
    with pefile.PE(data=data, fast_load=True) as image:
        if image.FILE_HEADER.Machine != 0x14C:
            issues.append(f"{name}: arquitetura diferente de x86")
        image.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]]
        )
        for library in getattr(image, "DIRECTORY_ENTRY_IMPORT", []):
            if library.dll.lower() != b"kernel32.dll":
                continue
            for symbol in library.imports:
                if symbol.name in {b"AddDllDirectory", b"RemoveDllDirectory", b"SetDefaultDllDirectories"}:
                    issues.append(f"{name}: KERNEL32!{symbol.name.decode('ascii')}")
    return issues


def main(directory):
    suffixes = {".exe", ".dll", ".pyd"}
    issues = []
    checked = 0
    for path in directory.rglob("*"):
        if path.is_file() and path.suffix.lower() in suffixes:
            issues.extend(check_image(path.relative_to(directory), path.read_bytes()))
            checked += 1
    archive = CArchiveReader(str(directory / "yt-dlp.exe"))
    for name in archive.toc:
        if Path(name).suffix.lower() in suffixes:
            issues.extend(check_image(f"yt-dlp.exe/{name}", archive.extract(name)))
            checked += 1
    print(f"Imagens PE x86 verificadas: {checked}. Problemas: {len(issues)}")
    for issue in issues:
        print(issue)
    return int(bool(issues))


if __name__ == "__main__":
    sys.exit(main(Path(sys.argv[1])))
