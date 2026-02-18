#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

REGISTRY_JSON="${1:-/home/dcalford/CliWork/local_work/docs/specifications_vnext/13_Native_Dialect_Gap_Closure/NATIVE_GAP_FEATURE_REGISTRY.json}"
SYN_REPORT_CSV="${2:-/home/dcalford/CliWork/local_work/docs/planning/NATIVE_SQL_IMPLEMENTATION_WORKTREE/gates/NSQL-GATE-04/SYN13_COVERAGE_REPORT.csv}"
BINDING_REPORT_CSV="${3:-/home/dcalford/CliWork/local_work/docs/planning/NATIVE_SQL_IMPLEMENTATION_WORKTREE/gates/NSQL-GATE-04/AST_SBLR_BINDING_REPORT.csv}"
OUT_MATRIX_CSV="${4:-/home/dcalford/CliWork/local_work/docs/planning/NATIVE_SQL_IMPLEMENTATION_WORKTREE/gates/NSQL-GATE-04/NATIVE_CAPABILITY_MATRIX.csv}"
OUT_ENGINE_CSV="${5:-/home/dcalford/CliWork/local_work/docs/planning/NATIVE_SQL_IMPLEMENTATION_WORKTREE/gates/NSQL-GATE-04/ENGINE_SURFACE_PACK_COVERAGE.csv}"
SUMMARY_OUT="${6:-}"

if [[ ! -f "${REGISTRY_JSON}" ]]; then
    echo "error: registry file not found: ${REGISTRY_JSON}" >&2
    exit 2
fi
if [[ ! -f "${SYN_REPORT_CSV}" ]]; then
    echo "error: SYN13 coverage report not found: ${SYN_REPORT_CSV}" >&2
    exit 2
fi
if [[ ! -f "${BINDING_REPORT_CSV}" ]]; then
    echo "error: binding coverage report not found: ${BINDING_REPORT_CSV}" >&2
    exit 2
fi
if ! command -v jq >/dev/null 2>&1; then
    echo "error: jq is required" >&2
    exit 2
fi

mkdir -p "$(dirname "${OUT_MATRIX_CSV}")"
mkdir -p "$(dirname "${OUT_ENGINE_CSV}")"

strip_csv_quotes() {
    local value="${1:-}"
    value="${value#\"}"
    value="${value%\"}"
    printf "%s" "${value}"
}

csv_escape() {
    local value="${1:-}"
    value="${value//\"/\"\"}"
    printf '"%s"' "${value}"
}

declare -A SYN_STATUS
declare -A MANDATORY_SCOPE
declare -A MANDATORY_MISS

while IFS=, read -r syn_id _ _ _ _ status mandatory_scope mandatory_miss _ _ _; do
    syn_id="$(strip_csv_quotes "${syn_id}")"
    if [[ -z "${syn_id}" || "${syn_id}" == "syntax_contract_id" ]]; then
        continue
    fi
    status="$(strip_csv_quotes "${status}")"
    mandatory_scope="$(strip_csv_quotes "${mandatory_scope}")"
    mandatory_miss="$(strip_csv_quotes "${mandatory_miss}")"
    SYN_STATUS["${syn_id}"]="${status}"
    MANDATORY_SCOPE["${syn_id}"]="${mandatory_scope}"
    MANDATORY_MISS["${syn_id}"]="${mandatory_miss}"
done < "${SYN_REPORT_CSV}"

declare -A BIND_STATUS
declare -A BIND_HASH
declare -A BIND_AST_KIND
declare -A BIND_ROOT_OPCODE

