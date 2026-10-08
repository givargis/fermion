#
# Copyright (c) 2025-2026 Tony Givargis
# University of California, Irvine
# run.py
#

import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent

sys.path.insert(0, str(ROOT.parent / 'python'))

import libfm

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--libfm',
        type=pathlib.Path,
        help='path to the Fermion shared library',
    )
    parser.add_argument(
        '--valgrind',
        action='store_true',
        help='run under Valgrind',
    )
    args = parser.parse_args()
    if args.valgrind:
        command = [
            'valgrind',
            '--leak-check=full',
            '--show-leak-kinds=all',
            '--errors-for-leak-kinds=definite,possible',
            '--track-origins=yes',
            '--error-exitcode=99',
            sys.executable,
            '-B',
            str(ROOT / 'run.py'),
        ]
        if args.libfm is not None:
            command += ['--libfm', str(args.libfm.resolve())]
        return subprocess.run(command, stderr=subprocess.DEVNULL).returncode
    passed = failed = 0
    for source in sorted(ROOT.glob('*/*.fm')):
        if source.parent.name not in {'valid', 'invalid'}:
            continue
        ok = False
        try:
            program = source.read_text(encoding='utf-8')
            if 'valid' == source.parent.name:
                expected = json.loads(
                    source.with_suffix('.json').read_text(encoding='utf-8')
                )
                ok = libfm.parse(program, libfm=args.libfm) == expected
            else:
                expected = source.with_suffix('.err').read_text(
                    encoding='utf-8'
                ).strip()
                try:
                    libfm.parse(program, libfm=args.libfm)
                except ValueError as error:
                    ok = (str(error) == expected)
        except (ValueError, OSError, RuntimeError):
            pass
        passed += ok
        failed += not ok
        print(f'[{"PASS" if ok else "FAIL"}] {source.relative_to(ROOT)}')
    print(f'Total: {passed} pass, {failed} fail')
    return int(0 < failed)

if '__main__' == __name__:
    sys.exit(main())
