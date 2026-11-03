#!/usr/bin/env python3
"""Generate reproducible synthetic measurements for numerical experiments."""

import argparse
import csv
from pathlib import Path

import numpy as np


BOUNDS = {
    "age": (18, 90),
    "heart_rate": (45, 120),
    "systolic_bp": (90, 180),
    "diastolic_bp": (55, 110),
    "glucose": (65, 250),
    "bmi": (16, 45),
}


def generate_dataset(size, seed):
    rng = np.random.default_rng(seed)
    age = np.clip(np.rint(rng.normal(50, 17, size)), 18, 90).astype(int)
    bmi = np.round(np.clip(rng.normal(26.5, 4.5, size), 16, 45), 1)
    heart_rate = np.round(
        np.clip(rng.normal(72 + 0.4 * (bmi - 25), 10, size), 45, 120), 1
    )
    diastolic = np.round(
        np.clip(rng.normal(77 + 0.25 * (bmi - 25), 8, size), 55, 110), 1
    )
    # A positive pulse pressure preserves ordering even after rounding/clipping.
    pulse_pressure = np.clip(
        rng.normal(43 + 0.25 * (age - 50), 8, size), 25, 70
    )
    systolic = np.round(np.clip(diastolic + pulse_pressure, 90, 180), 1)
    # Right-skewed glucose includes a modest upper tail without diagnostic labels.
    glucose = np.round(
        np.clip(rng.lognormal(np.log(95) + 0.008 * (bmi - 25), 0.22, size), 65, 250),
        1,
    )
    return {
        "patient_id": np.arange(1, size + 1),
        "age": age,
        "heart_rate": heart_rate,
        "systolic_bp": systolic,
        "diastolic_bp": diastolic,
        "glucose": glucose,
        "bmi": bmi,
    }


def validate_dataset(data, expected_size):
    for name, values in data.items():
        if len(values) != expected_size or not np.all(np.isfinite(values)):
            raise ValueError(f"{name}: incorrect row count or missing/nonfinite values")
    if not np.array_equal(data["patient_id"], np.arange(1, expected_size + 1)):
        raise ValueError("patient_id must be sequential, starting at 1")
    if not np.all(data["age"] == np.floor(data["age"])):
        raise ValueError("age must be integral")
    for name, (lower, upper) in BOUNDS.items():
        if np.any((data[name] < lower) | (data[name] > upper)):
            raise ValueError(f"{name}: values outside [{lower}, {upper}]")
    if not np.all(data["systolic_bp"] > data["diastolic_bp"]):
        raise ValueError("systolic pressure must exceed diastolic pressure")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument(
        "--output-dir", type=Path, default=Path(__file__).resolve().parents[2] / "data"
    )
    parser.add_argument("--sizes", type=int, nargs="+", default=[1000, 10000, 100000])
    args = parser.parse_args()
    if args.seed < 0:
        parser.error("--seed must be nonnegative")
    if any(size <= 0 for size in args.sizes):
        parser.error("--sizes must contain positive integers")

    master = generate_dataset(max(args.sizes), args.seed)
    validate_dataset(master, max(args.sizes))
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for size in sorted(set(args.sizes)):
        subset = {name: values[:size] for name, values in master.items()}
        validate_dataset(subset, size)
        name = f"{size // 1000}K" if size in (1000, 10000, 100000) else str(size)
        path = args.output_dir / f"patients_{name}.csv"
        with path.open("w", newline="", encoding="utf-8") as output:
            writer = csv.writer(output)
            writer.writerow(subset)
            writer.writerows(zip(*subset.values()))
        print(f"Wrote {path} ({size} rows; validation passed)")


if __name__ == "__main__":
    main()
