#!/usr/bin/env python3
import json

B = [1 << i for i in range(7)]
B1, B2, B3, B4, B5, B6, B7 = B

data = {
    'Parter': B1 | B2 | B3,
    'Piętro': B4 | B5 | B6 | B7,
    'Front': B1 | B6 | B7,
    'Ogrod': B2 | B3 | B4 | B5,
    'Salon': B2 | B3,
    'Sypialnia': B4,
    'Hania': B5,
    'Zuzia': B6,
    'Biuro': B7,
    'Kotłownia': B1,
    'Salon L': B2,
    'Salon R': B3,
}

if False:
    # minified
    print(json.dumps(data, separators=(',', ':')))
else:
    # pretty
    print(json.dumps(data, indent=4, separators=(',', ': ')))
