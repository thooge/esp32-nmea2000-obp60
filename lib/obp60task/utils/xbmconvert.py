#!/usr/bin/python
# 
# Convert a Gimp-created XBM file to bitmap useable by drawBitmap()
#

import os
import sys
import re
from PIL import Image

if len(sys.argv) < 2:
    print("Usage: xbmconvert.py <filename>")
    sys.exit(1)

xbmfilename = sys.argv[1]
if not os.path.isfile(xbmfilename):
    print(f"The file '{xbmfilename}' does not exists.")
    sys.exit(1)

im = Image.open(xbmfilename)
imname = "image"
with open(xbmfilename, 'r') as fh:
    pattern = r'static\s+unsigned\s+char\s+(\w+)_bits$$$$'
    for line in fh:
        match = re.search(pattern, line)
        if match:
            imname = match.group(1) 
            break
bytecount = int(im.width * im.height / 8)

print(f"#ifndef _{imname.upper()}_H_")
print(f"#define _{imname.upper()}_H_ 1\n")
print(f"#define {imname}_width {im.width}")
print(f"#define {imname}_height {im.height}")
print(f"const unsigned char {imname}_bits[{bytecount}] PROGMEM = {{")

n = 0
print("   ", end='')
f = im.tobytes()

switched_bytes = bytearray()
for i in range(0, len(f), 2):
    # Switch LSB and MSB
    switched_bytes.append(f[i + 1])  # Append MSB
    switched_bytes.append(f[i])      # Append LSB
 
#for b in im.tobytes():
for b in switched_bytes:
    #b2 = 0
    #for i in range(8):
    #    # b2 |= ((b >> i) & 1) << (7 - i)
    #    b2 <<= 1
    #    b2 |= b & 1
    #    b >>= 1
    n += 1
    print(f"0x{b:02x}", end='')
    if n < bytecount:
        print(', ', end='')
    if n % 12 == 0:
      print("\n   ", end='')
print("};\n\n#endif")
