# Fermion Language

## Defining a tensor

A tensor is a multidimensional collection of elements with a common data type.
Declare one by giving it a name, a dtype, and a shape:

```fm
A : float [2, 3];
```

This defines `A` as a tensor with two rows and three columns of 32 bit
floating point elements. Every statement ends with a semicolon. Names must be
defined before use and cannot be redefined.

The declaration syntax is:

```text
name : dtype [shape] [stride] = initializer;
```

The stride list and initializer are optional. Without an initializer, the
declaration leaves the tensor's values unspecified. An uninitialized tensor is
assumed to be an input tensor to the kernel.

```fm
Vector : int [8];
Matrix : float [2, 3] = zeros();
Volume : double [4, 8, 8] = ones();
```

Fermion describes tensor computations. It checks the program and produces an
Intermediate Representation (IR) containing types, geometry, expressions, and
initialization instructions for a downstream tool to process.

## Dtypes

The dtype determines the type of every element. These are the supported
language spellings:

| Dtype | Element type |
| --- | --- |
| `bool` | Boolean |
| `byte` | Signed 8 bit integer |
| `unsigned byte` | Unsigned 8 bit integer |
| `short` | Signed 16 bit integer |
| `unsigned short` | Unsigned 16 bit integer |
| `int` | Signed 32 bit integer |
| `unsigned int` | Unsigned 32 bit integer |
| `long` | Signed 64 bit integer |
| `unsigned long` | Unsigned 64 bit integer |
| `fp8_e4m3` | 8 bit floating point, E4M3 format |
| `fp8_e5m2` | 8 bit floating point, E5M2 format |
| `bf16` | 16 bit bfloat16 |
| `fp16` | 16 bit floating point |
| `float` | 32 bit floating point |
| `double` | 64 bit floating point |

## Shape

Shape lists the number of elements along each axis. The number of entries is
the tensor's rank, and their product is its logical element count:

```fm
N = 3;
A : float [2, N];
B : float [2, N * 2];
```

`A` has rank 2 and six elements. `B` has rank 2 and twelve elements. Shape
entries must be integer expressions that can be evaluated when parsing.
Dimensions are nonnegative. A zero dimension makes the tensor empty.
Declarations require at least one dimension and support up to 32 dimensions.

## Stride

Stride describes the storage distance, in elements, when moving one position
along an axis. Without an explicit stride, a nonempty tensor uses contiguous
storage in row major order. The last axis has stride 1.

```fm
Contiguous : float [2, 3];
Explicit : float [2, 3] [3, 1];
Padded : float [2, 3] [4, 1];
```

`Contiguous` and `Explicit` have the same geometry. For `Padded`, adjacent
columns are one element apart, but the next row begins four elements later,
leaving a gap between rows.

An explicit stride list must have one entry per shape entry. Its entries must
evaluate to positive integers, and the declared layout must not overlap itself.
For empty tensors with omitted strides, Fermion assigns stride 1 to every axis.

## Offset

Offset is the position of the first logical element relative to the underlying
storage, measured in elements. A newly declared tensor has offset 0. Offset is
part of the computed tensor geometry. Views such as slices can introduce a
nonzero offset:

```fm
T : float [8];
Window = T[2:6];
```

`Window` has shape `[4]`, stride `[1]`, and offset 2. The slice includes
positions 2 through 5. Its stop position is exclusive.

Shape, stride, and offset together describe how logical indices map to storage.
For indices `i, j, ...`, the storage position is:

```text
offset + i * stride[0] + j * stride[1] + ...
```

For the padded matrix above, element `[1, 2]` is at position
`0 + 1 * 4 + 2 * 1 = 6`.

## Initialization

Add an initializer after `=` to describe the tensor's initial values:

| Initializer | Meaning | Defaults |
| --- | --- | --- |
| `zeros()` | Fill with zero | No arguments |
| `ones()` | Fill with one | No arguments |
| `uniform(low, high)` | Random uniform | `uniform()` uses 0.0 and 1.0 |
| `normal(mean, stddev)` | Random normal | `normal()` uses 0.0 and 1.0 |
| `iota(start, step)` | Integer sequence | `iota()` uses 0 and 1 |

