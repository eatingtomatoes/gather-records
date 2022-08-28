#! /usr/bin/env python3

import json
import sys

visited_records = set()

for record in json.load(sys.stdin):
    if record["name"] in visited_records:
        continue

    visited_records.add(record["name"])

    print(f'{record["name"]} {{')

    for alias in record["types"]:
        print(f'\tusing {alias["to"]} = {alias["from"]};')

    for constant in record["constants"]:
        print(f'static constexpr {constant["type"]} {constant["name"]} = {constant["value"]};')

    print(f'}};\n')
