#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

SPEC_REGISTRY_JSON="${1:-/home/dcalford/CliWork/local_work/docs/specifications_vnext/13_Native_Dialect_Gap_Closure/NATIVE_GAP_FEATURE_REGISTRY.json}"
SCOPE_TSV="${2:-${REPO_ROOT}/tests/unit/data/native_sql_syn13_registration_scope.tsv}"
OUT_CSV="${3:-${REPO_ROOT}/build/native_sql_syn13_coverage_report.csv}"
SUMMARY_OUT="${4:-}"

if [[ ! -f "${SPEC_REGISTRY_JSON}" ]]; then
    echo "error: registry file not found: ${SPEC_REGISTRY_JSON}" >&2
    exit 2
fi
if [[ ! -f "${SCOPE_TSV}" ]]; then
    echo "error: scope file not found: ${SCOPE_TSV}" >&2
    exit 2
fi
if ! command -v jq >/dev/null 2>&1; then
    echo "error: jq is required for coverage generation" >&2
    exit 2
fi

declare -A MAP_FEATURE
declare -A MAP_PRIORITY
declare -A MAP_MANDATORY
declare -A MAP_SQL

while IFS=$'\t' read -r syn_id feature_key priority mandatory_scope sql_probe; do
    if [[ -z "${syn_id}" || "${syn_id}" == "syntax_contract_id" ]]; then
        continue
    fi
    MAP_FEATURE["${syn_id}"]="${feature_key}"
    MAP_PRIORITY["${syn_id}"]="${priority}"
    MAP_MANDATORY["${syn_id}"]="${mandatory_scope}"
    MAP_SQL["${syn_id}"]="${sql_probe}"
done < "${SCOPE_TSV}"

mkdir -p "$(dirname "${OUT_CSV}")"

csv_escape() {
    local value="${1//\"/\"\"}"
    printf '"%s"' "${value}"
}

{
    echo "syntax_contract_id,native_feature_key,priority,engine,gap_item_id,status,mandatory_scope,mandatory_miss,scope_feature_key,scope_priority,sql_probe"

    total_count=0
    mapped_count=0
    unmapped_count=0
    mandatory_count=0
    mandatory_miss_count=0

    while IFS=$'\t' read -r syn_id feature_key priority engine gap_item_id; do
        ((total_count += 1))

        scope_feature_key="${MAP_FEATURE[${syn_id}]:-}"
        scope_priority="${MAP_PRIORITY[${syn_id}]:-}"
        scope_mandatory="${MAP_MANDATORY[${syn_id}]:-0}"
        scope_sql="${MAP_SQL[${syn_id}]:-}"

        status="unmapped"
        mandatory_scope="0"
        mandatory_miss="0"

        if [[ -n "${scope_feature_key}" ]]; then
            status="mapped"
            ((mapped_count += 1))
            mandatory_scope="${scope_mandatory}"
            if [[ "${scope_feature_key}" != "${feature_key}" ]]; then
                echo "error: scope mapping mismatch for ${syn_id}: scope=${scope_feature_key}, registry=${feature_key}" >&2
                exit 3
            fi
            if [[ "${scope_mandatory}" == "1" ]]; then
                ((mandatory_count += 1))
            fi
        else
            ((unmapped_count += 1))
        fi

        if [[ "${mandatory_scope}" == "1" && "${status}" != "mapped" ]]; then
            mandatory_miss="1"
            ((mandatory_miss_count += 1))
        fi

        printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
            "$(csv_escape "${syn_id}")" \
            "$(csv_escape "${feature_key}")" \
            "$(csv_escape "${priority}")" \
            "$(csv_escape "${engine}")" \
            "$(csv_escape "${gap_item_id}")" \
            "$(csv_escape "${status}")" \
            "$(csv_escape "${mandatory_scope}")" \
            "$(csv_escape "${mandatory_miss}")" \
            "$(csv_escape "${scope_feature_key}")" \
            "$(csv_escape "${scope_priority}")" \
            "$(csv_escape "${scope_sql}")"
    done < <(jq -r '.[] | [.syntax_contract_id,.native_feature_key,.priority,.engine,.gap_item_id] | @tsv' "${SPEC_REGISTRY_JSON}")
} > "${OUT_CSV}"

summary_text="TOTAL=${total_count}
MAPPED=${mapped_count}
UNMAPPED=${unmapped_count}
MANDATORY_SCOPE=${mandatory_count}
MANDATORY_MISS=${mandatory_miss_count}
OUTPUT_CSV=${OUT_CSV}"

if [[ -n "${SUMMARY_OUT}" ]]; then
    mkdir -p "$(dirname "${SUMMARY_OUT}")"
    printf "%s\n" "${summary_text}" > "${SUMMARY_OUT}"
fi

printf "%s\n" "${summary_text}" >&2

if [[ "${mandatory_miss_count}" -ne 0 ]]; then
    exit 4
fi
