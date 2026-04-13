#!/usr/bin/env python3
"""
ChatGPT-generated code to convert ESP32 binary ADC log file to CSV.

Convert packed ESP32 binary log files to CSV for the packed record format:

    uint64_t header
        bits 63..56 : uint8_t  payload length (bytes)
        bits 55..48 : uint8_t  record type
        bits 47..0  : uint48_t timestamp in microseconds

This version decodes ALL record types generically, assuming each payload consists
of little-endian 32-bit signed integers.

Output modes:
1) widest
   - One CSV file
   - Includes value columns from value_0 up to value_(max_observed_value_count - 1)
   - Shorter payloads leave trailing value_* columns blank

2) split
   - Multiple CSV files
   - One CSV per unique (record_type, payload_length) combination
   - Output file names are derived from the requested output path:
       example.csv
       -> example__type_1__len_32.csv
       -> example__type_2__len_12.csv

Assumptions:
- little-endian header encoding
- payload length field is payload length only
- payload values are int32_t little-endian values
"""

from __future__ import annotations

import argparse
import csv
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

HEADER_FORMAT = "<Q"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
INT32_SIZE = 4


class RecordDecodeError(RuntimeError):
    """Raised when the binary log stream cannot be decoded safely."""


@dataclass(frozen=True)
class Record:
    time_us: int
    record_type: int
    payload_length: int
    values: tuple[int, ...]
    offset: int  # byte offset of header in file


def decode_header(raw_header: int) -> tuple[int, int, int]:
    """Return (length, record_type, time_us) from the packed 64-bit header."""
    length = (raw_header >> 56) & 0xFF
    record_type = (raw_header >> 48) & 0xFF
    time_us = raw_header & 0x0000FFFFFFFFFFFF
    return length, record_type, time_us


def decode_payload_as_int32(payload: bytes, payload_length: int, offset: int) -> tuple[int, ...]:
    """Decode payload bytes into little-endian signed 32-bit integers."""
    if payload_length % INT32_SIZE != 0:
        raise RecordDecodeError(
            f"Record at byte offset {offset} has payload length {payload_length}, "
            "which is not divisible by 4 for int32 decoding."
        )

    value_count = payload_length // INT32_SIZE
    if value_count == 0:
        return ()

    fmt = "<" + ("i" * value_count)
    return struct.unpack(fmt, payload)


def parse_records(input_path: Path) -> list[Record]:
    """Parse the binary log stream into generic records."""
    if not input_path.exists():
        raise FileNotFoundError(f"Input file does not exist: {input_path}")

    records: list[Record] = []
    offset = 0

    with input_path.open("rb") as f_in:
        while True:
            header_offset = offset
            header_bytes = f_in.read(HEADER_SIZE)

            if not header_bytes:
                break

            if len(header_bytes) < HEADER_SIZE:
                print(
                    f"Warning: truncated header at byte offset {header_offset}. "
                    f"Ignoring final {len(header_bytes)} byte(s)."
                )
                break

            (raw_header,) = struct.unpack(HEADER_FORMAT, header_bytes)
            payload_length, record_type, time_us = decode_header(raw_header)
            offset += HEADER_SIZE

            payload = f_in.read(payload_length)
            if len(payload) < payload_length:
                print(
                    f"Warning: truncated payload at byte offset {header_offset}. "
                    f"Expected {payload_length} payload bytes, got {len(payload)}. "
                    "Ignoring incomplete trailing record."
                )
                break

            values = decode_payload_as_int32(payload, payload_length, header_offset)
            offset += payload_length

            records.append(
                Record(
                    time_us=time_us,
                    record_type=record_type,
                    payload_length=payload_length,
                    values=values,
                    offset=header_offset,
                )
            )

    return records


def make_widest_header(max_value_count: int) -> list[str]:
    return [
        "time_us",
        "record_type",
        "payload_length",
        "value_count",
        *[f"value_{i}" for i in range(max_value_count)],
    ]


def record_to_widest_row(record: Record, max_value_count: int) -> list[object]:
    value_cols: list[object] = list(record.values)
    if len(value_cols) < max_value_count:
        value_cols.extend([""] * (max_value_count - len(value_cols)))

    return [
        record.time_us,
        record.record_type,
        record.payload_length,
        len(record.values),
        *value_cols,
    ]


