"""Read only the known AC7 data table to verify its native container layout."""
import struct
import sys
from inspect_native import GameMemory
mem = GameMemory(int(sys.argv[1]))
table = int(sys.argv[2], 16)
try:
    raw = mem.read(table, 0xB0)
    for i in range(0x28, len(raw), 8):
        print(hex(i), raw[i:i+8].hex(), struct.unpack('<Q', raw[i:i+8])[0])
    data, count, capacity = struct.unpack('<Qii', raw[0x30:0x40])
    print('elements', hex(data), count, capacity)
    assert 0 < count <= capacity <= 4096
    def string(address):
        ptr,n,cap=struct.unpack('<Qii',mem.read(address,16))
        assert 0 <= n <= cap <= 4096
        return mem.read(ptr,n*2).decode('utf-16-le').rstrip('\0') if n else ''
    for i in range(count):
        entry=mem.read(data+i*24,24)
        row=struct.unpack('<Q',entry[8:16])[0]
        print('row',i,hex(row),struct.unpack('<i',mem.read(row+8,4))[0],string(row+16),[string(row+96+k*24) for k in range(3)] if len(sys.argv)>3 else '')
finally:
    mem.close()
