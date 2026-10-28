#!/usr/bin/env python3
"""Apply the release changes to a 4Story game data folder (C8).

Usage: python3 tools/release/patch_game_data.py GAME_FOLDER [--backup]

Changes (each is skipped when already applied):
  - Tcd/TCharTitle.tcd: title "Araz > Life" -> "4Story > Life". Strings are
    MFC CArchive strings (one length byte, then the text), so the length
    byte is rewritten together with the text.
  - Intro/En_open_loading_co.tga: the 4Story logo (tools/release/intro_logo.swift).
--backup keeps the first original as <file>.orig.
"""
import shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TITLE_OLD = b'Araz > Life'
TITLE_NEW = b'4Story > Life'


def backup(path, enabled):
    original = path.with_name(path.name + '.orig')
    if enabled and not original.exists():
        shutil.copy2(path, original)


def patch_titles(game, keep):
    path = game / 'Tcd/TCharTitle.tcd'
    data = path.read_bytes()
    old = bytes([len(TITLE_OLD)]) + TITLE_OLD
    new = bytes([len(TITLE_NEW)]) + TITLE_NEW
    count = data.count(old)
    if count == 0:
        if new in data:
            print('TCharTitle.tcd: already patched')
            return
        sys.exit('TCharTitle.tcd: the "Araz > Life" title was not found; the release must not ship it unchecked')
    backup(path, keep)
    path.write_bytes(data.replace(old, new))
    print(f'TCharTitle.tcd: renamed {count} strings')


def patch_logo(game, keep):
    path = game / 'Intro/En_open_loading_co.tga'
    backup(path, keep)
    subprocess.run(['swift', str(ROOT / 'tools/release/intro_logo.swift'),
                    str(ROOT / 'native/client/app/assets/intro/4story_logo.png'), str(path)], check=True)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    game = Path(sys.argv[1])
    keep = '--backup' in sys.argv[2:]
    patch_titles(game, keep)
    patch_logo(game, keep)


if __name__ == '__main__':
    main()
