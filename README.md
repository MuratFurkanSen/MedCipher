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

The defaults write `data/patients_100.csv`, `patients_1K.csv`, and `patients_10K.csv`. A single master dataset with the largest requested size is generated; each smaller CSV contains its first N rows. Seed 42 makes repeated runs reproducible with the same NumPy environment and largest size. Changing the largest size can change the generated measurements.

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

## Scenario 4: weighted numerical scoring

Run `MedCipher --scoring-check` for correctness, or `MedCipher --benchmark-scoring` in Release mode to overwrite `results/scenario4.csv`. This scenario uses the original degree-8192 configuration, `{60,40,40,60}` coefficient modulus, scale 2^40, and 4096 slots. No rotation or relinearization keys are needed. It is not mixed with the stronger Scenario 3 measurements.

The fixed public weights are heart_rate **0.25**, systolic_bp **0.30**, glucose **0.30**, and bmi **0.15**. These weights define an illustrative numerical experiment, **not a clinically validated medical risk model**.

All four columns are encrypted independently in contiguous, zero-padded chunks. The evaluator receives only ciphertexts, the public SEAL context, chunk count, scale and fixed weights. Each ciphertext is multiplied by a plaintext-encoded public weight and rescaled once; equal levels/scales allow addition of the four weighted terms. The owner decrypts only the final score ciphertexts, decodes all slots and removes final padding. The returned score vector contains exactly one score per input record, in input order; no private measurement or intermediate decryption is exposed to the evaluator.

Correctness compares every score with an independent long-double weighted reference expressed as `(25*x1 + 30*x2 + 30*x3 + 15*x4)/100`. The fixed absolute tolerance is **1e-5**, well below the 0.1-unit dataset resolution and the smallest 0.015 score change from a one-step input change. Maximum/mean absolute error and maximum relative error are reported. For an exactly zero reference, the relative-error field uses absolute error instead. Tests include all three datasets, known independent outputs for isolated columns, signed/zero values and lengths 1, 4095, 4096, 4097 and 8193.

The benchmark reuses the existing steady-clock pipeline, 5 warmups, 30 repetitions and 1000-iteration plaintext batch. Reference-vector allocation, CSV loading, column assembly/padding, setup/keys, verification, serialization and output are outside the measured stages. Encoding covers input encoding; public-weight encoding, multiplication, rescaling and addition belong to homomorphic computation. Decryption covers final score ciphertexts only. Decoding covers all their slots and concatenation; trimming padding is outside the stage. Input storage counts all four columns and every encrypted chunk. Output storage is the sum of actual uncompressed SEAL serializations of final score ciphertexts, measured outside timed stages; the output ciphertext count is also recorded.

In the summary CSV, maximum errors are maxima over all records and all 30 measured repetitions; mean absolute error averages over those same records and repetitions. Timing statistics retain the existing median/mean/population-standard-deviation columns. Per-record results are computed and verified in memory; this file contains experimental summaries, not individual scores. Existing Scenario 1–3 result files are preserved. Scenario 5 is described below.


## Scenario 5: bounded polynomial evaluation

Run `MedCipher --polynomial-check` for correctness or `MedCipher --benchmark-polynomial` in Release mode to write `results/scenario5.csv`. Each degree has a separate 5-warmup/30-measured-run experiment for each dataset. The CSV contains 81 metric rows (3 degrees × 3 datasets × 9 metrics); degrees are never pooled into a performance result. Earlier result files are not overwritten.

The owner uses the fixed public transformation **z = (glucose − 157.5) / 92.5**, mapping the documented synthetic glucose bounds [65,250] to [-1,1]. Empty, nonfinite and out-of-domain inputs are rejected, not clamped. Normalization is performed before encryption, outside timed stages, just like input packing; both plaintext and encrypted evaluation receive the same normalized values. It uses no dataset-derived statistics. The encrypted evaluator receives only normalized ciphertexts, public context/degree/scale, and a relinearization key. It never receives plaintext measurements or the secret key and never decrypts. The owner decrypts only final outputs, one value per record.

The fixed numerical examples are:

- P2(z) = 1 + z/2 + z²/4.
- P3(z) = P2(z) + z³/8.
- P5(z) = P3(z) + z⁴/16 + z⁵/32.

These are **not clinically validated medical models**. They are bounded and positive on [-1,1]: all selected outputs lie between 1/32 and 63/32 by a conservative coefficient-sum bound. Consequently relative errors are well-defined, including at z=0. The fixed absolute tolerance is **1e-5**, about five parts per million of the maximum output; it is an experimental precision target and was not changed after testing. References use independent long-double Horner evaluation in a separate translation unit; encrypted evaluation uses balanced powers.

All degrees reuse the existing stronger configuration: polynomial modulus degree **16384**, coefficient bits **{60,35,35,35,35,35,35,35,35,35,60}** (435 total, below SEAL's 438-bit limit at its default 128-bit security setting), scale **2^35**, and 8192 slots. This chain is intentionally reused and overprovisioned for these smaller circuits; these experiments compare degrees under identical parameters, not minimal-cost parameter sets. No earlier scenario configuration changes.

The evaluator builds z², z³, and, when needed, z⁴ and z⁵ with balanced ciphertext multiplications, relinearizing and rescaling each result. Lower-level operands are aligned by modulus switching. Each power is multiplied by its public coefficient with an encoding scale calculated from the actual rescaling prime, giving a shared output scale. Weighted powers are added, then the public constant term is added. No extra multiplications are inserted to inflate depth.

| Degree | Ciphertext multiplication depth | Rescaling levels consumed | Input → output chain index | Ciphertext multiplications per chunk | Total rescale calls per chunk |
| --- | --- | --- | --- | --- | --- |
| 2 | 1 | 2 | 9 → 7 | 1 | 3 |
| 3 | 2 | 3 | 9 → 6 | 2 | 5 |
| 5 | 3 | 4 | 9 → 5 | 4 | 9 |

Depths and chain indices are checked against the actual ciphertext context at runtime. CSV fields record those observations, total ciphertext multiplications/rescales/modulus switches across all chunks, and the final scale. Scale/level mismatches at addition raise errors. Public coefficient encoding, alignment, arithmetic, relinearization, rescaling and trace counters are included in homomorphic-computation timing. Input normalization/validation, padding, reference allocation, setup/keys, verification, serialization, console output and file writing are excluded. The plaintext stage evaluates all normalized inputs in the same 1000-iteration batch convention used previously. All final slots are decoded; final padding is discarded outside the decode stage.

Input and output storage use actual uncompressed SEAL serialization sizes. Raw input storage is N × sizeof(double), representing one numerical column. Maximum errors cover all records and measured runs; mean absolute error is averaged across those records and runs. Tests include every dataset and degree, independently calculated boundary outputs at z=-1/0/1, signed inputs, lengths 1/4097/8191/8192/8193, padding and invalid-domain rejection, plus all previous scenario regressions. Ciphertext noise and machine scheduling vary between executions; reported results are measurements of this configuration, not a clinical or universal precision guarantee.
