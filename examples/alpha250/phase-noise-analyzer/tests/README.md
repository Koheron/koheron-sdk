# Moving-average regression

From the repository root, with the server external dependencies installed:

```sh
g++ -std=c++20 -Wall -Wextra -Iserver/external_libs \
    examples/alpha250/phase-noise-analyzer/tests/test_moving_averager.cpp \
    -o /tmp/test_moving_averager
/tmp/test_moving_averager
```

The test compares the actual averager with a chronological queue across growing,
shrinking, partially filled and wrapped windows, repeated resizing, and clearing.
It includes the failing case: append 1 and 2 with capacity 2, grow to 4, append 3;
the average must be 2.

This is a host test. It does not access hardware or test concurrent settings changes.
