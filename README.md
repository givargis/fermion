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
such as dictionaries.

## Usage

Build the static and shared libraries:

```sh
cd path/to/fermion/libfm/
make
```

This produces `libfm.a` and either `libfm.so` on Linux or `libfm.dylib` on
macOS. Include the self-contained public header `libfm/fm.h` and link your
C/C++ program against one of these libraries. You can use the files in place
or copy the library and header into your project. When linking the static
library, place `-lm` after it to resolve its math dependencies.

Public API names use the `fm_` prefix and names beginning with `fm__` are
internal.

To convert a Fermion source file to JSON:

```sh
cd path/to/fermion/fm2json/
make
./fm2json path/to/kernel.fm
```

The converter writes `path/to/kernel.fm.json` and reports errors to standard
error. Its build also builds the libraries.

## Example

See [fm2json/main.c](fm2json/main.c) for a complete example.

Declare `char errstr[FM_ERRSTR_LEN]` and pass it with a NUL-terminated source
program string to `fm_open()`. On success, it returns an opaque handle and
clears `errstr` to an empty string. On failure, it returns `NULL` and writes a
NUL-terminated diagnostic into the errstr buffer.

Use `fm_json()` to write the parsed representation to a file, or `fm_head()`
to access the linked list of tensor and compute nodes defined in
[libfm/fm.h](libfm/fm.h). Call `fm_close()` to release the handle and its
nodes.
