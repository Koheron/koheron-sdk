"""Numeric literals shared by memory-map validation and code generation."""
from __future__ import annotations

import re
from typing import Any


def parse_int(value: Any) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if not isinstance(value, str):
        return None
    try:
        return int(value.replace('_', '').strip(), 0)
    except ValueError:
        return None


def parse_range(value: Any) -> int | None:
    if isinstance(value, str):
        text = value.replace('_', '').strip()
        if any(char.isspace() for char in text):
            return None
        match = re.fullmatch(r'(.+?)([KMGkmg]?)', text)
        if not match:
            return None
        literal, suffix = match.groups()
        number = int(literal) if literal.isdecimal() else parse_int(literal)
        if number is None:
            return None
        return number * {'': 1, 'K': 1024, 'M': 1024**2, 'G': 1024**3}[suffix.upper()]
    return parse_int(value)


def size_bytes(value: Any) -> int:
    size = parse_range(value)
    if size is None or size <= 0:
        raise ValueError(f'Invalid memory range: {value!r}; expected a positive byte count or K/M/G size')
    return size


def range_literal(value: Any) -> str:
    """Preserve Vivado's unit notation while normalizing case and separators."""
    size = size_bytes(value)
    if isinstance(value, str):
        suffix = value.strip()[-1].upper()
        if suffix in 'KMG':
            return f'{size // {"K": 1024, "M": 1024**2, "G": 1024**3}[suffix]}{suffix}'
    return str(size)
