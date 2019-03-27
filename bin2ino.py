#!/usr/bin/env python3

import gzip
import mimetypes
import os
import sys
import textwrap

print('''// File generated automatically, do not modify.
#include "static.h"
''')

symbols = []

def iter_files(directory):
    directory = os.path.abspath(directory)
    for root, dirs, files in os.walk(directory):
        yield from (os.path.join(root, filename) for filename in files)

root = sys.argv[1]

for filename in sorted(iter_files(root)):
    with open(filename, 'rb') as f:
        data = f.read()
    data = gzip.compress(data)
    mime = mimetypes.guess_type(filename)[0]
    size = len(data)
    path = os.path.relpath(filename, root)
    symbol = path.upper().replace('-', '_').replace('.', '_').replace('/', '__')
    code = ', '.join(f'0x{c:02x}' for c in data)

    print(f'// {path}')
    print(f'static const unsigned char {symbol}[] PROGMEM = {{')
    for line in textwrap.wrap(code, 72):
        print('    ' + line)
    print('};')
    print()

    path = '/' + path
    symbols.append((path, mime, symbol, size))
    if os.path.basename(path.lower()) in ('index.html', 'index.htm'):
        alias = os.path.dirname(path)
        symbols.append((alias, mime, symbol, size))


print('''
const StaticEndpoint static_endpoints[] = {''')

for name, mime, symbol, size in symbols:
    print(f'    {{ "{name}", "{mime}", {symbol}, {size} }},');

print('    { nullptr, nullptr, nullptr, 0 } };')
