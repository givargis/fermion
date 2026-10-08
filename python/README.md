# Python Binding

[libfm.py](libfm.py) is a small Python wrapper around Fermion's C API using
`ctypes`. It parses and validates Fermion source, exports the intermediate
representation (IR) as JSON, and returns it as Python dictionaries and lists.

## Setup

You need Python 3 and the Fermion shared library. The wrapper uses only
Python's standard library and no Python packages need to be installed.

From the repository root, build the library and run the example:

```sh
make -C libfm
cd python
python3 test.py
```

See the [repository README](../README.md) for more about building and using
Fermion. By default, the wrapper loads `libfm/libfm.so` on Linux or
`libfm/libfm.dylib` on macOS, relative to the location of `libfm.py`.

## Usage

Run this from the `python/` directory, or make that directory available on
Python's import path:

```python
import libfm

program = """
N = 10;
M = 20;
.seed 100;
a : float [N, M] = uniform();
b : float [M, N] = uniform();
z = a @ b;
"""

ir = libfm.parse(program)
for node in ir["nodes"]:
    print(node["name"], node["geometry"]["shape"])
```

Output:

```text
a [10, 20]
b [20, 10]
z [10, 10]
```

The returned dictionary includes `version` and `nodes`. Nodes describe tensors
and computations, including their data types, geometry, initialization, and
expressions. [test.py](test.py) prints the full representation for this
example.

## API

```python
libfm.parse(program, *, libfm=None)
```

- `program`: a nonempty string containing Fermion source, with no NUL
  characters. To parse a file, read its contents and pass the resulting
  string.
- `libfm`: an optional path to the shared library, supplied as a string or
  path-like object. For example,
  `libfm.parse(program, libfm="/path/to/libfm.so")`.

Invalid argument types raise `TypeError`. Empty source, NUL characters, and
Fermion parsing or validation failures raise `ValueError`. Fermion's diagnostic
is included when available. A library loading failure raises `OSError`, and a
failure to export the IR raises `RuntimeError`.

Each call writes JSON to a temporary directory, reads it into Python, and
cleans up the temporary files and C handle before returning.
