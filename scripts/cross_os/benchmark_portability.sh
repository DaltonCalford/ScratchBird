#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
test_preset="linux-gcc-debug-test"
runs=3
output_json="${repo_root}/artifacts/cross_os/p6s3w2/xos-060-linux-benchmark.json"
baseline_json=""
test_regex="ServiceControllerListenerBootstrapTest\\.NormalizeConfigPathsAndValidateUtf8NormalizesKeyRuntimePaths|MySQLParserTest\\.CreateTableBasic|PostgreSQLParserTest\\.SimpleSelect|FirebirdParserTest\\.SelectSimple"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --test-preset)
      test_preset="$2"
      shift 2
      ;;
    --runs)
      runs="$2"
      shift 2
      ;;
    --output-json)
      output_json="$2"
      shift 2
      ;;
    --baseline-json)
      baseline_json="$2"
      shift 2
      ;;
    --test-regex)
      test_regex="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

mkdir -p "$(dirname "${output_json}")"

times=()
for i in $(seq 1 "${runs}"); do
  start_ns=$(date +%s%N)
  ctest --preset "${test_preset}" -R "${test_regex}" --output-on-failure >/dev/null
  end_ns=$(date +%s%N)
  elapsed_ms=$(( (end_ns - start_ns) / 1000000 ))
  times+=("${elapsed_ms}")
done

sorted_times=$(printf "%s\n" "${times[@]}" | sort -n)
median_ms=$(printf "%s\n" "${sorted_times}" | awk '{
  a[NR]=$1
}
END{
  if (NR % 2 == 1) {
    print a[(NR+1)/2]
  } else {
    print int((a[NR/2] + a[NR/2+1]) / 2)
  }
}')
mean_ms=$(printf "%s\n" "${times[@]}" | awk '{sum+=$1} END {if (NR>0) printf "%.2f", sum/NR; else print "0"}')

baseline_median_ms=""
regression_pct=""
within_threshold=""
if [[ -n "${baseline_json}" && -f "${baseline_json}" ]]; then
  baseline_median_ms=$(python3 - "${baseline_json}" <<'PY'
import json, sys
with open(sys.argv[1], encoding='utf-8') as f:
    payload=json.load(f)
print(payload.get("median_ms",""))
PY
)
  if [[ -n "${baseline_median_ms}" && "${baseline_median_ms}" != "0" ]]; then
    regression_pct=$(python3 - "${baseline_median_ms}" "${median_ms}" <<'PY'
import sys
baseline=float(sys.argv[1]); current=float(sys.argv[2])
print((current-baseline)/baseline*100.0)
PY
)
    within_threshold=$(python3 - "${regression_pct}" <<'PY'
import sys
print("yes" if float(sys.argv[1]) <= 5.0 else "no")
PY
)
  fi
fi

python3 - "${output_json}" "${test_preset}" "${runs}" "${test_regex}" "${mean_ms}" "${median_ms}" "${baseline_median_ms}" "${regression_pct}" "${within_threshold}" "${times[@]}" <<'PY'
import json
import sys

output_json = sys.argv[1]
test_preset = sys.argv[2]
runs = int(sys.argv[3])
test_regex = sys.argv[4]
mean_ms = float(sys.argv[5])
median_ms = int(float(sys.argv[6]))
baseline_median_ms = sys.argv[7]
regression_pct = sys.argv[8]
within_threshold = sys.argv[9]
samples = [int(v) for v in sys.argv[10:]]

payload = {
    "schema": "scratchbird.cross_os.benchmark.v1",
    "test_preset": test_preset,
    "runs": runs,
    "test_regex": test_regex,
    "samples_ms": samples,
    "mean_ms": mean_ms,
    "median_ms": median_ms,
}
if baseline_median_ms:
    payload["baseline_median_ms"] = int(float(baseline_median_ms))
if regression_pct:
    payload["regression_pct"] = float(regression_pct)
if within_threshold:
    payload["within_5pct_threshold"] = within_threshold

with open(output_json, "w", encoding="utf-8") as f:
    json.dump(payload, f, indent=2, sort_keys=True)
    f.write("\n")
PY

echo "Wrote benchmark JSON: ${output_json}"