while IFS=, read -r row_type _ syn_id _ _ status _ _ ast_kind root_opcode deterministic_hash_match _; do
    row_type="$(strip_csv_quotes "${row_type}")"
    if [[ -z "${row_type}" || "${row_type}" == "row_type" ]]; then
        continue
    fi
    if [[ "${row_type}" != "mandatory_scope" ]]; then
        continue
    fi
    syn_id="$(strip_csv_quotes "${syn_id}")"
    status="$(strip_csv_quotes "${status}")"
    ast_kind="$(strip_csv_quotes "${ast_kind}")"
    root_opcode="$(strip_csv_quotes "${root_opcode}")"
    deterministic_hash_match="$(strip_csv_quotes "${deterministic_hash_match}")"
    BIND_STATUS["${syn_id}"]="${status}"
    BIND_HASH["${syn_id}"]="${deterministic_hash_match}"
    BIND_AST_KIND["${syn_id}"]="${ast_kind}"
    BIND_ROOT_OPCODE["${syn_id}"]="${root_opcode}"
done < "${BINDING_REPORT_CSV}"

{
    echo "gap_item_id,engine,priority,native_feature_key,syntax_contract_id,ast_contract_id,sblr_contract_id,native_domain,contract_doc,scope_status,mandatory_scope,mandatory_miss,binding_status,binding_deterministic,ast_kind,root_opcode_symbol,phase4_engine_pack,phase4_ready,status_reason"

    while IFS=$'\t' read -r gap_item_id engine priority native_feature_key syntax_contract_id ast_contract_id sblr_contract_id native_domain contract_doc; do
        scope_status="${SYN_STATUS[${syntax_contract_id}]:-unmapped}"
        mandatory_scope="${MANDATORY_SCOPE[${syntax_contract_id}]:-0}"
        mandatory_miss="${MANDATORY_MISS[${syntax_contract_id}]:-0}"
        binding_status="${BIND_STATUS[${syntax_contract_id}]:-not_audited}"
        binding_deterministic="${BIND_HASH[${syntax_contract_id}]:-0}"
        ast_kind="${BIND_AST_KIND[${syntax_contract_id}]:-}"
        root_opcode="${BIND_ROOT_OPCODE[${syntax_contract_id}]:-}"

        phase4_engine_pack=0
        case "${engine}" in
            Cassandra|ClickHouse|DuckDB|FirebirdSQL|Milvus|Neo4j|OpenSearch|PostgreSQL|Redis)
                phase4_engine_pack=1
                ;;
        esac

        phase4_ready=0
        status_reason="out_of_scope_registry_row"
        if [[ "${mandatory_scope}" == "1" ]]; then
            if [[ "${scope_status}" != "mapped" ]]; then
                status_reason="mandatory_not_closed_syn_unmapped"
            elif [[ "${binding_status}" != "pass" ]]; then
                status_reason="mandatory_not_closed_binding_fail"
            else
                phase4_ready=1
                status_reason="mandatory_closed"
            fi
        elif [[ "${scope_status}" == "mapped" ]]; then
            status_reason="scoped_non_mandatory"
        fi

        printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
            "$(csv_escape "${gap_item_id}")" \
            "$(csv_escape "${engine}")" \
            "$(csv_escape "${priority}")" \
            "$(csv_escape "${native_feature_key}")" \
            "$(csv_escape "${syntax_contract_id}")" \
            "$(csv_escape "${ast_contract_id}")" \
            "$(csv_escape "${sblr_contract_id}")" \
            "$(csv_escape "${native_domain}")" \
            "$(csv_escape "${contract_doc}")" \
            "$(csv_escape "${scope_status}")" \
            "$(csv_escape "${mandatory_scope}")" \
            "$(csv_escape "${mandatory_miss}")" \
            "$(csv_escape "${binding_status}")" \
            "$(csv_escape "${binding_deterministic}")" \
            "$(csv_escape "${ast_kind}")" \
            "$(csv_escape "${root_opcode}")" \
            "$(csv_escape "${phase4_engine_pack}")" \
            "$(csv_escape "${phase4_ready}")" \
            "$(csv_escape "${status_reason}")"
    done < <(
        jq -r '.[] | [.gap_item_id,.engine,.priority,.native_feature_key,.syntax_contract_id,.ast_contract_id,.sblr_contract_id,.native_domain,.contract_doc] | @tsv' "${REGISTRY_JSON}" |
            LC_ALL=C sort -t $'\t' -k2,2 -k5,5
    )
} > "${OUT_MATRIX_CSV}"

