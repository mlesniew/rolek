#!/usr/bin/env python3

import os
import textwrap
import sys
import gzip
import mimetypes

symbols = {}

for idx, filename in enumerate(sys.argv[1:], 1):
    with open(filename, 'rb') as f:
        data = f.read()
    data = gzip.compress(data)
    mime = mimetypes.guess_type(filename)[0]
    size = len(data)
    name = os.path.basename(filename)
    symbol = name.upper().replace('.', '_')

    code = ', '.join('0x%02x' % c for c in data)
    code = 'const unsigned char %s[] PROGMEM = { %s };' % (symbol, code)

    print('// %s' % filename)
    for line in textwrap.wrap(code, 72):
        print(line)
    print()

    symbols[name] = {
            'mime': mime,
            'symbol': symbol,
            'size': size,
            }

print('''
void setup_static_endpoints(
        ESP8266WebServer & server,
        std::function<void(void)> before_fn,
        std::function<void(void)> after_fn) {''')

for name, info in symbols.items():
    handler = info['symbol'].lower() + '_handler';
    print('''    auto %s = [&server, before_fn, after_fn] {
        if (before_fn)
            before_fn();
        server.sendHeader("Content-Encoding", "gzip");
        server.send_P(200,
                "%s",
                (PGM_P)(%s),
                %i);
        if (after_fn)
            after_fn();
        };''' % (handler, info['mime'], info['symbol'], info['size']))

    names = [name]
    if name in ('index.htm', 'index.html'):
        names.append('')

    for alias in names:
        print('    server.on("/%s", HTTP_GET, %s);' % (alias, handler))

    print()

print('}')
