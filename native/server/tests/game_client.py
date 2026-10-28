"""Scripted stand-in for the 4Story client's network layer.

Implements the client side of the original session protocol so tests can drive
Login and Map without the Windows client:

* framing and per-packet keys from TNetLib/Session.cpp and the client's
  Engine Lib/TachyonSession.cpp (sequence number, XOR body, header mask);
* the client-to-server RC4 layer, keyed like native/server/win32/crypto.cpp
  (MD5 of the shared secret). That derivation has not been compared with
  Windows CryptoAPI output, so this proves the native servers agree with this
  script, not with the Windows client.

Protocol constants are read from the original headers and sources at import
time; nothing secret is duplicated here.
"""
import hashlib
import re
import socket
import struct
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'source/4Story 5.0 Source Files'
PROTOCOL = SOURCE / 'Lib/Own/TProtocol/include'
MASK = (1 << 64) - 1
HEADER = struct.Struct('<HHIq')


def _text(path):
    return path.read_bytes().decode('latin1')


def _load_ids():
    base = {m.group(1): int(m.group(2), 16)
            for m in re.finditer(r'#define\s+(CS_LOGIN|CS_MAP)\s+\(0x([0-9A-Fa-f]+)\)', _text(PROTOCOL / 'ProtocolBase.h'))}
    ids = {}
    for m in re.finditer(r'^#define\s+(CS_\w+)\s+\(\s*(CS_LOGIN|CS_MAP)\s*\+\s*0x([0-9A-Fa-f]+)\s*\)',
                         _text(PROTOCOL / 'CSProtocol.h'), re.M):
        ids[m.group(1)] = base[m.group(2)] + int(m.group(3), 16)
    return ids


def _load_session_constants():
    raw = (SOURCE / 'Server/TNetLib/Session.cpp').read_bytes()
    keys = [int(v, 16) for v in re.findall(rb'0x([0-9a-fA-F]{16})', raw.split(b'g_4skey', 1)[1].split(b'};', 1)[0])]
    secret = re.search(rb'g_strSecretKey\s*=\s*"([^"]*)"', raw).group(1)
    return keys, secret + b'\0'  # the original passes GetLength() + 1 bytes


IDS = _load_ids()
NAMES = {value: name for name, value in IDS.items()}
KEYS, SECRET = _load_session_constants()
TVERSION = int(re.search(r'#define\s+TVERSION\s+\(\(WORD\)\s*0x([0-9A-Fa-f]+)\)', _text(PROTOCOL / 'ProtocolBase.h')).group(1), 16)
CHECK_KEY = 0x336c3aebf71a8b08


def _signed(value):
    value &= MASK
    return value - (1 << 64) if value >> 63 else value


def request_checksum(seed):
    """The INT64 scramble shared by CS_LOGIN_REQ and CS_CONNECT_REQ."""
    value = seed
    index, body = value % 8, value // 8
    for _ in range(index):
        value = _signed((value ^ body) + CHECK_KEY)
    return value


def _rc4(key, data):
    s = list(range(256))
    j = 0
    for i in range(256):
        j = (j + s[i] + key[i % len(key)]) & 255
        s[i], s[j] = s[j], s[i]
    out = bytearray(data)
    i = j = 0
    for n in range(len(out)):
        i = (i + 1) & 255
        j = (j + s[i]) & 255
        s[i], s[j] = s[j], s[i]
        out[n] ^= s[(s[i] + s[j]) & 255]
    return bytes(out)


def _body_transform(body, key, decrypt):
    """CPacket::Encrypt/Decrypt: returns (transformed body, checksum of the plain body)."""
    out = bytearray(body)
    checksum = 0
    words = len(out) // 8
    for n in range(words):
        word, = struct.unpack_from('<Q', out, n * 8)
        plain = word ^ key if decrypt else word
        checksum ^= plain
        struct.pack_into('<Q', out, n * 8, word ^ key)
    key_bytes = struct.pack('<Q', key)
    crc = 0
    for i in range(len(out) - words * 8):
        at = words * 8 + i
        plain = out[at] ^ key_bytes[i] if decrypt else out[at]
        out[at] ^= key_bytes[i]
        checksum ^= plain
        crc = ((crc >> 4) & 0x0FFD) ^ key
        checksum = (checksum + crc) & MASK
    return bytes(out), _signed(checksum)


def _header_mask(header, key, size, message_id=None):
    """CPacket::EncryptHeader/DecryptHeader over the 14 bytes after wSize.

    Encrypting uses the original ID throughout; decrypting reads the ID after
    its two bytes have been unmasked, which is the same value.
    """
    out = bytearray(header)
    for i in range(14):
        if i == 2 and message_id is None:
            message_id, = struct.unpack_from('<H', out, 2)
        out[2 + i] ^= (key + (size if i < 2 else message_id) + i) & 255
    return bytes(out)