```fm
Z : float [2, 3] = zeros();
O : double [2, 3] = ones();
U : float [2, 3] = uniform(-1, 1);
G : float [2, 3] = normal(0, 0.5);
Indices : long [6] = iota(10, 2);
```

`Indices` describes the sequence `10, 12, 14, 16, 18, 20`. `iota` requires an
integer dtype, signed 64 bit integer parameters, and generated values that fit
the destination dtype. Its step can be positive, negative, or zero.
For `unsigned long`, generated values are additionally limited to the range
0 through 9223372036854775807 (`INT64_MAX`), even though the destination dtype
can represent larger values.

`uniform`, `normal`, and `iota` accept either no arguments or exactly two
arguments. Parameters must be evaluable during parsing. Uniform bounds must be
finite with `low < high`. Normal parameters must be finite with `stddev > 0`.

Use `.seed` before random initializers to specify their seeds:

```fm
.seed 42;
Weights : float [2, 3] = uniform(-1, 1);
Noise : float [2, 3] = normal();
```

The first random initializer receives seed 42 and the next receives seed 43.
The default seed, 0, specifies nondeterministic initialization. The parser
records these initialization instructions. The downstream consumer generates
the values.

## Operators

Define a computed tensor by assigning an expression to a new name. Fermion
infers its dtype and geometry from the expression:

```fm
A : float [2, 3];
B : float [2, 3];
C = A + B;
D = (A - B) * 2;
```

Elementwise operators act on corresponding elements. Two tensor operands must
have the same shape and dtype. A compatible scalar operand applies to each
element, as `2` does in the example above.

### Arithmetic

| Operator | Meaning |
| --- | --- |
| `+A` | Unary plus |
| `-A` | Negation |
| `A + B` | Addition |
| `A - B` | Subtraction |
| `A * B` | Elementwise multiplication |
| `A / B` | Division |
| `A % B` | Integer remainder |
| `A @ B` | Matrix multiplication |

Arithmetic requires numeric operands. Boolean tensors are not numeric operands.
The remainder operator requires integers.

### Matrix multiplication

Use `A @ B` to multiply matrices. Each result element is the sum of products
between a row of `A` and a column of `B`. The number of columns in `A` must
equal the number of rows in `B`. Both operands must be numeric tensors with
the same dtype and at least one axis.

For input shapes `[M, K]` and `[K, N]`, the result has shape `[M, N]`:

```fm
A : double [2, 3];
B : double [3, 4];
C = A @ B;
```

`C` has shape `[2, 4]`. Its element at row `i` and column `j` is:

```text
C[i, j] = A[i, 0] * B[0, j]
        + A[i, 1] * B[1, j]
        + A[i, 2] * B[2, j]
```

The operators `*` and `@` perform different computations. `A * B` multiplies
corresponding elements and requires matching shapes. `A @ B` sums products
along the shared inner dimension.

The operator also supports vectors:

| Left shape | Right shape | Result shape | Operation |
| --- | --- | --- | --- |
| `[K]` | `[K]` | `[]` | Vector dot product |
| `[M, K]` | `[K]` | `[M]` | Matrix times vector |
| `[K]` | `[K, N]` | `[N]` | Vector times matrix |
| `[M, K]` | `[K, N]` | `[M, N]` | Matrix times matrix |

```fm
X : float [3];
Y : float [3];
A : float [2, 3];
B : float [3, 4];
Dot = X @ Y;
MatrixVector = A @ X;
VectorMatrix = X @ B;
```

`Dot` is a scalar tensor. `MatrixVector` has shape `[2]`, and `VectorMatrix`
has shape `[4]`. A scalar tensor cannot be an operand of `@`.

For tensors with more than two axes, the last two axes describe each matrix.
The leading axes describe batches of matrices. Batch axes align from the right
and automatically broadcast. Each aligned pair must have equal sizes or one
size must be 1. Missing leading batch axes act as size 1.

```fm
A : double [5, 2, 3];
B : double [3, 4];
C = A @ B;
X : double [3];
D = A @ X;
```

