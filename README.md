# Fermion

Fermion, or `fm` for short, is the front-end lexer and parser for a high-level
synthesis tool that compiles tensor-compute kernels into computational
circuits. The synthesis tool is part of my ongoing research project at UCI and
not the focus of this repo.

Fermion is made available in the hope that it can serve as a foundation for
other projects that require parsing, analyzing, and transforming tensor-based
computational descriptions.

Fermion provides extensive support for tensor calculus and performs both
syntactic and semantic analysis of the resulting intermediate representation
(IR). In addition to validating the structure and meaning of expressions,
Fermion computes the geometric properties and dimensional structure associated
with each node in the parse tree. The resulting IR is a normalized,
semantically analyzed representation that can be traversed and processed
directly from C/C++ programs.

Fermion also provides a faithful JSON representation of the intermediate
representation. This makes it straightforward to integrate Fermion into Python
and other environments by deserializing the IR into native data structures
such as dictionaries (e.g., [python/README.md](python/README.md)).

## Usage

Build the static and shared library from the repository root:

```sh
make -C libfm
```

This produces `libfm/libfm.a` and either `libfm/libfm.so` on Linux or
`libfm/libfm.dylib` on macOS. Include the self-contained public header
`libfm/fm.h` and link your C/C++ program against one of these libraries. You
can use the files in place or copy the library and header into your project.
When linking the static library, place `-lm` after it to resolve its math
dependencies.

Public API names use the `fm_` prefix and names beginning with `fm__` are
internal.

## Tests

Build the static and shared library from the repository root:

```sh
make -C libfm
```

Run all tests:

```sh
python3 tests/run.py
```

Valid programs are parsed with the Python `libfm` binding and compared with
their complete `.json` IR expectations. Invalid programs are checked against
their `.err` diagnostics.

Each test prints `[PASS]` or `[FAIL]`, followed by total pass/fail counts.
The exit code is `0` when all tests pass and `1` when any test fails.

Use a different shared library:

```sh
python3 tests/run.py --libfm /path/to/libfm.so
```

Run under Valgrind (requires Valgrind in `PATH`):

```sh
python3 tests/run.py --valgrind
```

Valgrind diagnostics are suppressed. Memory errors produce exit code `99`,
even if the printed test counts show all tests passing.

## Example

See [fm2json/main.c](fm2json/main.c) for a complete example. This program
converts a Fermion source file to JSON:

```sh
make -C fm2json
./fm2json/fm2json path/to/kernel.fm
```
