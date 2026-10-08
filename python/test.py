#
# Copyright (c) 2025-2026 Tony Givargis
# University of California, Irvine
# test.py
#

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

print(ir)