`C` has shape `[5, 2, 4]`. The same matrix `B` is used for each of the five
matrices in `A`. `D` has shape `[5, 2]`, using the same vector `X` for each
matrix.

Multiple batch axes can expand independently:

```fm
A : double [2, 1, 3, 4];
B : double [5, 4, 6];
C = A @ B;
```

The batch shapes `[2, 1]` and `[5]` combine into `[2, 5]`. The matrix shapes
`[3, 4]` and `[4, 6]` produce `[3, 6]`, so `C` has shape `[2, 5, 3, 6]`.
The shared inner dimensions must match exactly even when batch axes broadcast.

Tensor views can be used directly as operands, including transposed matrices:

```fm
A : float [2, 3];
Gram = A @ A`;
```

`Gram` has shape `[2, 2]`. For a matrix, the backtick swaps its two axes.
For a tensor with batch axes, use an explicit permutation to swap only the
matrix axes, since the backtick reverses every axis.

`@` has the same precedence as `*` and `/` and associates from left to right.
Use parentheses to specify a different multiplication order, as in
`A @ (B @ C)`.

### Comparisons and logic

| Operator | Meaning |
| --- | --- |
| `A < B`, `A > B` | Less than, greater than |
| `A <= B`, `A >= B` | Less than or equal, greater than or equal |
| `A == B`, `A != B` | Equal, not equal |
| `!A` | Logical NOT |
| `A && B` | Logical AND |
| `A ^^ B` | Logical XOR |
| `A \|\| B` | Logical OR |
| `Condition ? A : B` | Select `A` where condition is true and `B` otherwise |

Tensor comparisons and logical operations produce Boolean tensors. A tensor
condition selects values element by element. A scalar Boolean condition selects
one entire branch:

```fm
A : float [2, 3];
B : float [2, 3];
Mask = A > B;
Equal = A == B;
Either = Mask || Equal;
Selected = Mask ? A : B;
```

### Integer bit operations

| Operator | Meaning |
| --- | --- |
| `~A` | Bitwise complement |
| `A & B` | Bitwise AND |
| `A ^ B` | Bitwise XOR |
| `A \| B` | Bitwise OR |
| `A << B` | Shift left |
| `A >> B` | Shift right |

These operators require integer operands:

```fm
Bits : unsigned int [4];
Low = Bits & 15;
Shifted = Bits << 2;
Inverted = ~Bits;
```

### Tensor views

Indexing, slicing, and axis permutation describe views of a tensor:

| Syntax | Meaning |
| --- | --- |
| `A[i]` | Select an index along the first axis |
| `A[start:stop:step]` | Select a slice with an exclusive stop |
| `A[:, i]` | Keep the first axis and select an index along the second |
| `` A` `` | Reverse the order of the axes |
| `A{1, 0}` | Permute axes into the specified order |

An integer index removes its axis. A slice keeps its axis. Slice bounds and
step can be omitted, as in `A[:]` or `A[::2]`. Axis numbers start at zero,
and a permutation must list every axis exactly once.

