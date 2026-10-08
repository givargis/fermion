# Fermion C API

Fermion parses a source program, validates its types and geometry, and produces
an Intermediate Representation (IR). The public API is declared in
[`fm.h`](../libfm/fm.h) and can be called from C or C++.

This document covers the public functions. See [LANG.md](LANG.md) for the
Fermion language used in the `program` argument to `fm_open`.
See [IR.md](IR.md) for an overview of the IR structures and enums.

## API overview

| Function | Purpose |
| --- | --- |
| `fm_open` | Parse and validate a program, returning an owning handle |
| `fm_close` | Release the handle and its IR |
| `fm_json` | Write the IR to a JSON file |
| `fm_head` | Get the first tensor or compute node |
| `fm_lookup` | Find a tensor or compute node by name |

## fm_open

```c
fm_t fm_open(const char *program, char errstr[FM_ERRSTR_LEN]);
```

`program` must point to a nonempty string terminated by NUL containing
[Fermion source](LANG.md). It is source text, not a file path.
`errstr` must point to a writable buffer of at least `FM_ERRSTR_LEN` bytes.

On success, the function returns an `fm_t` handle and sets `errstr[0]` to
`'\0'`. On failure, it returns `NULL` and writes a diagnostic terminated by
NUL into `errstr`. Diagnostics include a line and column followed by the error
message. The function releases any partially constructed state on failure.

The caller owns the returned handle and must eventually pass it to `fm_close`.
The source buffer and diagnostic buffer can be reused or released after
`fm_open` returns.

## fm_close

```c
void fm_close(fm_t fm);
```

Release the handle and all nodes, expressions, and names it owns. Passing
`NULL` is allowed and has no effect.

After closing a handle, do not use it or any pointer obtained from it.
Each successful `fm_open` call requires one matching `fm_close` call.

## fm_json

```c
int fm_json(fm_t fm, const char *pathname);
```

Write the handle's IR as JSON to `pathname`. The handle must be valid, and
`pathname` must point to a nonempty string terminated by NUL.

The function returns `0` on success and `-1` on failure. It opens the path for
writing, creating the file or truncating an existing file. A failed write can
leave a partial file. The function retains the handle and its IR for further
use and does not accept a diagnostic buffer.

## fm_head

```c
const struct fm_node *fm_head(fm_t fm);
```

Return the first tensor or compute node in source definition order. The nodes
form a linked list. Scalar constant definitions are evaluated during parsing
and do not appear as nodes.

The function returns `NULL` when the handle has no nodes or when `fm` is
`NULL`. The returned pointer belongs to the handle and remains valid until
`fm_close`. Do not free or modify the returned node.

## fm_lookup

```c
const struct fm_node *fm_lookup(fm_t fm, const char *name);
```

Find a tensor or compute node by its exact name. The name must be a nonempty
string terminated by NUL. Matching is case sensitive.

The function returns `NULL` if the name is not found, if it names a scalar
constant rather than a tensor, or if either argument is invalid. The returned
pointer belongs to the handle and remains valid until `fm_close`. Do not free
or modify the returned node.

## Example

This example parses a program, finds a result tensor, writes the IR as JSON,
and releases the handle:

```c
#include <stdio.h>
#include <stdlib.h>
#include "fm.h"

int main(void)
{
    const char *program =
        "A : float [2, 3];\n"
        "B : float [3, 4] = ones();\n"
        "Result = A @ B;\n";
    char errstr[FM_ERRSTR_LEN];
    fm_t fm = fm_open(program, errstr);

    if (!fm) {
        fprintf(stderr, "Parse failed: %s\n", errstr);
        return EXIT_FAILURE;
    }

    if (!fm_head(fm) || !fm_lookup(fm, "Result")) {
        fprintf(stderr, "Expected tensor was not found\n");
        fm_close(fm);
        return EXIT_FAILURE;
    }

    if (fm_json(fm, "kernel.json") != 0) {
        fprintf(stderr, "Could not write kernel.json\n");
        fm_close(fm);
        return EXIT_FAILURE;
    }

    fm_close(fm);
    return EXIT_SUCCESS;
}
```
