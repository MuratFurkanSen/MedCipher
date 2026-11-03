# MedCipher

MedCipher is a C++20 research application that uses Microsoft SEAL 4.1.2 and CKKS to evaluate numerical computations over encrypted synthetic medical measurements. The examples are numerical experiments, not clinical models. CKKS is approximate, and accuracy and timing depend on the parameters, input values and machine.

## Datasets

The included CSV files `data/patients_1K.csv`, `patients_10K.csv` and `patients_100K.csv` contain 1,000, 10,000 and 100,000 synthetic records. Columns are `patient_id`, `age`, `heart_rate`, `systolic_bp`, `diastolic_bp`, `glucose` and `bmi`. Smaller files are prefixes of the largest dataset. Values are synthetic, not patient data; their generation bounds are not clinical reference ranges.

`src/scripts/generate_dataset.py` can produce fixtures using Python and NumPy. It writes to `data/` by default and **overwrites existing files**; use `--output-dir` to preserve the supplied datasets. A fixed seed and master dataset size are required to reproduce a particular set of fixtures.

## Build and run

CMake 3.21 or later, a C++20 compiler, and access to Microsoft SEAL 4.1.2 are required. CMake fetches SEAL on first configuration; optional compression and Intel HEXL dependencies are disabled.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/MedCipher --smoke
./build/MedCipher --scenario1
./build/MedCipher --scenario2
./build/MedCipher --scenario3
./build/MedCipher --scenario4
./build/MedCipher --scenario5
./build/MedCipher --benchmark
```

Exactly one argument is required. Correctness modes return nonzero on failure. Benchmarking requires a Release build and refuses to overwrite existing output files.

## Scenarios

1. **Sum and mean:** encrypted sum and mean of heart rate. Absolute error limits are 0.01 for the sum and 0.0001 for the mean.
2. **Population variance:** encrypted `E[X²] − E[X]²` for heart rate. Absolute error limit is 0.01; small variances can suffer cancellation.
3. **Covariance and Pearson correlation:** encrypted covariance and, where eligible, correlation of heart rate and systolic blood pressure. The owner checks eligibility using a two-pass plaintext reference. The public correlation contract is `4096 ≤ Var(X)Var(Y) ≤ 16384`; zero variance is UNDEFINED, and other values outside the interval are UNSUPPORTED. Covariance remains available in both cases. The evaluator receives ciphertexts, public parameters and keys, record count, and the requested output mode; it receives no plaintext statistics and never decrypts. Eligibility itself is disclosed. The fixed inverse-square-root approximation is `P(v) = (1/√10240) Σ(k=0..8) [(-1)^k binomial(2k,k)/4^k] (v/10240 − 1)^k`. Its ideal relative approximation bound on this domain is approximately 0.00591; ciphertext arithmetic adds error. Covariance and supported correlation use an absolute tolerance of 0.01. Correlations are not clamped, so approximations can slightly exceed ±1.
4. **Weighted score:** `0.25 × heart_rate + 0.30 × systolic_bp + 0.30 × glucose + 0.15 × bmi` per record. Four columns are encrypted independently; the public coefficients multiply ciphertexts and only final scores are decrypted. Absolute tolerance is 1e-5.
5. **Polynomial evaluation:** the owner normalizes glucose using `z = (glucose − 157.5) / 92.5` for glucose in `[65,250]` before encryption. Empty, nonfinite and out-of-domain inputs are rejected. Encrypted balanced powers evaluate `P2 = 1 + z/2 + z²/4`, `P3 = P2 + z³/8`, and `P5 = P3 + z⁴/16 + z⁵/32`. The evaluator never decrypts intermediate values. Outputs are compared with an independent long-double reference using absolute tolerance 1e-5. Ciphertext multiplication depths are 1, 2 and 3, with 2, 3 and 4 rescaling levels respectively.

Input records are SIMD-packed into ciphertext chunks with zero padding; the owner removes final padding from per-record results. The evaluation functions use public context, ciphertexts, evaluation keys and public constants; the owner retains secret keys and decrypts only final results. This is an experimental separation of roles, not a complete authenticated or malicious-client protocol.

## CKKS configurations

Scenarios 1, 2 and 4 use polynomial modulus degree 8192, coefficient-modulus bits `{60,40,40,60}`, scale 2^40 and 4096 slots. Scenarios 3 and 5 use degree 16384, coefficient bits `{60,35,35,35,35,35,35,35,35,35,60}`, scale 2^35 and 8192 slots. The 435-bit chain is within SEAL's 438-bit limit for degree 16384 at its default 128-bit security setting. Parameters and tolerances apply to these experiments, not arbitrary data or clinical validation.

## Benchmarking and results

`--benchmark` runs all five scenarios on all three datasets; polynomial degrees 2, 3 and 5 are measured separately. Each dataset and scenario uses five warmups and 30 measured runs, with a 1,000-iteration plaintext timing batch. The steady-clock pipeline reports plaintext computation, encoding, encryption, homomorphic evaluation, decryption and decoding; encrypted total sums the five encrypted stages. It also reports computation slowdown, encryption/decryption overhead, median, mean and population standard deviation.

Dataset loading, input packing and normalization, key creation, verification, serialization and CSV writing are outside timed stages. Public coefficient encoding and homomorphic arithmetic are included in evaluation. Uncompressed SEAL serialization measures input ciphertext storage; scenarios 4 and 5 also record final output ciphertext storage. Raw storage counts the input doubles, excluding keys and CSV overhead. Error fields summarize measured runs; parameters, compiler, build and available machine metadata accompany measurements. Timings are single-machine observations, and plaintext ratios can be noisy.

After every scenario succeeds, results are published as `results/scenario1.csv` through `results/scenario5.csv`. Runs are staged before publication; failures do not publish incomplete CSVs. Existing files are never overwritten, so archive or remove them before a new run. Generated results are ignored by Git.