```fm
A : float [2, 3];
Row = A[1];
Column = A[:, 1];
Transposed = A`;
Permuted = A{1, 0};
```

`Row` has shape `[3]`, and `Column` has shape `[2]`. Both `Transposed` and
`Permuted` have shape `[3, 2]`. Views carry the shape, stride, and offset
needed to describe their relationship to the original tensor.

### Precedence

The following table lists operators from highest to lowest precedence.
Parentheses override this order:

| Operators | Group |
| --- | --- |
| `[...]`, `{...}`, backtick | Tensor views |
| Unary `+`, `-`, `~`, `!` | Unary operations |
| `*`, `@`, `/`, `%` | Multiplication and division |
| `+`, `-` | Addition and subtraction |
| `<<`, `>>` | Shifts |
| `<`, `>`, `<=`, `>=` | Ordering comparisons |
| `==`, `!=` | Equality comparisons |
| `&` | Bitwise AND |
| `^` | Bitwise XOR |
| `\|` | Bitwise OR |
| `&&` | Logical AND |
| `^^` | Logical XOR |
| `\|\|` | Logical OR |
| `? :` | Conditional selection |

Binary operators at the same precedence group associate from left to right.
Unary operators and conditional selection associate from right to left.
For example, `A + B * 2` means `A + (B * 2)`, while `(A + B) * 2` scales
the sum.

## Broadcast

Use `broadcast` to give a tensor a target shape by repeating values along
selected axes:

```text
broadcast(tensor, [shape])
```

The input axes align with the target axes from the right. The target rank must
be at least the input rank. Each aligned dimension must either match the input
dimension or expand an input dimension of size 1. Additional leading axes can
have any nonnegative size.

```fm
Row : float [3];
Column : float [2, 1];
Rows = broadcast(Row, [2, 3]);
Columns = broadcast(Column, [2, 3]);
Sum = Rows + Columns;
```

`Rows` repeats the three values of `Row` across two rows. `Columns` repeats
each value of `Column` across three columns. Both results have shape `[2, 3]`,
so they can be added element by element.

Broadcast preserves the dtype and offset. It describes a view with stride 0
along each added or expanded axis, so repeated positions refer to the same
input element. Matching axes keep their original strides.

Target dimensions must be integer expressions evaluable during parsing. They
must be nonnegative, and the target can have up to 32 axes. An input dimension
of size 0 must remain 0 on its aligned axis. A dimension of size 1 can become
0, producing an empty tensor.

The operand must be a tensor. Indexing every axis produces a scalar tensor,
which can also be broadcast:

```fm
Values : float [3];
Repeated = broadcast(Values[0], [2, 3]);
```

`Repeated` has shape `[2, 3]` with the same value at every position.

## Cast

Use `cast` to convert an expression to a specified dtype:

```text
cast<dtype>(expression)
```

The target can be any dtype listed above. The operand can be a tensor or an
integer, floating point, or Boolean scalar. A tensor operand keeps its shape.
A scalar operand produces a scalar tensor with rank 0 and one element.

```fm
A : float [2, 3];
B : double [2, 3];
Converted = cast<double>(A);
Sum = Converted + B;
Scalar = cast<float>(2);
```

`Converted` has dtype `double` and shape `[2, 3]`. Explicit conversion allows
it to be added to `B`, since tensor arithmetic requires matching dtypes.
`Scalar` has dtype `float` and shape `[]`.

Cast also allows Boolean tensor values to participate in numeric computations:

```fm
A : float [2, 3];
Mask = A > 0;
NumericMask = cast<float>(Mask);
Masked = A * NumericMask;
```

## Functions

Call a function by its name with arguments in parentheses. Axis arguments are
integer expressions evaluated during parsing. Axes start at zero. Negative
axes count from the end, so `-1` refers to the last axis.

### Geometry queries

These functions accept any tensor and return integer scalars evaluated during
parsing. Their results can be used in declarations and shape expressions.

| Function | Result |
| --- | --- |
| `ndim(A)` | Number of axes |
| `numel(A)` | Logical element count |
| `shape(A, axis)` | Size of the specified axis |

```fm
A : float [2, 3];
Rank = ndim(A);
Size = numel(A);
Columns = shape(A, -1);
B : float [Columns, Size];
```

`Rank` is 2, `Size` is 6, and `Columns` is 3. `B` has shape `[3, 6]`.

### Tensor arrangement

These operations preserve the dtype:

| Function | Meaning |
| --- | --- |
| `flatten(A)` | Combine all axes into one axis |
| `reshape(A, [shape])` | Change the shape while preserving the element count |
| `squeeze(A, axis)` | Remove an axis of size 1 |
| `unsqueeze(A, axis)` | Insert an axis of size 1 |
| `concat(A, B, axis)` | Join two tensors along an axis |
| `gather(A, indices, axis)` | Select values using an index tensor |
| `scatter(A, indices, source, axis)` | Place source values at indexed positions in a result based on `A` |

For `reshape`, dimensions must be nonnegative, except that one dimension can
be `-1` to infer its size from the element count. Inference requires the
product of the other dimensions to be nonzero and to divide the element count
exactly. Use `[]` for a scalar tensor when the input has one element.

For `unsqueeze`, the axis specifies a position in the result rank. For
`concat`, the operands must have matching dtypes, ranks, and dimensions
outside the specified axis.

```fm
A : float [2, 3];
Flat = flatten(A);
Reshaped = reshape(A, [3, -1]);
Expanded = unsqueeze(A, 0);
Restored = squeeze(Expanded, 0);
Joined = concat(A, A, 0);
```

`Flat` has shape `[6]`, `Reshaped` has shape `[3, 2]`, and `Expanded` has shape
`[1, 2, 3]`. `Restored` has shape `[2, 3]`, and `Joined` has shape `[4, 3]`.

Gather and scatter require signed integer index tensors with the same rank as
the input. Outside the selected axis, index dimensions cannot exceed input
dimensions. Gather produces the shape of `indices`. Scatter requires `source`
to have the same shape as `indices` and the same dtype as `A`. Its result has
the shape of `A`.

```fm
A : float [2, 3];
Indices : long [2, 1] = zeros();
Picked = gather(A, Indices, 1);
Updated = scatter(A, Indices, Picked, 1);
```

`Picked` selects column 0 from each row and has shape `[2, 1]`. `Updated` has
shape `[2, 3]`.

### Elementwise math

Elementwise functions preserve the input shape. Numeric functions accept
integer or floating point tensors. Functions described as floating point
require a floating point tensor. Results preserve the input dtype unless the
function is a Boolean test.

| Functions | Meaning | Input |
| --- | --- | --- |
| `abs(A)` | Absolute value | Numeric |
| `min(A, B)`, `max(A, B)` | Minimum or maximum at each position | Numeric |
| `clamp(A, low, high)` | Limit values to the given bounds | Numeric |
| `copysign(A, B)` | Magnitude of `A` with the sign of `B` | Floating point |
| `ceil(A)`, `floor(A)` | Round toward positive or negative infinity | Floating point |
| `round(A)`, `trunc(A)` | Round to nearest or truncate toward zero | Floating point |
| `exp(A)`, `exp2(A)` | Exponential with base e or base 2 | Floating point |
| `log(A)`, `log1p(A)` | Natural logarithm of `A` or of `1 + A` | Floating point |
| `sqrt(A)`, `pow(A, B)` | Square root or power | Floating point |
| `sin(A)`, `cos(A)`, `tan(A)` | Sine, cosine, tangent | Floating point |
| `sec(A)`, `cot(A)` | Secant, cotangent | Floating point |
| `asin(A)`, `acos(A)`, `atan(A)` | Inverse trigonometric functions | Floating point |
| `atan2(Y, X)` | Angle from the two coordinates | Floating point |
| `erf(A)`, `erfc(A)` | Error function and its complement | Floating point |
| `tgamma(A)`, `lgamma(A)` | Gamma function and logarithm of its absolute value | Floating point |
| `normal_cdf(A)`, `normal_sf(A)` | Standard normal cumulative probability and survival probability | Floating point |

For functions with multiple value arguments, the first argument must be a
tensor. Later arguments can be tensors with the same dtype and shape, or
compatible scalars.

```fm
A : float [2, 3];
Magnitude = abs(A);
Bounded = clamp(A, -1, 1);
Squared = pow(A, 2);
Wave = sin(A);
```

### Floating point tests and activations

These tests take a floating point tensor and return a Boolean tensor of the
same shape:

| Function | Test |
| --- | --- |
| `signbit(A)` | Sign bit is set |
| `isinf(A)` | Value is infinite |
| `isnan(A)` | Value is NaN |
| `isnormal(A)` | Value is a normal floating point number |
| `isfinite(A)` | Value is finite |

The activation functions `relu(A)`, `gelu(A)`, `sigmoid(A)`, `silu(A)`, and
`softplus(A)` take a floating point tensor and preserve its shape and dtype.

| Function | Meaning |
| --- | --- |
| `relu(A)` | Rectified linear unit, `max(A, 0)` |
| `gelu(A)` | Gaussian error linear unit, `A * normal_cdf(A)` |
| `sigmoid(A)` | Logistic function, `1 / (1 + exp(-A))` |
| `silu(A)` | Sigmoid linear unit, `A * sigmoid(A)` |
| `softplus(A)` | Smooth rectified linear unit, `log(1 + exp(A))` |

```fm
A : float [2, 3];
Finite = isfinite(A);
Activated = relu(A);
Probability = sigmoid(A);
```

### Reductions and scans

Reductions combine values along one axis and remove that axis from the result.
Reducing a vector produces a scalar tensor.

| Function | Meaning | Input | Result dtype |
| --- | --- | --- | --- |
| `reduce_min(A, axis)` | Minimum along the axis | Numeric | Same as `A` |
| `reduce_max(A, axis)` | Maximum along the axis | Numeric | Same as `A` |
| `sum(A, axis)` | Sum along the axis | Numeric | Same as `A` |
| `product(A, axis)` | Product along the axis | Numeric | Same as `A` |
| `count(A, axis)` | Count participating elements | Any tensor | `long` |
| `argmin(A, axis)` | Index of the minimum along the axis | Numeric | `long` |
| `argmax(A, axis)` | Index of the maximum along the axis | Numeric | `long` |
| `any(A, axis)` | Whether any value is true | Any tensor | `bool` |
| `all(A, axis)` | Whether all values are true | Any tensor | `bool` |

For numeric inputs to `any` and `all`, zero is false and nonzero is true.
`min(A, B)` and `max(A, B)` compare values at corresponding positions.
Use `reduce_min` and `reduce_max` to combine values along an axis.

All reductions except `argmin` and `argmax` accept an optional final Boolean
tensor mask. Only positions where the mask is true participate. The mask shape
must be compatible with broadcasting to the input shape.

```fm
A : float [2, 3];
RowSums = sum(A, 1);
ColumnMaxima = reduce_max(A, 0);
Positive = A > 0;
PositiveSums = sum(A, 1, Positive);
PositiveCounts = count(A, 1, Positive);
```

`RowSums`, `PositiveSums`, and `PositiveCounts` have shape `[2]`.
`ColumnMaxima` has shape `[3]`.

Scans accumulate values along an axis while preserving the shape and dtype:

| Function | Meaning |
| --- | --- |
| `cumsum(A, axis)` | Cumulative sum including the current element |
| `cumprod(A, axis)` | Cumulative product including the current element |

Both require numeric tensors and take exactly two arguments.

```fm
A : long [4] = iota(1, 1);
RunningSum = cumsum(A, 0);
RunningProduct = cumprod(A, 0);
```

The results describe `1, 3, 6, 10` and `1, 2, 6, 24`, respectively.

### Sorting and selection

These functions accept numeric tensors and return `long` tensors containing
indices along the chosen axis:

| Function | Meaning |
| --- | --- |
| `argsort(A, axis, descending)` | Indices that order every value along the axis |
| `topk(A, k, axis, descending, sorted)` | Indices of the selected `k` values along the axis |

`argsort` preserves the input shape. `topk` replaces the selected axis size
with `k`, which must be between zero and the input axis size. The Boolean
`descending` argument selects largest values first when true and smallest
values first when false. For `topk`, `sorted` specifies whether the selected
indices follow that value order. All arguments are required. `k` and the
Boolean flags must be evaluable during parsing.

Use `gather` to retrieve the corresponding values:

```fm
A : float [2, 3];
Order = argsort(A, 1, false);
Ordered = gather(A, Order, 1);
BestIndices = topk(A, 2, 1, true, true);
BestValues = gather(A, BestIndices, 1);
```

`Ordered` has shape `[2, 3]`. `BestIndices` and `BestValues` have shape
`[2, 2]`.

## Kernel inputs, constants, and outputs

A tensor's definition and use determine its role in the kernel:

| Tensor role | Rule |
| --- | --- |
| Input | A tensor declaration without an initializer |
| Constant | A tensor declaration with an initializer, internal to the kernel |
| Intermediate | A computed tensor used in a subsequent computation |
| Output | A remaining computed tensor not used in a subsequent computation |

Inputs receive their values from outside the kernel. Constant tensors receive
their values from their initializers and are internal to the kernel.
Intermediate tensors connect computations within the kernel. Output tensors
provide the results of the kernel.

```fm
X : float [2, 3];
Weights : float [3, 4] = ones();
Bias : float [4] = zeros();
Product = X @ Weights;
Result = Product + broadcast(Bias, [2, 4]);
RowTotals = sum(Result, 1);
RowMaxima = reduce_max(Result, 1);
```

`X` is an input. `Weights` and `Bias` are internal constant tensors.
`Product` and `Result` are intermediate tensors because later computations use
them. `RowTotals` and `RowMaxima` are outputs because no subsequent computation
uses them. A kernel can have multiple inputs and multiple outputs.

## Miscellaneous

### Comments

Fermion supports C and C++ comment syntax. Comments are ignored when parsing:

| Syntax | Meaning |
| --- | --- |
| `// comment` | Comment through the end of the line |
| `/* comment */` | Block comment that can span multiple lines |

