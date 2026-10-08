#
# Copyright (c) 2025-2026 Tony Givargis
# University of California, Irvine
# libfm.py
#

import ctypes
import json
import os
import pathlib
import sys
import tempfile

def parse(program, *, libfm=None):

    if not isinstance(program, str):
        raise TypeError('program must be a string')

    if not program or '\0' in program:
        raise ValueError('program must be nonempty and not contain NUL chars')

    if libfm is None:
        if 'darwin' == sys.platform:
            name = 'libfm.dylib'
        else:
            name = 'libfm.so'
        libfm = pathlib.Path(__file__).resolve().parent.parent / 'libfm' / name

    lib = ctypes.PyDLL(os.fspath(libfm))
    lib.fm_open.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    lib.fm_open.restype = ctypes.c_void_p
    lib.fm_json.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.fm_json.restype = ctypes.c_int
    lib.fm_close.argtypes = [ctypes.c_void_p]
    lib.fm_close.restype = None

    errstr = ctypes.create_string_buffer(128)  # FM_ERRSTR_LEN in fm.h
    fm = lib.fm_open(program.encode('utf-8'), errstr)
    try:
        if not fm:
            if errstr.value:
                raise ValueError(errstr.value.decode(
                    'utf-8',
                    errors='replace',
                ))
            else:
                raise ValueError('fm_open failed without a diagnostic')
        with tempfile.TemporaryDirectory(prefix='libfm-') as directory:
            pathname = pathlib.Path(directory) / 'program.json'
            if lib.fm_json(fm, os.fsencode(pathname)):
                raise RuntimeError('failed to extract json file')
            with pathname.open(encoding='utf-8') as stream:
                return json.load(stream)
    finally:
        lib.fm_close(fm)