def write_widest_csv(records: list[Record], output_path: Path) -> None:
    max_value_count = max((len(r.values) for r in records), default=0)

    with output_path.open("w", newline="") as f_out:
        writer = csv.writer(f_out)
        writer.writerow(make_widest_header(max_value_count))

        for record in records:
            writer.writerow(record_to_widest_row(record, max_value_count))


def split_output_path(base_output_path: Path, record_type: int, payload_length: int) -> Path:
    stem = base_output_path.stem
    suffix = base_output_path.suffix or ".csv"
    return base_output_path.with_name(
        f"{stem}__type_{record_type}__len_{payload_length}{suffix}"
    )


def write_split_csvs(records: list[Record], base_output_path: Path) -> list[Path]:
    grouped: dict[tuple[int, int], list[Record]] = defaultdict(list)
    for record in records:
        grouped[(record.record_type, record.payload_length)].append(record)

    written_paths: list[Path] = []

    for (record_type, payload_length), group in sorted(grouped.items()):
        out_path = split_output_path(base_output_path, record_type, payload_length)
        value_count = payload_length // INT32_SIZE

        with out_path.open("w", newline="") as f_out:
            writer = csv.writer(f_out)
            writer.writerow([
                "time_us",
                "record_type",
                "payload_length",
                "value_count",
                *[f"value_{i}" for i in range(value_count)],
            ])

            for record in group:
                writer.writerow([
                    record.time_us,
                    record.record_type,
                    record.payload_length,
                    len(record.values),
                    *record.values,
                ])

        written_paths.append(out_path)

    return written_paths


def print_summary(records: list[Record], input_path: Path, output_path: Path, layout: str) -> None:
    type_counts: dict[int, int] = defaultdict(int)
    combo_counts: dict[tuple[int, int], int] = defaultdict(int)

    for record in records:
        type_counts[record.record_type] += 1
        combo_counts[(record.record_type, record.payload_length)] += 1

    print("Done.")
    print(f"Input:              {input_path}")
    print(f"Input size:         {input_path.stat().st_size} bytes")
    print(f"Requested output:   {output_path}")
    print(f"Layout:             {layout}")
    print(f"Total records:      {len(records)}")

    if records:
        max_payload_length = max(r.payload_length for r in records)
        max_value_count = max(len(r.values) for r in records)
        print(f"Max payload length: {max_payload_length} byte(s)")
        print(f"Max value count:    {max_value_count}")
    else:
        print("Max payload length: 0 byte(s)")
        print("Max value count:    0")

    print("Record counts by type:")
    for record_type in sorted(type_counts):
        print(f"  type {record_type}: {type_counts[record_type]}")

    print("Record counts by (type, payload_length):")
    for (record_type, payload_length) in sorted(combo_counts):
        print(f"  type {record_type}, len {payload_length}: {combo_counts[(record_type, payload_length)]}")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert packed ESP32 binary log file to CSV for all record types."
    )
    parser.add_argument("input_path", type=Path, help="Input binary log file")
    parser.add_argument("output_path", type=Path, help="Output CSV path (or base path for split mode)")
    parser.add_argument(
        "--layout",
        choices=("widest", "split"),
        default="widest",
        help=(
            "Output layout: "
            "'widest' = one CSV with columns up to the largest observed payload length; "
            "'split' = one CSV per unique (record_type, payload_length)."
        ),
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    if argv is None:
        argv = sys.argv[1:]

    try:
        args = parse_args(argv)
        records = parse_records(args.input_path)

        if args.layout == "widest":
            write_widest_csv(records, args.output_path)
            print_summary(records, args.input_path, args.output_path, args.layout)

        elif args.layout == "split":
            written_paths = write_split_csvs(records, args.output_path)
            print_summary(records, args.input_path, args.output_path, args.layout)
            print("Generated CSV files:")
            for path in written_paths:
                print(f"  {path}")

        else:
            raise ValueError(f"Unsupported layout: {args.layout}")

    except Exception as exc:
        print(f"Error: {exc}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())