Block comments must be closed and cannot be nested.

```fm
// Kernel input
Input : float [2, 3];

/* Initialize the weights.
   This comment spans two lines. */
Weights : float [3, 4] = ones();
Result = Input @ Weights; // Kernel output
```

### Whitespace

Spaces, tabs, and line breaks between tokens do not affect the computation.
Indentation is optional, and a statement can span multiple lines. Statements
still require a terminating semicolon.

```fm
Input:float[2,3];
Weights : float [3, 4] = ones();
Result =
    Input
    @ Weights
    ;
```

Whitespace must preserve token boundaries. For example, `unsigned int` needs
separation between its two words, while an operator such as `>=` must remain
together. Whitespace inside a quoted literal is part of its value.

### Tensor and identifier names

Names follow the familiar C and C++ identifier pattern. A name starts with a
letter or underscore, followed by letters, digits, or underscores. Names are
case sensitive, so `Input` and `input` are different identifiers.

| Name | Valid identifier form |
| --- | --- |
| `Input`, `weights`, `_temporary`, `layer2_output` | Yes |
| `2input`, `layer-output`, `my tensor` | No |

Language keywords and built in function names are reserved. For example,
`float`, `cast`, and `sum` cannot be tensor names. Define each name before
using it, and use a new name for every definition. These rules apply to scalar
constants as well as tensors.

