"""The ARM server advertises uint64_t vector elements as unsigned long long."""
import struct

import numpy as np
import pytest

from koheron.koheron import build_payload


@pytest.mark.parametrize("cpp_type,dtype,values,format", [
    ("unsigned long long", "uint64", [1, 2**63 + 123, 2**64 - 1], "<3Q"),
    ("long long", "int64", [-2**63, -1, 2**63 - 1], "<3q"),
])
def test_64_bit_vector_payload(cpp_type, dtype, values, format):
    payload = build_payload(
        [{"type": f"std::vector<{cpp_type}>"}],
        [np.array(values, dtype=dtype)],
    )
    # Vector length is big-endian; the ARM vector body preserves raw bytes.
    assert payload == struct.pack(">I", 24) + struct.pack(format, *values)
