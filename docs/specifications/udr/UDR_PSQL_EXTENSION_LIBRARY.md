# UDR Extension Library for PSQL

Status: Draft (Research + Design). This document analyzes current PSQL
capabilities and proposes UDR-based extension packs to add scientific,
statistical, and financial functionality without expanding the core engine.

## 1. Scope and Purpose

PSQL is a strong procedural language for data manipulation and reporting, but
it intentionally focuses on database-centric operations. This spec identifies
gaps relative to Python and other analytical ecosystems and defines UDR packs
that can be installed to provide those features safely.

## 2. Current PSQL Baseline (Summary)

**Language features (core PSQL):**
- Blocks, variables, constants, SET/SELECT INTO
- Control flow: IF, CASE (statement), LOOP/WHILE/FOR
- Cursor operations, scrollable cursors, cursor handle passing
- TRY/EXCEPT, SQLSTATE handling, user exceptions
- EXECUTE BLOCK, autonomous transactions, block-level triggers

**Built-in function coverage (core subset):**
- Temporal: NOW, CURRENT_DATE/TIME, DATE_ADD/SUB, DATE_DIFF
- String: LTRIM, RTRIM, CONCAT, CONCAT_WS
- JSON/JSONB functions
- Array helpers (JSON array strings, ARRAY_AGG, ARRAY_POSITION, ARRAY_SLICE)
- Spatial (basic ST_* functions)

See:
- `parser/05_PSQL_PROCEDURAL_LANGUAGE.md`
- `core/INTERNAL_FUNCTIONS.md`
- `ddl/DDL_USER_DEFINED_RESOURCES.md`

## 3. Gap Analysis (Python + Common Analytics)

PSQL currently lacks or has limited equivalents for:

**Scientific math**
- Special functions (gamma, erf, Bessel, zeta)
- Vectorized numerical ops (element-wise arrays)

**Statistics**
- Descriptive stats (median, percentile, skew, kurtosis)
- Distributions (pdf/cdf/quantile, sampling)
- Hypothesis tests (t-test, chi-square, KS)

**Linear algebra**
- Matrix operations, decomposition (SVD, eigen), solving systems

**Time series / signal**
- Rolling metrics (EMA, WMA), seasonal decomposition
- FFT/convolution, filtering

**Financial**
- NPV, IRR, XNPV/XIRR, amortization schedules
- Day-count conventions, yield curves, option pricing

**Text analytics**
- Levenshtein, trigram similarity, Soundex/Metaphone

These are widely available in Python (NumPy/SciPy/Pandas),
R, MATLAB, Excel, and in analytics-focused DB extensions (PostgreSQL,
Oracle analytic packages).

## 4. UDR Extension Model

**Principle:** provide advanced functionality through UDR packs so the core
engine remains minimal and secure.

**Naming convention:**
- Schema prefix per pack (e.g., `sb_math.*`, `sb_stats.*`, `sb_finance.*`)
- Functions are deterministic where possible and declared IMMUTABLE/STABLE.

**Data interchange:**
- Numeric arrays are passed as JSON array strings (aligned with current array
  semantics).
- Table-returning functions (SETOF) are used for structured outputs
  (e.g., amortization schedules).

**Security:**
- UDR packs are signed and explicitly installed.
- Default resource limits and no network access unless explicitly granted.

## 5. Proposed UDR Packs

### 5.1 sb_math (Scientific Math)
**Purpose:** parity with Python math + SciPy special functions.

Candidate functions:
- `sb_math.gamma(x)`, `sb_math.lgamma(x)`, `sb_math.erf(x)`, `sb_math.erfc(x)`
- `sb_math.bessel_j(n, x)`, `sb_math.bessel_y(n, x)`
- `sb_math.hypot(x, y)`, `sb_math.expm1(x)`, `sb_math.log1p(x)`

External libs: Boost.Math (preferred), Cephes (optional).

### 5.2 sb_stats (Statistics)
**Purpose:** descriptive stats, distributions, and tests.

Candidate functions (scalar):
- `sb_stats.mean(json_array)`, `sb_stats.median(json_array)`
- `sb_stats.var(json_array)`, `sb_stats.stddev(json_array)`
- `sb_stats.quantile(json_array, q)`
- `sb_stats.corr(x_array, y_array)`