```fm
Width = 3;
input_tensor : float [2, Width];
_weights2 : float [Width, 4] = ones();
layer2_output = input_tensor @ _weights2;
```

### Literals

Literals provide scalar values directly in an expression:

| Kind | Examples | Meaning |
| --- | --- | --- |
| Integer | `42`, `0x2A`, `0X2a` | Decimal or hexadecimal integer |
| Floating point | `1.5`, `2.0`, `1.5e3`, `2E-3` | Floating point value |
| Boolean | `true`, `false` | Boolean value |
| Character | `'A'`, `'\n'`, `'\x41'` | Integer value of a single byte |
| String | `"hello"`, `"line\n"` | String scalar |

A leading minus is the unary negation operator, as in `-42` or `-1.5`.
Integer literals are evaluated with arbitrary precision. A particular use,
such as a shape or initializer parameter, can impose a smaller range.
Floating point literals are represented as double precision values during
parsing. Values outside the supported floating point range are rejected.

Floating point parsing and JSON export currently depend on the host process's
numeric locale (`LC_NUMERIC`). Use the `C` numeric locale, which uses a decimal
point, when parsing source or exporting JSON. Decimal-comma locales can reject
source literals such as `0.5` and cause JSON export to write invalid numbers
such as `0,0`. Fermion does not set the host process's locale.

