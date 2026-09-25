"""Read selected files from the installed game's archive, without extracting it."""
from pathlib import Path
import struct


class GameArchive:
    def __init__(self, path):
        self.path = Path(path)
        self.entries = {}
        with self.path.open('rb') as stream:
            count = struct.unpack('<I', stream.read(4))[0]
            if count > 1_000_000:
                raise ValueError('Invalid game archive index')
            offset = 0
            for _ in range(count):
                length = struct.unpack('<H', stream.read(2))[0]
                name = stream.read(length).decode('utf-8')
                size = struct.unpack('<I', stream.read(4))[0]
                self.entries[name] = (offset, size)
                offset += size
            self.base = stream.tell()
        if self.base + offset != self.path.stat().st_size:
            raise ValueError('Game archive index does not cover its payload')

    def text(self, name):
        offset, size = self.entries[name]
        with self.path.open('rb') as stream:
            stream.seek(self.base + offset)
            result = stream.read(size)
        if len(result) != size:
            raise ValueError('Truncated archive entry: ' + name)
        return result.decode('utf-8-sig')