Candidate functions (distribution):
- `sb_stats.norm_pdf(x, mean, std)`, `sb_stats.norm_cdf(x, mean, std)`
- `sb_stats.norm_ppf(p, mean, std)` (inverse cdf)
- `sb_stats.poisson_pmf(k, lambda)`, `sb_stats.poisson_cdf(k, lambda)`

Candidate tests:
- `sb_stats.t_test(a_array, b_array, mode)` -> JSON summary
- `sb_stats.chi_square(observed, expected)` -> JSON summary

### 5.3 sb_linalg (Linear Algebra)
**Purpose:** matrix operations and solving systems.

Candidate functions:
- `sb_linalg.dot(a_array, b_array)`
- `sb_linalg.matmul(a_matrix, b_matrix)`
- `sb_linalg.solve(a_matrix, b_array)`
- `sb_linalg.svd(a_matrix)` -> JSON (U, S, Vt)

External libs: Eigen (preferred) or BLAS/LAPACK.

### 5.4 sb_timeseries (Time Series)
**Purpose:** rolling metrics and decomposition.

Candidate functions:
- `sb_ts.rolling_mean(series, window)`
- `sb_ts.rolling_std(series, window)`
- `sb_ts.ema(series, span)`
- `sb_ts.seasonal_decompose(series, period)` -> JSON components

### 5.5 sb_signal (Signal Processing)
**Purpose:** FFT, convolution, filters.

Candidate functions:
- `sb_signal.fft(series)` / `sb_signal.ifft(series)`
- `sb_signal.convolve(a, b, mode)`
- `sb_signal.lowpass(series, cutoff, order)`

External libs: FFTW (optional).

### 5.6 sb_finance (Financial)
**Purpose:** finance-grade analysis and scheduling.

Candidate functions:
- `sb_fin.npv(rate, cashflows)`
- `sb_fin.irr(cashflows)`
- `sb_fin.xnpv(rate, cashflows, dates)`
- `sb_fin.xirr(cashflows, dates)`
- `sb_fin.pv(rate, nper, pmt, fv)`
- `sb_fin.fv(rate, nper, pmt, pv)`
- `sb_fin.amort_schedule(principal, rate, term_months)` -> SETOF rows

Optional: day-count conventions, yield curve helpers, Black-Scholes.

### 5.7 sb_text (Text Analytics)
**Purpose:** fuzzy matching and similarity.

Candidate functions:
- `sb_text.levenshtein(a, b)`
- `sb_text.trigram_similarity(a, b)`
- `sb_text.soundex(a)` / `sb_text.metaphone(a)`

### 5.8 sb_geo (Advanced Spatial)
**Purpose:** advanced spatial predicates and topology.

Candidate functions:
- `sb_geo.distance(geom1, geom2)`
- `sb_geo.buffer(geom, radius)`
- `sb_geo.intersects(geom1, geom2)`

External libs: GEOS/PROJ (optional).

## 6. Example UDR Registration

```sql
CREATE LIBRARY sb_math_lib
    AS '/opt/scratchbird/udr/libsb_math.so';

CREATE FUNCTION sb_math.gamma(x DOUBLE)
RETURNS DOUBLE
EXTERNAL NAME 'sb_math_lib:gamma'
ENGINE UDR;
```

## 7. Recommended Priorities

- **Alpha**: sb_math (core), sb_stats (descriptive), sb_finance (NPV/IRR)
- **Beta**: sb_linalg, sb_timeseries, sb_text
- **Post-beta**: sb_signal, advanced sb_geo

## 8. Open Questions

- Confirm JSON array string vs native ARRAY for UDR parameter contracts.
- Decide standard error reporting format for statistical tests.
- Determine UDR bundle signing and distribution strategy for optional packs.

## 9. External Reference Inspirations

- Python: NumPy, SciPy, Pandas (math, stats, time series)
- R: base stats + CRAN finance/time-series packages
- PostgreSQL: contrib extensions, analytic function coverage
- Oracle/SQL Server: analytic/financial function sets
- QuantLib: financial math reference
- Boost.Math / Eigen / FFTW: C++ implementation targets