Character and string literals support these escapes:

| Escape | Meaning |
| --- | --- |
| `\0` | NUL byte, allowed in character literals only |
| `\a`, `\b`, `\f` | Alert, backspace, form feed |
| `\n`, `\r`, `\t`, `\v` | Newline, carriage return, tab, vertical tab |
| `\\`, `\'`, `\"` | Backslash, single quote, double quote |
| `\xHH`, `\XHH` | Byte specified by exactly two hexadecimal digits |

Quoted literals cannot contain an actual line break. Use `\n` to represent
one. A character literal must contain exactly one byte after decoding escapes.
Strings can be empty, but cannot contain a NUL byte. Strings are scalar values
for constant expressions such as comparisons, not tensor dtypes.

```fm
Decimal = 42;
Hexadecimal = 0x2A;
Scale = 1.5e3;
Letter = 'A';
Newline = '\n';
Enabled = true;
Same = "hello" == "hello";
```

### Scalar constants

Assign a scalar expression to a name to define a scalar constant:

```fm
Rows = 2;
Columns = 3;
Elements = Rows * Columns;
Scale = 0.5;
A : float [Rows, Columns];
Scaled = A * Scale;
```

These definitions are evaluated during parsing. `Elements` is the integer 6,
and `Scale` is a floating point scalar. Scalar constants can be used in shape
and stride lists, initializer parameters, axes, and tensor expressions where
compatible values are required.

