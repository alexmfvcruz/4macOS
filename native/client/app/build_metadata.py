"""Build channel, version and UTC stamp for the native client (Q-C8B1).

build.py embeds the values into compat/src/build_info.cpp with -D flags; the
in-game label reads them as <channel><version>-<sha256 prefix>.<utc>. The same
text is kept in the executable as one marker string, so release packaging can
read back what an existing binary was built with (read_stamp).
"""
import re, sys
from datetime import datetime, timezone
from pathlib import Path

CHANNELS = ('beta', 'dev', 'stable')
VERSION = re.compile(r'[0-9]+(\.[0-9]+)*')
UTC = re.compile(r'[0-9]{8}T[0-9]{6}Z')
# Marker bytes in the executable: FOURSTORY_BUILD_STAMP=<channel>|<version>|<utc>
MARKER = re.compile(rb'FOURSTORY_BUILD_STAMP=(beta|dev|stable)\|([0-9]+(?:\.[0-9]+)*)\|([0-9]{8}T[0-9]{6}Z)\x00')


def check(channel, version):
    if channel not in CHANNELS:
        raise ValueError(f'channel must be one of {", ".join(CHANNELS)}, not {channel!r}')
    if not VERSION.fullmatch(version or ''):
        raise ValueError(f'version must be numbers separated by periods (example 0.1), not {version!r}')


def ask(channel=None, version=None, stdin=None, prompt=input):
    """Return (channel, version). Missing values are asked on a terminal and
    refused without one; nothing is inferred from branch, path or bundle."""
    stdin = stdin or sys.stdin
    if channel is None or version is None:
        if not stdin.isatty():
            raise ValueError('--channel and --version are required when no terminal can answer the prompt')
        while channel is None or channel not in CHANNELS:
            channel = prompt('Build channel (beta, dev, stable): ').strip().lower()
        while version is None or not VERSION.fullmatch(version):
            version = prompt('Version number (example 0.1): ').strip()
    check(channel, version)
    return channel, version


def utc_now():
    """Build time, recorded once before compiling."""
    return datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')


def defines(channel, version, utc):
    check(channel, version)
    if not UTC.fullmatch(utc):
        raise ValueError(f'bad UTC stamp {utc!r}')
    return [f'-DFOURSTORY_BUILD_CHANNEL="{channel}"', f'-DFOURSTORY_BUILD_VERSION="{version}"',
            f'-DFOURSTORY_BUILD_UTC="{utc}"']


def read_stamp(executable):
    """(channel, version, utc) embedded in a built executable, or None."""
    found = set(MARKER.findall(Path(executable).read_bytes()))
    if len(found) != 1:
        return None
    return tuple(part.decode() for part in found.pop())
