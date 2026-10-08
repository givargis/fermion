# Tests

Build the shared library from the repository root:

```sh
make -C libfm
```

Run all tests:

```sh
python3 tests/run.py
```

From this directory, use `python3 run.py`. The runner finds its files relative
to the script, so it also works from other directories.

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