def local_address():
    """A non-loopback IPv4 address of this machine.

    Login and Map classify every peer whose address equals Control's
    (127.0.0.1 in the local cluster) as a server, so the scripted client binds
    its sockets to another local address.
    """
    output = subprocess.run(['/sbin/ifconfig'], capture_output=True, text=True).stdout
    for address in re.findall(r'\binet (\d+\.\d+\.\d+\.\d+)', output):
        if not address.startswith('127.'):
            return address
    return None


class Reader:
    def __init__(self, data):
        self.data, self.at = data, 0

    def take(self, fmt):
        values = struct.unpack_from('<' + fmt, self.data, self.at)
        self.at += struct.calcsize('<' + fmt)
        return values[0] if len(values) == 1 else values

    def string(self):
        length = self.take('i')
        value = self.data[self.at:self.at + length]
        self.at += length
        return value.decode('latin1')

    def left(self):
        return len(self.data) - self.at


def string(value):
    raw = value.encode('latin1')
    return struct.pack('<i', len(raw)) + raw


class Session:
    """One client connection (Login or Map) with the original packet protection."""

    def __init__(self, host, port, source=None, timeout=20):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(timeout)
        if source:
            self.sock.bind((source, 0))
        self.sock.connect((host, port))
        self.sent = self.received = 0
        self.rc4_key = hashlib.md5(SECRET).digest()
        self.log = []  # (direction, name) for every packet
        self.inbox = []  # received packets not yet asked for

    def send(self, name, payload=b''):
        message_id = IDS[name]
        self.sent += 1
        key = KEYS[self.sent % len(KEYS)]
        size = HEADER.size + len(payload)
        body, checksum = _body_transform(payload, key, decrypt=False)
        header = _header_mask(HEADER.pack(size, message_id, self.sent, checksum), key, size, message_id)
        wire = bytearray(_rc4(self.rc4_key, header + body))
        struct.pack_into('<H', wire, 0, size)  # the client restores wSize after RC4
        self.sock.sendall(wire)
        self.log.append(('>', name))

    def _exact(self, count):
        data = b''
        while len(data) < count:
            chunk = self.sock.recv(count - len(data))
            if not chunk:
                raise ConnectionError('server closed the session')
            data += chunk
        return data

    def receive(self):
        """Next packet as (name, Reader); verifies sequence number and checksum."""
        raw = self._exact(HEADER.size)
        size, = struct.unpack_from('<H', raw, 0)
        raw += self._exact(size - HEADER.size)
        self.received += 1
        key = KEYS[self.received % len(KEYS)]
        header = _header_mask(raw[:HEADER.size], key, size)
        _, message_id, number, checksum = HEADER.unpack(header)
        if number != self.received:
            raise AssertionError(f'packet number {number}, expected {self.received}')
        body, computed = _body_transform(raw[HEADER.size:], key, decrypt=True)
        if computed != checksum:
            raise AssertionError(f'checksum mismatch on packet 0x{message_id:04X}')
        name = NAMES.get(message_id, f'0x{message_id:04X}')
        self.log.append(('<', name))
        return name, Reader(body)

    def expect(self, wanted, seconds=20):
        """Returns the next `wanted` packet; packets that arrive first are kept
        in self.inbox, which later calls search before reading the socket."""
        for index, (name, reader) in enumerate(self.inbox):
            if name == wanted:
                del self.inbox[index]
                return reader
        deadline = time.monotonic() + seconds
        try:
            while time.monotonic() < deadline:
                name, reader = self.receive()
                if name == wanted:
                    return reader
                self.inbox.append((name, reader))
        except (socket.timeout, ConnectionError) as error:
            raise AssertionError(f'no {wanted}: {type(error).__name__}; received: {self.summary()}') from error
        raise AssertionError(f'no {wanted} within {seconds}s; received: {self.summary()}')

    def summary(self):
        """Received packet names with counts, in first-seen order."""
        counts = {}
        for direction, name in self.log:
            if direction == '<':
                counts[name] = counts.get(name, 0) + 1
        return counts

    def drain(self, seconds):
        """Reads into self.inbox until the server is quiet for `seconds`."""
        previous = self.sock.gettimeout()
        self.sock.settimeout(seconds)
        try:
            while True:
                self.inbox.append(self.receive())
        except socket.timeout:
            pass
        finally:
            self.sock.settimeout(previous)

    def closed_by_server(self, seconds=5):
        previous = self.sock.gettimeout()
        self.sock.settimeout(seconds)
        try:
            while True:
                if not self.sock.recv(4096):
                    return True
        except socket.timeout:
            return False
        except OSError:
            return True
        finally:
            self.sock.settimeout(previous)

    def close(self):
        self.sock.close()