A scalar constant differs from an initialized constant tensor. `N = 3`
defines an integer scalar. `T : int [1] = ones()` defines a tensor with one
axis and one element. `cast<int>(3)` produces a scalar tensor with rank 0.
Geometry queries such as `shape(A, 0)` also produce scalar constants.

### Indexing and slicing

Indices start at zero. A negative index counts from the end of its axis,
so `-1` selects the last element. An integer index must be within the axis
bounds and removes that axis from the result.

A slice has the form `start:stop:step`. The start is included and the stop is
excluded. The default step is 1, and an explicit step must be nonzero.
For a positive step, omitted bounds select from the beginning to the end.
For a negative step, omitted bounds select from the last element back through
the first. Explicit negative bounds count from the end. Slice bounds are
clipped to the valid range, and a slice can produce an empty tensor.

```fm
A : long [8] = iota();
Last = A[-1];
Middle = A[2:6];
Alternate = A[::2];
Reversed = A[::-1];
Empty = A[4:2];
```

`Last` is a scalar tensor selecting 7. `Middle` selects `2, 3, 4, 5`,
`Alternate` selects `0, 2, 4, 6`, and `Reversed` selects
`7, 6, 5, 4, 3, 2, 1, 0`. `Empty` has shape `[0]`.

Separate axis selections with commas. Any remaining axes are kept in full.
Indices, slice bounds, and steps must be integer expressions evaluable during
parsing.

```fm
A : float [2, 3, 4];
LastPlane = A[-1];
Column = A[:, 1, :];
ReverseColumns = A[:, ::-1, :];
```

`LastPlane` has shape `[3, 4]`, `Column` has shape `[2, 4]`, and
`ReverseColumns` has shape `[2, 3, 4]`. Slices preserve axes and update their
strides and offsets. A negative step produces a negative stride.

### Constant expression behavior

Scalar constant expressions follow the operator precedence described above.
Integer arithmetic stays integer when both operands are integers. Integer
division truncates toward zero, and the remainder has the sign of the dividend.
Combining an integer with a floating point operand in arithmetic produces a
floating point scalar.

```fm
IntegerQuotient = 7 / 2;
NegativeQuotient = -7 / 2;
Remainder = -7 % 2;
RealQuotient = 7 / 2.0;
```

The values are `3`, `-3`, `-1`, and `3.5`, respectively. Division or remainder
by zero is an error when evaluated. Floating point constant computations that
produce a nonfinite value are also rejected.

Boolean scalar expressions use short circuit evaluation. `&&` skips its right
operand when the left is false, and `||` skips its right operand when the left
is true. `^^` evaluates both operands. A scalar conditional expression
evaluates only the selected branch. Every operand must still have valid
syntax and compatible types.

```fm
SkippedAnd = false && (1 / 0 == 0);
SkippedOr = true || (1 / 0 == 0);
Selected = true ? 3 : (1 / 0);
```

These definitions evaluate to `false`, `true`, and `3`. These evaluation rules
apply to scalar constant expressions evaluated by the parser.
