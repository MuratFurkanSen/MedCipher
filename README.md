# MedCipher

A C++20 academic project for exploring CKKS homomorphic encryption with Microsoft SEAL. The first milestone runs an encrypted addition smoke test; it does not produce benchmark results.

## Synthetic datasets

Install NumPy and run the generator with Python 3:

```sh
python3 -m pip install numpy
python3 scripts/generate_dataset.py
# Optional overrides:
python3 scripts/generate_dataset.py --seed 42 --output-dir data --sizes 100 1000 10000
```

The defaults write `data/patients_100.csv`, `patients_1000.csv`, and `patients_10000.csv`. A single master dataset with the largest requested size is generated; each smaller CSV contains its first N rows. Seed 42 makes repeated runs reproducible with the same NumPy environment and largest size. Changing the largest size can change the generated measurements.

| Column | Meaning | Generated bounds |
| --- | --- | --- |
| `patient_id` | Sequential identifier starting at 1; metadata only | 1–N |
| `age` | Age in years (integer) | 18–90 |
| `heart_rate` | Heart rate in beats per minute | 45–120 |
| `systolic_bp` | Systolic blood pressure in mmHg | 90–180 |
| `diastolic_bp` | Diastolic blood pressure in mmHg | 55–110 |
| `glucose` | Blood glucose in mg/dL | 65–250 |
| `bmi` | Body mass index in kg/m² | 16–45 |

Age, BMI, heart rate, and blood-pressure components use bounded normal distributions; glucose uses a bounded, right-skewed lognormal distribution. Simple age/BMI dependencies add variation, and positive pulse pressure ensures systolic pressure exceeds diastolic pressure. Measurements other than age are rounded to one decimal place. The script validates row counts, finite/nonmissing values, ranges, identifiers, integer ages, and blood-pressure consistency before writing each file. Bounds are generation limits, not clinical reference intervals. All records are synthetic numerical fixtures, contain no real patient data, and are not a validated clinical population model. No encryption or benchmarking is performed by the generator.

## Build and run

Open the project in CLion and build and run the `MedCipher` target using its configured CMake profile. CMake fetches Microsoft SEAL **4.1.2** on the first configure, so network access is required. Optional compression and Intel HEXL dependencies are disabled.

Alternatively, using an existing CMake and C++20 toolchain:

```sh
cmake -S . -B build
cmake --build build
./build/MedCipher
```

The program encrypts 1.25 and 2.5, adds the ciphertexts, decrypts the result, and checks its absolute error against 1e-5. CKKS is approximate, so the decrypted value can vary slightly between runs. A failed check or exception returns a nonzero exit code.

`src/crypto` contains the smoke test; `src/scenarios` and `src/benchmark` are reserved for later work. Generated results belong in `results/` and are ignored by Git.

## Correctness verification

Run `ctest --test-dir cmake-build-debug --output-on-failure` after building the CLion Debug profile (or replace the directory with your build directory). The deterministic inputs have the independently calculated plaintext sum 3.75. The test requires an absolute error below 1e-5 and returns a failing exit code otherwise. Encryption uses randomness, so the exact error varies. This correctness test is not a benchmark.

The example uses a polynomial modulus degree of 8192 and coefficient modulus bit sizes `{60, 40, 40, 60}`. These define the CKKS ciphertext ring and available modulus budget; the 2^40 encoding scale controls precision. The smoke test only adds ciphertexts and does not consume levels through multiplication or rescaling. Evaluation receives ciphertexts and the public context, while decryption happens only after evaluation. These parameters are an example, not a performance or security study.
