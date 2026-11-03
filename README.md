# MedCipher

A C++20 academic project for exploring CKKS homomorphic encryption with Microsoft SEAL. The application runs and benchmarks an encrypted sum and mean workload.

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

Run `MedCipher --smoke` to encrypt 1.25 and 2.5, add the ciphertexts, decrypt the result, and check its absolute error against 1e-5. Running without arguments checks all three heart-rate datasets. CKKS is approximate, so the decrypted value can vary slightly between runs. A failed check or exception returns a nonzero exit code.

`src/crypto` contains SEAL setup and the smoke test, `src/scenarios` contains the sum and mean workload, and `src/benchmark` contains timing and results recording. Generated results belong in `results/` and are ignored by Git.

## Correctness verification

Run `ctest --test-dir cmake-build-debug --output-on-failure` after building the CLion Debug profile (or replace the directory with your build directory). The deterministic inputs have the independently calculated plaintext sum 3.75. The test requires an absolute error below 1e-5 and returns a failing exit code otherwise. Encryption uses randomness, so the exact error varies. This correctness test is not a benchmark.

The example uses a polynomial modulus degree of 8192 and coefficient modulus bit sizes `{60, 40, 40, 60}`. These define the CKKS ciphertext ring and available modulus budget; the 2^40 encoding scale controls precision. The smoke test only adds ciphertexts and does not consume levels through multiplication or rescaling. Evaluation receives ciphertexts and the public context, while decryption happens only after evaluation. These parameters are an example, not a performance or security study.

## Encrypted sum and mean

The workload packs up to 4096 heart rates per ciphertext, zero-pads unused slots, and uses power-of-two rotations and additions to sum each chunk. It combines encrypted chunk sums and multiplies by an encoded reciprocal for the mean; rescaling after multiplication brings the scale back near 2^40. Only the final sum and mean are decrypted. The same 8192 degree and `{60, 40, 40, 60}` coefficient modulus as the smoke test provide 4096 slots and a level for this multiplication.

The correctness checks require absolute errors at most 0.01 for the sum and 0.0001 for the mean, much smaller than the 0.1-unit input resolution and sufficient to allow CKKS approximation and rescaling error. Relative errors are also displayed. These thresholds apply to the provided datasets; they are not general precision guarantees. CSV input is intended for the provided simple numeric, unquoted files.

## Scenario 1 benchmark

Configure a Release profile in CLion, build it, and run `MedCipher --benchmark`. The command refuses non-Release builds. It performs five warmups and 30 measured runs per dataset, reusing the context and keys, and overwrites `results/scenario1.csv`. Run correctness checks separately with `ctest --test-dir <release-build-directory> --output-on-failure`. Generated result files are ignored by Git.

All times use `steady_clock` and are milliseconds. The plaintext stage times a batch of 1,000 sum-and-mean computations and divides by 1,000 to reduce clock-resolution effects; the last results are consumed by the correctness check. Each iteration calls a function in a separate translation unit (without link-time optimization). Encoding covers encoding pre-padded input chunks; encryption covers encryption of those plaintexts, including construction of SEAL encryptors. Homomorphic computation includes copying chunks, rotating and adding slots, combining chunks, encoding the reciprocal, multiplying, and rescaling. Decryption covers both final ciphertexts and construction of decryptors; decoding covers both final plaintexts. The encrypted total is the per-run sum of encoding, encryption, computation, decryption and decoding. Encryption/decryption overhead is the per-run sum of encryption and decryption times. Computation slowdown is the per-run homomorphic computation time divided by plaintext sum-and-mean time. For each metric the CSV records the median, mean, and population standard deviation across runs. Medians of separate stages need not add to the median total.

CSV loading, context/key creation, input chunk preparation and padding, input ciphertext serialization, correctness comparisons, console output, and CSV output are excluded from timed regions. Input ciphertext storage is the actual uncompressed SEAL serialization size summed across chunks; raw storage is the number of input doubles multiplied by `sizeof(double)`. The storage ratio excludes keys, result ciphertexts, and CSV file overhead. Error columns report the maximum absolute and relative errors across measured runs; printed result values in the CSV come from the last run. Every warmup and measured run must pass the correctness tolerances.

The CSV records the running OS/kernel and architecture, CPU model when accessible, build configuration, compiler and SEAL version. Plaintext timing for small inputs remains sensitive to clock resolution and function-call overhead, making slowdown ratios noisy; this is a single-machine run, not a controlled cross-machine study. Serialization is performed between encryption and evaluation, outside their measured intervals, and may affect cache state. The implementation uses the existing parameters and reports no security or performance claims beyond the recorded experiment.

## Scenario 2: population variance

