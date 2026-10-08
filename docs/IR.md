# Fermion Intermediate Representation

Fermion converts a validated program into an Intermediate Representation (IR)
containing tensor definitions and their computations. This guide describes
the public data structures from the top down. See [`fm.h`](../libfm/fm.h)
for the complete definitions and operation comments, [README.md](README.md)
for the API, and [LANG.md](LANG.md) for the source language.

## Handle and node list

An `fm_t` handle owns the IR. `fm_head` returns the first `struct fm_node`,
and each node's `link` points to the next node in source definition order.
The final link is `NULL`. `fm_lookup` finds a node by name.

Nodes represent declared and computed tensors. Scalar constant definitions
are evaluated during parsing and do not become nodes. All IR pointers belong
to the handle and remain valid until `fm_close`. Treat the IR as read only.

## Tensor nodes

`struct fm_node` describes a named tensor:

| Field | Purpose |
| --- | --- |
| `name` | Tensor name |
| `op` | Node category, selected by `enum fm_node_op` |
| `dtype` | Element type, selected by `enum fm_dtype` |
| `geometry` | Shape and storage geometry |
| `init` | Initialization kind and parameters for a declared tensor |
| `expr` | Root expression for a computed tensor |
| `max_expr_id` | Maximum expression ID allocated for this computation |
| `link` | Next node in the list |

`FM_NODE_OP_TENSOR` identifies a tensor declaration. `FM_NODE_OP_COMPUTE`
identifies a computed tensor with an expression. Input, constant, intermediate,
and output roles follow the rules in [LANG.md](LANG.md#kernel-inputs-constants-and-outputs).
They are not separate values of `enum fm_node_op`.

The `init` record uses `enum fm_init_op` to identify zeros, ones, uniform,
normal, or iota initialization. `FM_INIT_OP_UNKNOWN` indicates no initializer.
The remaining fields hold the parameters:

| Fields | Use |
| --- | --- |
| `low`, `high` | Uniform bounds, or normal mean and standard deviation |
| `start`, `step` | Iota sequence parameters |
| `seed` | Random initializer seed, with 0 specifying nondeterministic initialization |

## Geometry and dtypes

Both nodes and expressions contain a `struct fm_geometry`:

| Field | Purpose |
| --- | --- |
| `ndim` | Rank |
| `size` | Storage size in elements, which can include gaps |
| `numel` | Logical element count |
| `offset` | First logical element's position within storage |
| `shape` | Size of each axis |
| `stride` | Storage distance in elements along each axis |

Only the first `ndim` entries of `shape` and `stride` describe active axes.
The arrays have capacity `FM_MAX_NDIM`. A scalar tensor has rank 0 and one
logical element. An empty tensor has zero logical elements.

Views can retain the original storage size while changing shape, strides,
and offset. Strides can be negative for reversed slices or zero for broadcast
axes. Storage size and logical element count therefore need not match.

`enum fm_dtype` identifies Boolean, signed and unsigned integer, and floating
point element types. Its names describe fixed formats, such as
`FM_DTYPE_INT32` and `FM_DTYPE_FP32`. The language spellings are listed in
[LANG.md](LANG.md#dtypes).

## Expressions

A computed node's `expr` points to a `struct fm_expr`. Expressions connect
operations to their operands and describe the resulting dtype and geometry.

| Field | Purpose |
| --- | --- |
| `id` | Expression identifier within the computation |
| `refs` | Internal reference count used for ownership |
| `lineno`, `column` | Source location associated with the expression |
| `op` | Operation, selected by `enum fm_expr_op` |
| `dtype` | Result element type |
| `left`, `right` | Operand expressions, where used by the operation |
| `geometry` | Result geometry |
| `u` | Operation specific value or metadata |

`enum fm_expr_op` covers scalar values, tensor references, views, casts,
arithmetic, comparisons, logic, math functions, matrix multiplication,
index operations, reductions, scans, and sorting. Fermion can insert view
and copy operations while normalizing a computation, so the IR need not
mirror the source expression exactly.

The operation determines which operands and union members are meaningful.
For example, a binary arithmetic operation uses `left` and `right`. A scalar
expression uses `u.fp64` or `u.int64`, with Boolean scalar values represented
as 0 or 1. A tensor reference uses `u.node` to refer to a named tensor node.
Conditional selection uses `u.expr` for the condition, and `left` and `right`
for its two branches. Clamp uses `u.expr` for its upper bound.

## Operation metadata

The expression union also contains small records for operations with additional
parameters:

| Structure | Fields | Use |
| --- | --- | --- |
| `struct fm_fold` | `axis`, `op` | Reduction or scan axis and combining operation |
| `struct fm_sort` | `axis`, `k`, `sorted`, `descending` | Argsort and topk parameters |
| `struct fm_index` | `axis`, `source` | Concat, gather, and scatter parameters |

`enum fm_fold_op` selects minimum, maximum, sum, count, product, argmin,
argmax, any, or all. A fold uses `right` for its input and `left` for an
optional mask. A scan accumulates the selected operation along its axis.

For scatter, `u.index.source` points to the source expression. The primary
operands are the input and indices. For the exact operand layout of each
operation, consult the comments beside `enum fm_expr_op` in
[`fm.h`](../libfm/fm.h).
