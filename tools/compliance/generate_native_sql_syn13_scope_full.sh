#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

REGISTRY_JSON="${1:-${REPO_ROOT}/docs/planning/native_sql/NATIVE_GAP_FEATURE_REGISTRY.json}"
OUT_TSV="${2:-${REPO_ROOT}/tests/unit/data/native_sql_syn13_registration_scope.tsv}"

if [[ ! -f "${REGISTRY_JSON}" ]]; then
    echo "error: registry file not found: ${REGISTRY_JSON}" >&2
    exit 2
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "error: jq is required" >&2
    exit 2
fi

mkdir -p "$(dirname "${OUT_TSV}")"

probe_for_domain() {
    local domain="${1:-}"
    case "${domain}" in
        command_surface)
            printf "%s" \
                "SELECT FIRST 10 SKIP 5 id FROM docs PLAN NATURAL OPTIMIZE FOR 100 ROWS FOR UPDATE WITH LOCK"
            ;;
        extensibility_surface)
            printf "%s" "INSTALL EXTENSION httpfs"
            ;;
        security_surface)
            printf "%s" "SECURITY LABEL FOR sec_provider ON TABLE docs IS 'classified'"
            ;;
        index_vector_search_surface)
            printf "%s" \
                "CREATE INDEX idx_vectors_pq ON vectors USING IVF_PQ (embedding) WITH (NLIST = 1024, M = 16, NPROBE = 32)"
            ;;
        datatype_surface)
            printf "%s" \
                "CREATE TABLE t_types ( agg AGGREGATEFUNCTION(sum, UInt64), sagg SIMPLEAGGREGATEFUNCTION(sum, UInt64), dyn DYNAMIC(JSON), bits QBIT(128), big BIGNUM(256) )"
            ;;
        streaming_replication_surface)
            printf "%s" "CREATE PUBLICATION pub_all FOR ALL TABLES"
            ;;
        runtime_surface)
            printf "%s" "SET SINGLE_WRITER ON"
            ;;
        connector_surface)
            printf "%s" "COPY docs TO PROGRAM 'gzip > /tmp/docs.gz' WITH (FORMAT CSV, HEADER)"
            ;;
        *)
            printf "%s" "SELECT 1"
            ;;
    esac
}

{
    printf "syntax_contract_id\tnative_feature_key\tpriority\tmandatory_scope\tsql_probe\n"
    jq -r '.[] | [.syntax_contract_id,.native_feature_key,.priority,.native_domain] | @tsv' "${REGISTRY_JSON}" \
        | LC_ALL=C sort -t $'\t' -k1,1 \
        | while IFS=$'\t' read -r syntax_id feature_key priority native_domain; do
            sql_probe="$(probe_for_domain "${native_domain}")"
            printf "%s\t%s\t%s\t1\t%s\n" \
                "${syntax_id}" \
                "${feature_key}" \
                "${priority}" \
                "${sql_probe}"
        done
} > "${OUT_TSV}"

row_count="$(awk 'NR>1 { ++n } END { print n + 0 }' "${OUT_TSV}")"
printf "SYN13_SCOPE_ROWS=%s\nOUTPUT_TSV=%s\n" "${row_count}" "${OUT_TSV#${REPO_ROOT}/}" >&2
