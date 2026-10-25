#!/usr/bin/env python3
"""Print 0.1.74 RMS paths for a JAR; never copy, delete, or change any save."""
import argparse
import hashlib
import json
from pathlib import Path


def internal_string_bytes(text):
    # VM strings hold UTF-16 units encoded individually, including surrogate pairs.
    utf16 = text.encode('utf-16-be', 'surrogatepass')
    return b''.join(chr(int.from_bytes(utf16[i:i+2], 'big')).encode('utf-8', 'surrogatepass')
                    for i in range(0, len(utf16), 2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('jar', type=Path)
    parser.add_argument('--store', help='optional exact, case-sensitive record store name')
    args = parser.parse_args()
    digest = hashlib.sha256(args.jar.read_bytes()).hexdigest()
    directory = '/switch/j2me/rms/suites/' + digest
    result = dict(jar=str(args.jar),jar_sha256=digest,sd_directory=directory,
                  policy='Different JAR bytes use different directories; renaming unchanged bytes does not.')
    if args.store is not None:
        units = len(args.store.encode('utf-16-be','surrogatepass')) // 2
        if not 1 <= units <= 32: parser.error('store name must contain 1..32 UTF-16 code units')
        result['store_name'] = args.store
        result['sd_store_file'] = directory + '/r' + internal_string_bytes(args.store).hex() + '.bin'
    print(json.dumps(result, ensure_ascii=True, indent=2))


if __name__ == '__main__':
    main()