{
    echo "engine,total_rows,mapped_rows,mandatory_scope_rows,mandatory_miss_rows,binding_pass_rows,binding_fail_rows,phase4_ready_rows,p0_rows,p1_rows,p2_rows"
    awk -F, '
        NR == 1 { next }
        {
            gsub(/"/, "", $0);
            engine = $2;
            priority = $3;
            scope_status = $10;
            mandatory_scope = $11;
            mandatory_miss = $12;
            binding_status = $13;
            phase4_ready = $18;

            total[engine] += 1;
            if (scope_status == "mapped") mapped[engine] += 1;
            if (mandatory_scope == "1") mandatory[engine] += 1;
            if (mandatory_miss == "1") miss[engine] += 1;
            if (binding_status == "pass") bind_pass[engine] += 1;
            if (binding_status != "pass" && binding_status != "not_audited") bind_fail[engine] += 1;
            if (phase4_ready == "1") ready[engine] += 1;

            if (priority == "P0") p0[engine] += 1;
            else if (priority == "P1") p1[engine] += 1;
            else if (priority == "P2") p2[engine] += 1;
        }
        END {
            for (engine in total) {
                printf "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                    engine,
                    total[engine],
                    mapped[engine] + 0,
                    mandatory[engine] + 0,
                    miss[engine] + 0,
                    bind_pass[engine] + 0,
                    bind_fail[engine] + 0,
                    ready[engine] + 0,
                    p0[engine] + 0,
                    p1[engine] + 0,
                    p2[engine] + 0;
            }
        }
    ' "${OUT_MATRIX_CSV}" | LC_ALL=C sort
} > "${OUT_ENGINE_CSV}"

total_rows="$(awk -F, 'NR > 1 { ++n } END { print n + 0 }' "${OUT_MATRIX_CSV}")"
mandatory_scope_rows="$(awk -F, 'NR > 1 && $11 == "\"1\"" { ++n } END { print n + 0 }' "${OUT_MATRIX_CSV}")"
mandatory_closed_rows="$(awk -F, 'NR > 1 && $11 == "\"1\"" && $18 == "\"1\"" { ++n } END { print n + 0 }' "${OUT_MATRIX_CSV}")"
mandatory_open_rows="$(awk -F, 'NR > 1 && $11 == "\"1\"" && $18 != "\"1\"" { ++n } END { print n + 0 }' "${OUT_MATRIX_CSV}")"
phase4_pack_rows="$(awk -F, 'NR > 1 && $17 == "\"1\"" { ++n } END { print n + 0 }' "${OUT_MATRIX_CSV}")"
matrix_sha="$(sha256sum "${OUT_MATRIX_CSV}" | awk '{ print $1 }')"
engine_sha="$(sha256sum "${OUT_ENGINE_CSV}" | awk '{ print $1 }')"

summary_text="TOTAL_ROWS=${total_rows}
MANDATORY_SCOPE_ROWS=${mandatory_scope_rows}
MANDATORY_CLOSED_ROWS=${mandatory_closed_rows}
MANDATORY_OPEN_ROWS=${mandatory_open_rows}
PH4_PACK_ROWS=${phase4_pack_rows}
MATRIX_SHA256=${matrix_sha}
ENGINE_SHA256=${engine_sha}
OUTPUT_MATRIX=${OUT_MATRIX_CSV}
OUTPUT_ENGINE=${OUT_ENGINE_CSV}"

if [[ -n "${SUMMARY_OUT}" ]]; then
    mkdir -p "$(dirname "${SUMMARY_OUT}")"
    printf "%s\n" "${summary_text}" > "${SUMMARY_OUT}"
fi

printf "%s\n" "${summary_text}" >&2

if [[ "${mandatory_open_rows}" -ne 0 ]]; then
    exit 4
fi
