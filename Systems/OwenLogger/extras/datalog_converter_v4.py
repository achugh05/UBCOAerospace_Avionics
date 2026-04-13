#!/usr/bin/env python3
"""
ChatGPT-generated code to convert ESP32 binary ADC log file to CSV.

Convert packed ESP32 binary log files to CSV.

Packed record format:
    uint64_t header
        bits 63..56 : uint8_t  payload length (bytes)
        bits 55..48 : uint8_t  record type
        bits 47..0  : uint48_t timestamp in microseconds

Payloads are decoded generically as little-endian signed 32-bit integers.

This version adds a user-editable processing step:
- By default, CSV output contains processed values.
- Raw integer CSV output is still available with --output-mode raw.
- The default processing function converts each signed integer reading to
  millivolts using a configurable full-scale reference and PGA.

Expected workflow for custom processing:
1) Edit PROCESSING_CONFIG_BY_TYPE for per-record-type settings.
2) Modify process_record_values() if you want custom engineering units,
   derived channels, calibration, or filtering.
"""

from __future__ import annotations

import argparse
import csv
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

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


@dataclass(frozen=True)
class ProcessingConfig:
    """User-editable processing parameters for a record type."""
    code_bits: int = 24
    full_scale_mv: float = 2048.0
    pga: float = 1.0


# ---------------------------------------------------------------------------
# USER-EDITABLE PROCESSING CONFIGURATION
# ---------------------------------------------------------------------------
#
# Add entries here to override settings for specific record types.
# Example:
#   56: ProcessingConfig(code_bits=24, full_scale_mv=2500.0, pga=1.0)
#   2:  ProcessingConfig(code_bits=24, full_scale_mv=2048.0, pga=128.0)
#

ADC0_TYPE_ID = 56
ADC1_TYPE_ID = 2

DEFAULT_PROCESSING_CONFIG = ProcessingConfig()
PROCESSING_CONFIG_BY_TYPE: dict[int, ProcessingConfig] = {
    ADC0_TYPE_ID: ProcessingConfig(code_bits=24, full_scale_mv=5000.0, pga=1.0),
    ADC1_TYPE_ID: ProcessingConfig(code_bits=24, full_scale_mv=3000.0, pga=64.0)
}


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


# ---------------------------------------------------------------------------
# USER-EDITABLE DATA PROCESSING STEP
# ---------------------------------------------------------------------------

def convert_signed_code_to_millivolts(
    code: int,
    *,
    code_bits: int = 24,
    full_scale_mv: float = 2048.0,
    pga: float = 1.0,
) -> float:
    """
    Minimal default conversion from signed ADC code to millivolts.

    The conversion assumes a signed bipolar ADC transfer function where the raw
    code spans approximately ±(full_scale_mv / pga).

    This is intentionally simple so users can replace it with sensor-specific
    processing later.
    """
    if code_bits < 2:
        raise ValueError(f"code_bits must be >= 2, got {code_bits}")
    if pga == 0:
        raise ValueError("pga must not be 0")

    positive_full_scale_code = (1 << (code_bits - 1)) - 1
    input_full_scale_mv = full_scale_mv / pga
    return (code / positive_full_scale_code) * input_full_scale_mv


def process_record_values(record: Record) -> list[float]:
    """
    Default user-modifiable processing hook.

    Edit this function to add your own record-type-specific conversion, sensor
    calibration, filtering, scaling, unit conversions, or derived channels.
    """
    cfg = PROCESSING_CONFIG_BY_TYPE.get(record.record_type, DEFAULT_PROCESSING_CONFIG)

    # If record type is not recognized, use raw values
    if cfg is DEFAULT_PROCESSING_CONFIG:
        return record.values
    
    return [
        convert_signed_code_to_millivolts(
            value,
            code_bits=cfg.code_bits,
            full_scale_mv=cfg.full_scale_mv,
            pga=cfg.pga,
        )
        for value in record.values
    ]


# ---------------------------------------------------------------------------
# CSV WRITING
# ---------------------------------------------------------------------------

def make_header(max_value_count: int, output_mode: str) -> list[str]:
    value_prefix = "raw_value" if output_mode == "raw" else "processed_value"
    base = [
        "time_us",
        "record_type",
        "payload_length",
        "value_count",
    ]
    if output_mode == "processed":
        base.append("processed_units")
    return base + [f"{value_prefix}_{i}" for i in range(max_value_count)]


def get_output_values(record: Record, output_mode: str) -> list[object]:
    if output_mode == "raw":
        return list(record.values)
    if output_mode == "processed":
        return list(process_record_values(record))
    raise ValueError(f"Unsupported output mode: {output_mode}")


def record_to_row(record: Record, max_value_count: int, output_mode: str) -> list[object]:
    value_cols = get_output_values(record, output_mode)
    if len(value_cols) < max_value_count:
        value_cols.extend([""] * (max_value_count - len(value_cols)))

    row: list[object] = [
        record.time_us,
        record.record_type,
        record.payload_length,
        len(record.values),
    ]
    if output_mode == "processed":
        row.append("mV")
    row.extend(value_cols)
    return row


def write_widest_csv(records: list[Record], output_path: Path, output_mode: str) -> None:
    max_value_count = max((len(r.values) for r in records), default=0)

    with output_path.open("w", newline="") as f_out:
        writer = csv.writer(f_out)
        writer.writerow(make_header(max_value_count, output_mode))

        for record in records:
            writer.writerow(record_to_row(record, max_value_count, output_mode))


def split_output_path(base_output_path: Path, record_type: int, payload_length: int) -> Path:
    stem = base_output_path.stem
    suffix = base_output_path.suffix or ".csv"
    return base_output_path.with_name(
        f"{stem}__type_{record_type}__len_{payload_length}{suffix}"
    )


def write_split_csvs(records: list[Record], base_output_path: Path, output_mode: str) -> list[Path]:
    grouped: dict[tuple[int, int], list[Record]] = defaultdict(list)
    for record in records:
        grouped[(record.record_type, record.payload_length)].append(record)

    written_paths: list[Path] = []

    for (record_type, payload_length), group in sorted(grouped.items()):
        out_path = split_output_path(base_output_path, record_type, payload_length)
        value_count = payload_length // INT32_SIZE

        with out_path.open("w", newline="") as f_out:
            writer = csv.writer(f_out)
            writer.writerow(make_header(value_count, output_mode))

            for record in group:
                writer.writerow(record_to_row(record, value_count, output_mode))

        written_paths.append(out_path)

    return written_paths


def print_summary(
    records: list[Record],
    input_path: Path,
    output_path: Path,
    layout: str,
    output_mode: str,
) -> None:
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
    print(f"Output mode:        {output_mode}")
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
    parser.add_argument(
        "--output-mode",
        choices=("processed", "raw"),
        default="processed",
        help=(
            "CSV contents: 'processed' applies process_record_values() and is the default; "
            "'raw' writes decoded signed integers directly."
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
            write_widest_csv(records, args.output_path, args.output_mode)
            print_summary(records, args.input_path, args.output_path, args.layout, args.output_mode)

        elif args.layout == "split":
            written_paths = write_split_csvs(records, args.output_path, args.output_mode)
            print_summary(records, args.input_path, args.output_path, args.layout, args.output_mode)
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