Run `MedCipher --variance-check` for correctness and `MedCipher --benchmark-variance` in Release mode to write `results/scenario2.csv`. It uses the same warmup, repetitions, timing pipeline, storage definition and environment metadata as Scenario 1. Plaintext variance uses the same E[X²] − E[X]² expression and the same 1,000-iteration timing batch. Only the final variance is decrypted. The automated checks include independently computed dataset references, a constant vector, and a 4,097-element boundary case.

Each padded input ciphertext is squared and relinearized, rescaled, and rotated to produce encrypted sums of squares. In parallel, the unsquared inputs are summed and multiplied by 1/N; that encrypted mean is squared and relinearized. A second multiplication by 1/N computes E[X²]. Both paths rescale twice and reach the same level. The primes used for rescaling are near 2^40, so the resulting scales differ slightly: the evaluator checks that they differ by less than 1e-5 before matching their scale metadata for subtraction. This approximation contributes systematic error and the subtraction can amplify relative error when variance is small. The correctness check accepts absolute error up to 0.01 and a variance no smaller than -0.01; it is suited to these fixtures, not a general precision guarantee.

## Scenario 3: statistical relationships

`MedCipher --relationships` runs correctness and eligibility tests. `MedCipher --benchmark-strong` runs all three scenarios using degree 16384, coefficient-modulus bits `{60,35,35,35,35,35,35,35,35,35,60}`, and scale 2^35. The 435-bit chain is below SEAL's 438-bit limit for degree 16384 at its default 128-bit security level. It provides nine rescaling levels and 8192 slots. Results go to `scenario1_strong.csv`, `scenario2_strong.csv`, and `scenario3.csv`, preserving the original CSVs. The initial 390-bit, 2^30 prototype missed the unchanged covariance tolerance, so the scale and intermediate primes were increased. No tolerance was relaxed.

The public correlation contract is **4096 <= Var(X)Var(Y) <= 16384**. This is an explicit restricted numerical domain, not a medical guarantee. It was fixed before inspecting dataset statistics. The data owner screens eligibility locally using an independent two-pass, long-double reference computation. The evaluator receives only ciphertexts, public context, evaluation keys, public record count, and a request to compute covariance alone or both outputs. No means, variances, normalization factors, or plaintext measurements are passed to it. It never decrypts. Only the owner decrypts final covariance and, when eligible, final correlation. Zero-variance inputs are marked UNDEFINED and other out-of-domain inputs UNSUPPORTED; covariance is still calculated homomorphically. Unavailable correlation/error fields are `nan` in the CSV, not numerical successes.

For encrypted variance product v, set u = v/10240 - 1. The inverse-square-root approximation is

```
P(v) = (1/sqrt(10240)) * sum(k=0..8, (-1)^k * binomial(2*k,k)/4^k * u^k)
```

All coefficients and transformations are public and dataset-independent. On the contract, |u| <= 0.6. The binomial-series tail gives relative inverse-square-root error at most `sqrt(1.6) * (binomial(18,9)/4^9) * 0.6^9 / 0.4`, approximately 0.00591. Cauchy–Schwarz therefore bounds the ideal-arithmetic correlation error by the same value. The remaining error budget must cover CKKS arithmetic; tests use unchanged absolute limits of 0.01 for covariance and correlation. This is not a guarantee for arbitrary inputs or malicious clients.

The evaluator computes encrypted sums, products and second moments using rotations and additions. Covariance/variance take two rescaling levels, the variance product takes a third, its affine normalization a fourth, balanced powers through degree eight reach level seven, coefficient products level eight, and final covariance multiplication level nine. The reusable constant multiplier encodes coefficients using the actual next modulus prime so that each rescale reaches its intended scale, avoiding the larger systematic error from simply overwriting mismatched scales.

Scenario 3 uses the same pipeline, five warmups, 30 measured runs, and 1000-iteration plaintext timing batch. Two input columns count toward raw bytes and serialized input bytes. Covariance and correlation share their encrypted moments and are timed jointly; UNSUPPORTED/UNDEFINED rows time covariance-only evaluation and must not be compared as complete correlation workloads. Owner eligibility checking and independent plaintext references do not enter the encrypted timing. Padding and column assembly are outside timed stages. The zero-variance and domain decisions disclose eligibility, not statistics, to the evaluator.

Tests cover the three supplied datasets (explicitly reporting unsupported cases), positive and negative perfect correlation at the public domain boundaries and midpoint, 4097 and 8193 records, constant input, and both low/high out-of-domain inputs. The 8193 fixture crosses the new 8192-slot boundary. Dataset generators and CSV inputs are unchanged. Scenario 4 is not implemented.
