#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

IN_MATRIX_CSV="${1:-${REPO_ROOT}/docs/planning/native_sql/gates/NSQL-GATE-06/NATIVE_CAPABILITY_MATRIX.csv}"
OUT_DIR="${2:-${REPO_ROOT}/docs/planning/native_sql/gates/NSQL-GATE-07}"

TRACKER_TSV="${OUT_DIR}/EXECUTION_TRACKER.tsv"
OWNER_LOAD_TSV="${OUT_DIR}/EXECUTION_TRACKER_OWNER_LOAD.tsv"
SPRINT_LOAD_TSV="${OUT_DIR}/EXECUTION_TRACKER_SPRINT_LOAD.tsv"
SUMMARY_ENV="${OUT_DIR}/EXECUTION_TRACKER_SUMMARY.env"

if [[ ! -f "${IN_MATRIX_CSV}" ]]; then
    echo "error: input matrix not found: ${IN_MATRIX_CSV}" >&2
    exit 2
fi

mkdir -p "${OUT_DIR}"

awk -F, '
function strip_quotes(v) {
    gsub(/^"/, "", v)
    gsub(/"$/, "", v)
    return v
}
function owner_for(engine) {
    if (engine == "MySQL") return "SQL-Compat-MySQL"
    if (engine == "PostgreSQL") return "SQL-Compat-PostgreSQL"
    if (engine == "FirebirdSQL") return "SQL-Compat-Firebird"
    if (engine == "MariaDB") return "SQL-Compat-MariaDB"
    if (engine == "Cassandra") return "Polyglot-CQL"
    if (engine == "MongoDB") return "Polyglot-Mongo"
    if (engine == "Neo4j") return "Polyglot-Cypher"
    if (engine == "Redis") return "Polyglot-Redis"
    if (engine == "Milvus") return "Polyglot-Milvus"
    if (engine == "ClickHouse") return "Analytics-ClickHouse"
    if (engine == "InfluxDB") return "TimeSeries-InfluxDB"
    if (engine == "OpenSearch") return "Search-OpenSearch"
    if (engine == "DuckDB") return "Analytics-DuckDB"
    return "Core-V3-Engine"
}
function engine_rank(engine) {
    if (engine == "MySQL") return 1
    if (engine == "PostgreSQL") return 2
    if (engine == "FirebirdSQL") return 3
    if (engine == "MariaDB") return 4
    if (engine == "Cassandra") return 5
    if (engine == "ClickHouse") return 6
    if (engine == "DuckDB") return 7
    if (engine == "InfluxDB") return 8
    if (engine == "MongoDB") return 9
    if (engine == "Neo4j") return 10
    if (engine == "OpenSearch") return 11
    if (engine == "Redis") return 12
    if (engine == "Milvus") return 13
    return 99
}
function sprint_for(priority, rank) {
    if (priority == "P0") {
        if (rank <= 3) return "Sprint-3"
        return "Sprint-4"
    }
    if (priority == "P1") {
        if (rank <= 3) return "Sprint-4"
        if (rank <= 9) return "Sprint-5"
        return "Sprint-6"
    }
    return "Sprint-6"
}
function workpack_for(priority) {
    if (priority == "P0") return "WP-06-P0"
    if (priority == "P1") return "WP-06-P1"
    return "WP-06-P2"
}
BEGIN {
    OFS = "\t"
    print "gap_item_id","engine","priority","native_feature_key","syntax_contract_id","ast_contract_id","sblr_contract_id","native_domain","contract_doc","status_reason","owner","sprint","workpack_id","parser_touched","emitter_touched","executor_touched","doc_paths","test_paths","row_status","notes"
}
NR == 1 {
    next
}
{
    for (i = 1; i <= NF; i++) {
        $i = strip_quotes($i)
    }

    mandatory_scope = $11
    mandatory_miss = $12
    if (mandatory_scope != "1" || mandatory_miss != "1") {
        next
    }

    gap_item_id = $1
    engine = $2
    priority = $3
    native_feature_key = $4
    syntax_contract_id = $5
    ast_contract_id = $6
    sblr_contract_id = $7
    native_domain = $8
    contract_doc = $9
    status_reason = $19

    owner = owner_for(engine)
    rank = engine_rank(engine)
    sprint = sprint_for(priority, rank)
    workpack = workpack_for(priority)

    print gap_item_id,engine,priority,native_feature_key,syntax_contract_id,ast_contract_id,sblr_contract_id,native_domain,contract_doc,status_reason,owner,sprint,workpack,"Y","Y","Y","docs/user-documentation/language-guide/","tests/unit/;tests/integration/","OPEN","Gate-06 mandatory-open baseline"
}
' "${IN_MATRIX_CSV}" > "${TRACKER_TSV}"

{
    echo -e "owner\ttotal_rows\tp0_rows\tp1_rows\tp2_rows"
    awk -F'\t' '
        NR == 1 { next }
        {
            owner = $11
            priority = $3
            total[owner] += 1
            if (priority == "P0") p0[owner] += 1
            else if (priority == "P1") p1[owner] += 1
            else if (priority == "P2") p2[owner] += 1
        }
        END {
            for (owner in total) {
                printf "%s\t%d\t%d\t%d\t%d\n",
                    owner,
                    total[owner],
                    p0[owner] + 0,
                    p1[owner] + 0,
                    p2[owner] + 0
            }
        }
    ' "${TRACKER_TSV}" | LC_ALL=C sort
} > "${OWNER_LOAD_TSV}"

{
    echo -e "sprint\ttotal_rows\tp0_rows\tp1_rows\tp2_rows"
    awk -F'\t' '
        NR == 1 { next }
        {
            sprint = $12
            priority = $3
            total[sprint] += 1
            if (priority == "P0") p0[sprint] += 1
            else if (priority == "P1") p1[sprint] += 1
            else if (priority == "P2") p2[sprint] += 1
        }
        END {
            for (sprint in total) {
                printf "%s\t%d\t%d\t%d\t%d\n",
                    sprint,
                    total[sprint],
                    p0[sprint] + 0,
                    p1[sprint] + 0,
                    p2[sprint] + 0
            }
        }
    ' "${TRACKER_TSV}" | LC_ALL=C sort -t$'\t' -k1,1
} > "${SPRINT_LOAD_TSV}"

total_rows="$(awk -F'\t' 'NR > 1 { ++n } END { print n + 0 }' "${TRACKER_TSV}")"
p0_rows="$(awk -F'\t' 'NR > 1 && $3 == "P0" { ++n } END { print n + 0 }' "${TRACKER_TSV}")"
p1_rows="$(awk -F'\t' 'NR > 1 && $3 == "P1" { ++n } END { print n + 0 }' "${TRACKER_TSV}")"
p2_rows="$(awk -F'\t' 'NR > 1 && $3 == "P2" { ++n } END { print n + 0 }' "${TRACKER_TSV}")"
s3_rows="$(awk -F'\t' 'NR > 1 && $12 == "Sprint-3" { ++n } END { print n + 0 }' "${TRACKER_TSV}")"
s4_rows="$(awk -F'\t' 'NR > 1 && $12 == "Sprint-4" { ++n } END { print n + 0 }' "${TRACKER_TSV}")"
s5_rows="$(awk -F'\t' 'NR > 1 && $12 == "Sprint-5" { ++n } END { print n + 0 }' "${TRACKER_TSV}")"
s6_rows="$(awk -F'\t' 'NR > 1 && $12 == "Sprint-6" { ++n } END { print n + 0 }' "${TRACKER_TSV}")"
owner_count="$(awk -F'\t' 'NR > 1 { owners[$11] = 1 } END { c = 0; for (o in owners) { c += 1 } print c + 0 }' "${TRACKER_TSV}")"
tracker_sha="$(sha256sum "${TRACKER_TSV}" | awk '{ print $1 }')"
owner_load_sha="$(sha256sum "${OWNER_LOAD_TSV}" | awk '{ print $1 }')"
sprint_load_sha="$(sha256sum "${SPRINT_LOAD_TSV}" | awk '{ print $1 }')"

cat > "${SUMMARY_ENV}" <<EOF
TRACKER_TOTAL_ROWS=${total_rows}
TRACKER_P0_ROWS=${p0_rows}
TRACKER_P1_ROWS=${p1_rows}
TRACKER_P2_ROWS=${p2_rows}
TRACKER_SPRINT3_ROWS=${s3_rows}
TRACKER_SPRINT4_ROWS=${s4_rows}
TRACKER_SPRINT5_ROWS=${s5_rows}
TRACKER_SPRINT6_ROWS=${s6_rows}
TRACKER_OWNER_COUNT=${owner_count}
TRACKER_SHA256=${tracker_sha}
OWNER_LOAD_SHA256=${owner_load_sha}
SPRINT_LOAD_SHA256=${sprint_load_sha}
INPUT_MATRIX=${IN_MATRIX_CSV#${REPO_ROOT}/}
OUTPUT_TRACKER=${TRACKER_TSV#${REPO_ROOT}/}
OUTPUT_OWNER_LOAD=${OWNER_LOAD_TSV#${REPO_ROOT}/}
OUTPUT_SPRINT_LOAD=${SPRINT_LOAD_TSV#${REPO_ROOT}/}
EOF

printf "%s\n" \
    "TRACKER_TOTAL_ROWS=${total_rows}" \
    "TRACKER_P0_ROWS=${p0_rows}" \
    "TRACKER_P1_ROWS=${p1_rows}" \
    "TRACKER_P2_ROWS=${p2_rows}" \
    "TRACKER_SPRINT3_ROWS=${s3_rows}" \
    "TRACKER_SPRINT4_ROWS=${s4_rows}" \
    "TRACKER_SPRINT5_ROWS=${s5_rows}" \
    "TRACKER_SPRINT6_ROWS=${s6_rows}" \
    "TRACKER_OWNER_COUNT=${owner_count}" \
    "OUTPUT_TRACKER=${TRACKER_TSV#${REPO_ROOT}/}" >&2
