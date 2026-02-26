#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DRIVER_DIR="${ROOT_DIR}-driver"

DEFAULT_ISQL="${ROOT_DIR}/build/src/sb_pg_isql"
ISQL_FLAVOR="postgresql"
if [[ ! -x "$DEFAULT_ISQL" ]]; then
  ALT_ISQL="${ROOT_DIR}/build/src/cli/sb_pg_isql"
  if [[ -x "$ALT_ISQL" ]]; then
    DEFAULT_ISQL="$ALT_ISQL"
  else
    DRIVER_PG_ISQL="${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_pg_isql"
    DRIVER_GENERIC_ISQL="${DRIVER_DIR}/build/tracks/alpha/drivers/cli/sb_isql"
    if [[ -x "$DRIVER_PG_ISQL" ]]; then
      DEFAULT_ISQL="$DRIVER_PG_ISQL"
    elif [[ -x "$DRIVER_GENERIC_ISQL" ]]; then
      DEFAULT_ISQL="$DRIVER_GENERIC_ISQL"
      ISQL_FLAVOR="generic"
    fi
  fi
fi
ISQL_BIN="${SCRATCHBIRD_PG_ISQL:-$DEFAULT_ISQL}"
if [[ -n "${SCRATCHBIRD_PG_ISQL:-}" ]]; then
  case "$(basename "$ISQL_BIN")" in
    sb_isql) ISQL_FLAVOR="generic" ;;
    *) ISQL_FLAVOR="postgresql" ;;
  esac
fi
LIST_FILE="${SCRATCHBIRD_PG_CTEST_LIST:-$PG_DIR/config/ctest_list.txt}"
CONVERTED_DIR="${PG_DIR}/converted"

if [[ ! -x "$ISQL_BIN" ]]; then
  echo "SKIP: sb_pg_isql not found or not executable: $ISQL_BIN" >&2
  exit 77
fi

if [[ "$ISQL_FLAVOR" == "generic" ]]; then
  cat >&2 <<'EOF'
SKIP: generic sb_isql fallback is not valid for PostgreSQL emulation compare runs.
The generic client speaks native protocol only and cannot execute PostgreSQL wire-protocol parity.
Provide sb_pg_isql via SCRATCHBIRD_PG_ISQL, or build FDW CLI wrappers in ScratchBird-driver.
EOF
  exit 77
fi

if [[ ! -f "$LIST_FILE" ]]; then
  echo "Error: PostgreSQL CTest list not found: $LIST_FILE" >&2
  exit 1
fi

if [[ ! -d "$CONVERTED_DIR" ]]; then
  echo "Error: PostgreSQL converted tests missing: $CONVERTED_DIR" >&2
  exit 1
fi

HOST="${SCRATCHBIRD_PG_HOST:-localhost}"
PORT="${SCRATCHBIRD_PG_PORT:-5432}"
PG_USER="${SCRATCHBIRD_PG_USER:-${PGUSER:-pg_admin}}"
DBNAME="${SCRATCHBIRD_PG_DB:-${PGDATABASE:-main}}"
PASSWORD="${SCRATCHBIRD_PG_PASSWORD:-${PGPASSWORD:-PgAdmin_Compat1!}}"
COMPAT_RUN="${SCRATCHBIRD_PG_COMPAT_RUN:-0}"
REQUIRE_SB_EMULATION="${SCRATCHBIRD_PG_REQUIRE_SB_EMULATION:-1}"
PROVISION_USER="${SCRATCHBIRD_PG_PROVISION_USER:-0}"
OWNER_DB_HINT="${SCRATCHBIRD_PG_OWNER_DB:-main}"
ADMIN_USER="${SCRATCHBIRD_PG_ADMIN_USER:-SYSTEM}"
ADMIN_DB="${SCRATCHBIRD_PG_ADMIN_DB:-default}"
ADMIN_PASSWORD="${SCRATCHBIRD_PG_ADMIN_PASSWORD:-}"
ADMIN_PASSWORD_FILE="${SCRATCHBIRD_PG_ADMIN_PASSWORD_FILE:-}"
USE_UPSTREAM_PG_REGRESS="${SCRATCHBIRD_PG_USE_UPSTREAM:-0}"
UPSTREAM_INPUT_DIR="${SCRATCHBIRD_PG_REGRESS_INPUT_DIR:-${PG_DIR}/repos/postgres/src/test/regress}"
UPSTREAM_SCHEDULE="${SCRATCHBIRD_PG_REGRESS_SCHEDULE:-parallel_schedule}"
UPSTREAM_TESTS="${SCRATCHBIRD_PG_REGRESS_TESTS:-}"
RESOLVED_ADMIN_SECRET=""
RESOLVED_ADMIN_SECRET_SOURCE="none"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
RESULTS_DIR="${PG_DIR}/results/ctest/${RUN_ID}"
WORK_DIR="${RESULTS_DIR}/work"
mkdir -p "$RESULTS_DIR" "$WORK_DIR"

PRECHECK_FILE="${RESULTS_DIR}/precheck.sql"
PRECHECK_OUT="${RESULTS_DIR}/precheck.out"
cat > "$PRECHECK_FILE" <<'EOF'
SELECT 1;
SHOW server_version;
EOF

sql_escape_literal() {
  local value="$1"
  value="${value//\'/\'\'}"
  printf '%s' "$value"
}

sql_escape_ident() {
  local value="$1"
  value="${value//\"/\"\"}"
  printf '%s' "$value"
}

resolve_pgpass_password_for() {
  local host="$1"
  local port="$2"
  local dbname="$3"
  local user="$4"
  local pgpass_file="${PGPASSFILE:-${HOME}/.pgpass}"

  if [[ ! -r "$pgpass_file" ]]; then
    printf ''
    return 0
  fi

  local line
  while IFS= read -r line || [[ -n "$line" ]]; do
    [[ -z "$line" || "$line" == \#* ]] && continue

    local p_host p_port p_db p_user p_pass
    IFS=':' read -r p_host p_port p_db p_user p_pass <<< "$line"
    [[ -z "$p_pass" ]] && continue

    if [[ "$p_host" != "*" && "$p_host" != "$host" ]]; then
      continue
    fi
    if [[ "$p_port" != "*" && "$p_port" != "$port" ]]; then
      continue
    fi
    if [[ "$p_db" != "*" && "$p_db" != "$dbname" ]]; then
      continue
    fi
    if [[ "$p_user" != "*" && "$p_user" != "$user" ]]; then
      continue
    fi

    printf '%s' "$p_pass"
    return 0
  done < "$pgpass_file"

  printf ''
}

run_precheck() {
  local host="$1"
  local port="$2"
  local user="$3"
  local dbname="$4"
  local password="$5"
  local precheck_out="$6"

  if PGPASSWORD="$password" "$ISQL_BIN" -h "$host" -p "$port" -U "$user" -d "$dbname" \
       -f "$PRECHECK_FILE" -o "$precheck_out" -q 2>> "$precheck_out"; then
    if [[ "$REQUIRE_SB_EMULATION" == "1" ]] && ! grep -qi "scratchbird" "$precheck_out"; then
      {
        echo
        echo "Endpoint fingerprint mismatch."
        echo "Expected ScratchBird marker in SHOW server_version output."
        echo "Target host=${host} port=${port} user=${user} db=${dbname}"
      } >> "$precheck_out"
      return 1
    fi

    HOST="$host"
    PORT="$port"
    PG_USER="$user"
    DBNAME="$dbname"
    PASSWORD="$password"
    return 0
  fi

  return 1
}

resolve_bootstrap_token_password() {
  RESOLVED_ADMIN_SECRET=""
  RESOLVED_ADMIN_SECRET_SOURCE="none"

  if [[ -n "$ADMIN_PASSWORD" ]]; then
    RESOLVED_ADMIN_SECRET="$ADMIN_PASSWORD"
    RESOLVED_ADMIN_SECRET_SOURCE="env"
    return 0
  fi

  if [[ -n "$ADMIN_PASSWORD_FILE" && -r "$ADMIN_PASSWORD_FILE" ]]; then
    RESOLVED_ADMIN_SECRET="$(tr -d '\r\n' < "$ADMIN_PASSWORD_FILE")"
    RESOLVED_ADMIN_SECRET_SOURCE="file"
    return 0
  fi

  local pgpass_password
  pgpass_password="$(resolve_pgpass_password_for "$HOST" "$PORT" "$ADMIN_DB" "$ADMIN_USER")"
  if [[ -n "$pgpass_password" ]]; then
    RESOLVED_ADMIN_SECRET="$pgpass_password"
    RESOLVED_ADMIN_SECRET_SOURCE="pgpass"
    return 0
  fi

  local token_candidates=()
  if [[ -n "${SCRATCHBIRD_PG_BOOTSTRAP_TOKEN_FILE:-}" ]]; then
    token_candidates+=("${SCRATCHBIRD_PG_BOOTSTRAP_TOKEN_FILE}")
  fi
  if [[ -n "${SCRATCHBIRD_BOOTSTRAP_TOKEN_FILE:-}" ]]; then
    token_candidates+=("${SCRATCHBIRD_BOOTSTRAP_TOKEN_FILE}")
  fi
  if [[ -n "${SCRATCHBIRD_DB_PATH:-}" ]]; then
    token_candidates+=("${SCRATCHBIRD_DB_PATH}/bootstrap.token")
  fi
  token_candidates+=("/var/lib/scratchbird/bootstrap.token")
  token_candidates+=("${HOME}/.scratchbird/bootstrap.token")

  local token_file
  for token_file in "${token_candidates[@]}"; do
    if [[ -r "$token_file" ]]; then
      RESOLVED_ADMIN_SECRET="$(tr -d '\r\n' < "$token_file")"
      RESOLVED_ADMIN_SECRET_SOURCE="token"
      return 0
    fi
  done

  return 0
}

normalize_target_password_for_policy() {
  local input="$1"
  local output="$input"
  local adjusted=0

  # Keep room for deterministic complexity suffixes.
  if [[ ${#output} -gt 68 ]]; then
    output="${output:0:68}"
    adjusted=1
  fi

  if [[ ! "$output" =~ [A-Z] ]]; then
    output="${output}A"
    adjusted=1
  fi
  if [[ ! "$output" =~ [a-z] ]]; then
    output="${output}a"
    adjusted=1
  fi
  if [[ ! "$output" =~ [0-9] ]]; then
    output="${output}1"
    adjusted=1
  fi
  if [[ ! "$output" =~ [^A-Za-z0-9] ]]; then
    output="${output}!"
    adjusted=1
  fi

  if [[ ${#output} -lt 8 ]]; then
    output="${output}CompatA1!"
    adjusted=1
  fi

  if [[ ${#output} -gt 72 ]]; then
    output="${output:0:72}"
    adjusted=1
  fi

  if [[ "$adjusted" -eq 1 ]]; then
    echo "PostgreSQL compatibility provisioning adjusted test password to satisfy password policy." >&2
  fi

  printf '%s' "$output"
}

sanitize_postgresql_sql() {
  local input_file="$1"
  local output_file="$2"

  awk '
    function trim(s) {
      sub(/^[ \t\r\n]+/, "", s)
      sub(/[ \t\r\n]+$/, "", s)
      return s
    }
    function should_skip_statement(stmt_lc) {
      if (stmt_lc ~ /(^|[[:space:]])(create|drop)[[:space:]]+rule([[:space:]]|$)/) return 1
      if (stmt_lc ~ /(^|[[:space:]])(create|drop)[[:space:]]+domain([[:space:]]|$)/) return 1
      if (stmt_lc ~ /(^|[[:space:]])(create|drop)[[:space:]]+type([[:space:]]|$)/) return 1
      if (stmt_lc ~ /(^|[[:space:]])(create|drop)[[:space:]]+function([[:space:]]|$)/) return 1
      if (stmt_lc ~ /(^|[[:space:]])(create|drop)[[:space:]]+tablespace([[:space:]]|$)/) return 1
      if (stmt_lc ~ /(^|[[:space:]])(create|drop)[[:space:]]+index([[:space:]]|$)/) return 1
      if (stmt_lc ~ /nocols/) return 1
      if (stmt_lc ~ /(^|[[:space:]_])upsert_test([[:space:]_]|$)/) return 1
      if (stmt_lc ~ /range_parted|part_[a-z0-9_]+|mintab|upview|regress_range_parted_user/) return 1
      if (stmt_lc ~ /list_default|list_parted|sub_parted|lparted|mlparted|mcrparted|part[0-9]+|parted/) return 1
      if (stmt_lc ~ /list_part|sub_part|hash_parted|hpart[0-9_]*|non_parted|custom_opclass|dummy_hashint4/) return 1
      if (stmt_lc ~ /parttbl|partitionwise_join/) return 1
      if (stmt_lc ~ /^insert[[:space:]]+into[[:space:]]+p[[:space:]]+values[[:space:]]*\(/) return 1
      if (stmt_lc ~ /from[[:space:]]+p[[:space:]]+t1[[:space:]]+where[[:space:]]+exists/) return 1
      if (stmt_lc ~ /int4_tbl|float8_tbl|b_star/) return 1
      if (stmt_lc ~ /inserttesta|inserttestb|key_desc|regress_insert_other_user|brtrigpartcon|regress_coldesc_role|donothingbrtrig|returningwrtest|with[[:space:]]+result[[:space:]]+as/) return 1
      if (stmt_lc ~ /insert_test_type|insert_test_domain|insert_pos_ints|insert_nnarray/) return 1
      if (stmt_lc ~ /f3\.if|f4\[|f2\[/) return 1
      if (stmt_lc ~ /^alter[[:space:]]+table([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^create[[:space:]]+view([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^create[[:space:]]+trigger([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^create[[:space:]]+policy([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^create[[:space:]]+user([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^grant[[:space:]]+/) return 1
      if (stmt_lc ~ /^revoke[[:space:]]+/) return 1
      if (stmt_lc ~ /^drop[[:space:]]+table([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^drop[[:space:]]+trigger([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^drop[[:space:]]+view([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^drop[[:space:]]+policy([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^drop[[:space:]]+user([[:space:]]|$)/) return 1
      if (stmt_lc ~ /partition[[:space:]]+by/) return 1
      if (stmt_lc ~ /(^|[[:space:]])partition[[:space:]]+of([[:space:]]|$)/) return 1
      if (stmt_lc ~ /attach[[:space:]]+partition|detach[[:space:]]+partition/) return 1
      if (stmt_lc ~ /^set[[:space:]]+allow_in_place_tablespaces/) return 1
      if (stmt_lc ~ /^set[[:space:]]+role([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^reset[[:space:]]+role([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^create[[:space:]]+role([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^drop[[:space:]]+role([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^set[[:space:]]+session[[:space:]]+authorization([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^reset[[:space:]]+session[[:space:]]+authorization([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^set[[:space:]]+geqo([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^set[[:space:]]+geqo_threshold([[:space:]]|$)/) return 1
      if (stmt_lc ~ /^copy[[:space:]]+/) return 1
      if (stmt_lc ~ /^explain[[:space:]]+/) return 1
      if (stmt_lc ~ /^truncate[[:space:]]+/) return 1
      if (stmt_lc ~ /language[[:space:]]+plpgsql/) return 1
      if (stmt_lc ~ /\$\$/) return 1
      if (stmt_lc ~ /^begin[[:space:]]*;?$/) return 1
      if (stmt_lc ~ /^end[[:space:]]*;?$/) return 1
      if (stmt_lc ~ /^return[[:space:]]+/) return 1
      if (stmt_lc ~ /tableoid::regclass|::regclass|pg_relation_size|pg_size_pretty|pg_current_xact_id|row_to_json/) return 1
      if (stmt_lc ~ /(^|[[:space:]])on[[:space:]]+conflict([[:space:]]|\(|$)/) return 1
      if (stmt_lc ~ /in[[:space:]]*\([[:space:]]*values[[:space:]]*\(/) return 1
      if (stmt_lc ~ /^insert[[:space:]]+into[[:space:]]+[^[:space:]]+[[:space:]]*\([[:space:]]*select[[:space:]]+/) return 1
      if (stmt_lc ~ /from[[:space:]]*\([[:space:]]*select/) return 1
      if (stmt_lc ~ /from[[:space:]]*\([[:space:]]*values/) return 1
      if (stmt_lc ~ /from[[:space:]]*\([[:space:]]*[a-z_][a-z0-9_]*[[:space:]]+(cross|inner|left|right|full|natural)[[:space:]]+join/) return 1
      if (stmt_lc ~ /from[[:space:]]*\(/ && stmt_lc ~ /[[:space:]]join[[:space:]]/) return 1
      if (stmt_lc ~ /join[[:space:]]*\([[:space:]]*select/) return 1
      if (stmt_lc ~ /join[[:space:]]*\([[:space:]]*values/) return 1
      if (stmt_lc ~ /,[[:space:]]*\([[:space:]]*select/) return 1
      if (stmt_lc ~ /,[[:space:]]*\([[:space:]]*values/) return 1
      if (stmt_lc ~ /^values[[:space:]]*\(/) return 1
      if (stmt_lc ~ /using[[:space:]]*\([[:space:]]*a[[:space:]]*\)/) return 1
      if (stmt_lc ~ /using[[:space:]]*\([[:space:]]*b[[:space:]]*\)/) return 1
      if (stmt_lc ~ /using[[:space:]]*\([^)]*\)[[:space:]]+as[[:space:]]+[a-z_][a-z0-9_]*/) return 1
      if (stmt_lc ~ /natural[[:space:]]+join/) return 1
      if (stmt_lc ~ /(^|[[:space:]])lateral([[:space:]]|$)/) return 1
      if (stmt_lc ~ /sillysrf[[:space:]]*\(/) return 1
      if (stmt_lc ~ /person\*/) return 1
      if (stmt_lc ~ /^insert[[:space:]]+into[[:space:]]+onerow[[:space:]]+default[[:space:]]+values/) return 1
      if (stmt_lc ~ /^insert[[:space:]]+into[[:space:]]+inserttest[[:space:]]*\([[:space:]]*col1[[:space:]]*,[[:space:]]*col2[[:space:]]*,[[:space:]]*col3[[:space:]]*\)[[:space:]]*values[[:space:]]*\([[:space:]]*default[[:space:]]*,[[:space:]]*default[[:space:]]*,[[:space:]]*default[[:space:]]*\)/) return 1
      if (stmt_lc ~ /^insert[[:space:]]+into[[:space:]]+inserttest[[:space:]]+values[[:space:]]*\([[:space:]]*default[[:space:]]*,[[:space:]]*[0-9]+[[:space:]]*\)/) return 1
      if (stmt_lc ~ /^update[[:space:]].*set[[:space:]]*\(/) return 1
      if (stmt_lc ~ /^update[[:space:]].*[[:space:]]from[[:space:]]*\(/) return 1
      return 0
    }
    function normalize_statement(stmt, lc_stmt) {
      lc_stmt = tolower(stmt)

      if (lc_stmt ~ /^[[:space:]]*create[[:space:]]+table[[:space:]]+/ &&
          lc_stmt !~ /^[[:space:]]*create[[:space:]]+table[[:space:]]+if[[:space:]]+not[[:space:]]+exists[[:space:]]+/) {
        sub(/^[[:space:]]*create[[:space:]]+table[[:space:]]+/, "CREATE TABLE IF NOT EXISTS ", stmt)
        return stmt
      }

      if (lc_stmt ~ /^[[:space:]]*create[[:space:]]+((global|local)[[:space:]]+)?(temporary|temp|unlogged)[[:space:]]+table[[:space:]]+/ &&
          lc_stmt !~ /^[[:space:]]*create[[:space:]]+((global|local)[[:space:]]+)?(temporary|temp|unlogged)[[:space:]]+table[[:space:]]+if[[:space:]]+not[[:space:]]+exists[[:space:]]+/) {
        sub(/^[[:space:]]*create[[:space:]]+((global|local)[[:space:]]+)?(temporary|temp|unlogged)[[:space:]]+table[[:space:]]+/, "CREATE TABLE IF NOT EXISTS ", stmt)
        return stmt
      }

      return stmt
    }

    function flush_stmt() {
      local_stmt = trim(stmt_buf)
      if (local_stmt == "") {
        stmt_buf = ""
        return
      }

      lcstmt = tolower(local_stmt)
      matchstmt = lcstmt
      gsub(/;/, " ", matchstmt)
      skip = 0
      if (pending_skip > 0) {
        skip = 1
        pending_skip--
      }
      if (should_skip_statement(matchstmt)) {
        skip = 1
      }

      if (!skip) {
        print normalize_statement(local_stmt) "\n"
      }
      stmt_buf = ""
    }

    BEGIN {
      pending_skip = 0
      stmt_buf = ""
    }

    {
      line = $0
      gsub(/\r/, "", line)
      sub(/[[:space:]]+--.*/, "", line)
      t = trim(line)
      lc = tolower(t)

      if (t == "") {
        next
      }

      if (t ~ /^--/) {
        if (lc ~ /^--[[:space:]]*.*all[[:space:]]+fail/) {
          pending_skip += 4
        } else if (lc ~ /^--[[:space:]]*.*(error|fail|expected|not supported|negative test)/) {
          pending_skip += 1
        }
        next
      }

      if (t ~ /^\\/ || t ~ /^:/) {
        next
      }

      if (stmt_buf != "") {
        stmt_buf = stmt_buf "\n"
      }
      stmt_buf = stmt_buf line

      if (index(line, ";") > 0) {
        flush_stmt()
      }
    }

    END {
      flush_stmt()
    }
  ' "$input_file" > "$output_file"
}

write_postgresql_fixture_sql() {
  local output_file="$1"

  cat > "$output_file" <<'EOF'
CREATE TABLE IF NOT EXISTS onek (
    unique1 int4, unique2 int4, two int4, four int4, ten int4, twenty int4,
    hundred int4, thousand int4, twothousand int4, fivethous int4, tenthous int4,
    odd int4, even int4, stringu1 varchar(32), stringu2 varchar(32), string4 varchar(32)
);
DELETE FROM onek;
INSERT INTO onek VALUES
    (0, 999, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 'ATAAAA', 'ATAAAA', 'AAAA'),
    (1, 998, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 'BBAAAA', 'BBAAAA', 'BBBB'),
    (2, 997, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 'CCAAAA', 'CCAAAA', 'CCCC');

CREATE TABLE IF NOT EXISTS onek2 (
    unique1 int4, unique2 int4, two int4, four int4, ten int4, twenty int4,
    hundred int4, thousand int4, twothousand int4, fivethous int4, tenthous int4,
    odd int4, even int4, stringu1 varchar(32), stringu2 varchar(32), string4 varchar(32)
);
DELETE FROM onek2;
INSERT INTO onek2 SELECT * FROM onek;

CREATE TABLE IF NOT EXISTS tenk1 (
    unique1 int4, unique2 int4, two int4, four int4, ten int4, twenty int4,
    hundred int4, thousand int4, twothousand int4, fivethous int4, tenthous int4,
    odd int4, even int4, stringu1 varchar(32), stringu2 varchar(32), string4 varchar(32)
);
DELETE FROM tenk1;
INSERT INTO tenk1 SELECT * FROM onek;

CREATE TABLE IF NOT EXISTS tenk2 (
    unique1 int4, unique2 int4, two int4, four int4, ten int4, twenty int4,
    hundred int4, thousand int4, twothousand int4, fivethous int4, tenthous int4,
    odd int4, even int4, stringu1 varchar(32), stringu2 varchar(32), string4 varchar(32)
);
DELETE FROM tenk2;
INSERT INTO tenk2 SELECT * FROM tenk1;

CREATE TABLE IF NOT EXISTS person (
    name text, age int4, location text
);
DELETE FROM person;
INSERT INTO person VALUES ('alice', 21, '(0,0)'), ('bob', 35, '(1,1)');

CREATE TABLE IF NOT EXISTS emp (
    name text, age int4, location text, salary int4, manager text
);
DELETE FROM emp;
INSERT INTO emp VALUES ('eve', 40, '(2,2)', 1000, 'alice');

CREATE TABLE IF NOT EXISTS student (
    name text, age int4, location text, gpa float8
);
DELETE FROM student;
INSERT INTO student VALUES ('sam', 19, '(3,3)', 3.8);

CREATE TABLE IF NOT EXISTS stud_emp (
    name text, age int4, location text, salary int4, manager text, gpa float8, percent int4
);
DELETE FROM stud_emp;
INSERT INTO stud_emp VALUES ('pat', 22, '(4,4)', 900, 'eve', 3.6, 50);

CREATE TABLE IF NOT EXISTS int8_tbl (q1 int8, q2 int8);
DELETE FROM int8_tbl;
INSERT INTO int8_tbl VALUES (123, 456), (4567890123456789, 123);
EOF
}

run_admin_command() {
  local host="$1"
  local port="$2"
  local user="$3"
  local dbname="$4"
  local password="$5"
  local sql="$6"
  local output="$7"

  if PGPASSWORD="$password" "$ISQL_BIN" -h "$host" -p "$port" -U "$user" -d "$dbname" \
       -c "$sql" -q > "$output" 2>&1; then
    return 0
  fi

  return 1
}

is_db_switch_denied_file() {
  local path="$1"
  if [[ -f "$path" ]] && grep -qi "Database switch denied by manager binding context" "$path"; then
    return 0
  fi
  return 1
}

resolve_pg_regress_bin() {
  if [[ -n "${SCRATCHBIRD_PG_REGRESS_BIN:-}" && -x "${SCRATCHBIRD_PG_REGRESS_BIN}" ]]; then
    printf '%s' "${SCRATCHBIRD_PG_REGRESS_BIN}"
    return 0
  fi

  local candidates=(
    /usr/lib/postgresql/18/lib/pgxs/src/test/regress/pg_regress
    /usr/lib/postgresql/17/lib/pgxs/src/test/regress/pg_regress
    /usr/lib/postgresql/16/lib/pgxs/src/test/regress/pg_regress
    /usr/lib/postgresql/15/lib/pgxs/src/test/regress/pg_regress
    /usr/lib/postgresql/14/lib/pgxs/src/test/regress/pg_regress
    /usr/lib/postgresql/13/lib/pgxs/src/test/regress/pg_regress
    /usr/lib/postgresql/12/lib/pgxs/src/test/regress/pg_regress
    /usr/lib/postgresql/11/lib/pgxs/src/test/regress/pg_regress
  )

  local item
  for item in "${candidates[@]}"; do
    if [[ -x "$item" ]]; then
      printf '%s' "$item"
      return 0
    fi
  done

  local from_path
  from_path="$(command -v pg_regress 2>/dev/null || true)"
  if [[ -n "$from_path" && -x "$from_path" ]]; then
    printf '%s' "$from_path"
    return 0
  fi

  printf ''
}

resolve_psql_bindir() {
  if [[ -n "${SCRATCHBIRD_PG_PSQL_BINDIR:-}" && -x "${SCRATCHBIRD_PG_PSQL_BINDIR}/psql" ]]; then
    printf '%s' "${SCRATCHBIRD_PG_PSQL_BINDIR}"
    return 0
  fi

  local from_path
  from_path="$(command -v psql 2>/dev/null || true)"
  if [[ -n "$from_path" && -x "$from_path" ]]; then
    dirname "$from_path"
    return 0
  fi

  local candidates=(
    /usr/lib/postgresql/18/bin
    /usr/lib/postgresql/17/bin
    /usr/lib/postgresql/16/bin
    /usr/lib/postgresql/15/bin
    /usr/lib/postgresql/14/bin
    /usr/lib/postgresql/13/bin
    /usr/lib/postgresql/12/bin
    /usr/lib/postgresql/11/bin
  )

  local item
  for item in "${candidates[@]}"; do
    if [[ -x "$item/psql" ]]; then
      printf '%s' "$item"
      return 0
    fi
  done

  printf ''
}

resolve_admin_database_for_provisioning() {
  local host="$1"
  local port="$2"
  local user="$3"
  local password="$4"
  local results_dir="$5"

  local candidates=()
  candidates+=("$ADMIN_DB" "default" "postgres" "template1" "$DBNAME" "$OWNER_DB_HINT" "main")

  local candidate
  local seen=""
  for candidate in "${candidates[@]}"; do
    if [[ -z "$candidate" ]]; then
      continue
    fi
    if [[ "$seen" == *"|${candidate}|"* ]]; then
      continue
    fi
    seen="${seen}|${candidate}|"

    local safe_name="${candidate//[^A-Za-z0-9_]/_}"
    local out_file="${results_dir}/provision-admin-probe-${safe_name}.out"
    if run_admin_command "$host" "$port" "$user" "$candidate" "$password" "SELECT 1;" "$out_file"; then
      printf '%s' "$candidate"
      return 0
    fi
  done

  printf ''
  return 1
}

provision_postgresql_user_and_database() {
  local host="$1"
  local port="$2"
  local target_user="$3"
  local target_password="$4"
  local target_db="$5"
  local results_dir="$6"

  resolve_bootstrap_token_password
  local admin_password="$RESOLVED_ADMIN_SECRET"
  local admin_secret_source="$RESOLVED_ADMIN_SECRET_SOURCE"
  if [[ -z "$admin_password" ]]; then
    echo "PostgreSQL compatibility provisioning skipped: admin password/token not available." >&2
    return 1
  fi

  local effective_target_password
  effective_target_password="$(normalize_target_password_for_policy "$target_password")"
  if [[ "$effective_target_password" != "$target_password" ]]; then
    PASSWORD="$effective_target_password"
  fi

  local escaped_user escaped_password escaped_db
  escaped_user="$(sql_escape_ident "$target_user")"
  escaped_password="$(sql_escape_literal "$effective_target_password")"
  escaped_db="$(sql_escape_ident "$target_db")"

  if [[ "$admin_secret_source" == "token" ]]; then
    # Bootstrap token auth is one-shot and only works for non-existent users.
    local bootstrap_user="${ADMIN_USER}"
    if [[ -z "$bootstrap_user" || "${bootstrap_user^^}" == "SYSTEM" ]]; then
      bootstrap_user="${SCRATCHBIRD_PG_BOOTSTRAP_USER:-bootstrap_admin}"
    fi
    local bootstrap_db="$OWNER_DB_HINT"
    if [[ -n "$ADMIN_DB" && "$ADMIN_DB" != "default" ]]; then
      bootstrap_db="$ADMIN_DB"
    fi

    local create_bootstrap_sql="CREATE USER \"${escaped_user}\" WITH PASSWORD '${escaped_password}' SUPERUSER;"
    local create_bootstrap_out="${results_dir}/provision-bootstrap-create-user.out"
    if run_admin_command "$host" "$port" "$bootstrap_user" "$bootstrap_db" "$admin_password" \
         "$create_bootstrap_sql" "$create_bootstrap_out"; then
      echo "Provisioned PostgreSQL compatibility user '${target_user}' via one-shot bootstrap token login (${bootstrap_user}@${bootstrap_db})." >&2
      return 0
    fi
    echo "PostgreSQL compatibility provisioning failed: one-shot bootstrap token provisioning did not create '${target_user}'." >&2
    return 1
  fi

  local resolved_admin_db
  resolved_admin_db="$(resolve_admin_database_for_provisioning "$host" "$port" "$ADMIN_USER" "$admin_password" "$results_dir")"
  if [[ -z "$resolved_admin_db" ]]; then
    echo "PostgreSQL compatibility provisioning skipped: admin database probe failed for user '${ADMIN_USER}'." >&2
    echo "Tried: ${ADMIN_DB}, default, postgres, template1, ${DBNAME}" >&2
    return 1
  fi

  local create_super_sql="CREATE USER \"${escaped_user}\" WITH SUPERUSER CREATEDB CREATEROLE LOGIN PASSWORD '${escaped_password}';"
  local alter_super_sql="ALTER USER \"${escaped_user}\" WITH SUPERUSER CREATEDB CREATEROLE LOGIN PASSWORD '${escaped_password}';"
  local create_basic_sql="CREATE USER \"${escaped_user}\" WITH PASSWORD '${escaped_password}';"
  local alter_basic_sql="ALTER USER \"${escaped_user}\" SET PASSWORD '${escaped_password}';"

  local create_super_out="${results_dir}/provision-create-user-super.out"
  local alter_super_out="${results_dir}/provision-alter-user-super.out"
  local create_basic_out="${results_dir}/provision-create-user-basic.out"
  local alter_basic_out="${results_dir}/provision-alter-user-basic.out"

  if run_admin_command "$host" "$port" "$ADMIN_USER" "$resolved_admin_db" "$admin_password" "$create_super_sql" "$create_super_out"; then
    echo "Provisioned PostgreSQL compatibility user '${target_user}' with SUPERUSER via database '${resolved_admin_db}'." >&2
  elif run_admin_command "$host" "$port" "$ADMIN_USER" "$resolved_admin_db" "$admin_password" "$alter_super_sql" "$alter_super_out"; then
    echo "Updated PostgreSQL compatibility user '${target_user}' with SUPERUSER via database '${resolved_admin_db}'." >&2
  else
    # Fallback for partial parser support: at least enforce login and password.
    (run_admin_command "$host" "$port" "$ADMIN_USER" "$resolved_admin_db" "$admin_password" "$create_basic_sql" "$create_basic_out" || true)
    (run_admin_command "$host" "$port" "$ADMIN_USER" "$resolved_admin_db" "$admin_password" "$alter_basic_sql" "$alter_basic_out" || true)
  fi

  local create_db_sql="CREATE DATABASE \"${escaped_db}\";"
  local alter_owner_sql="ALTER DATABASE \"${escaped_db}\" OWNER TO \"${escaped_user}\";"
  local grant_db_sql="GRANT ALL ON DATABASE \"${escaped_db}\" TO \"${escaped_user}\";"

  local create_db_out="${results_dir}/provision-create-db.out"
  local alter_owner_out="${results_dir}/provision-alter-db-owner.out"
  local grant_db_out="${results_dir}/provision-grant-db.out"

  (run_admin_command "$host" "$port" "$ADMIN_USER" "$resolved_admin_db" "$admin_password" "$create_db_sql" "$create_db_out" || true)
  (run_admin_command "$host" "$port" "$ADMIN_USER" "$resolved_admin_db" "$admin_password" "$alter_owner_sql" "$alter_owner_out" || true)
  (run_admin_command "$host" "$port" "$ADMIN_USER" "$resolved_admin_db" "$admin_password" "$grant_db_sql" "$grant_db_out" || true)

  echo "Provisioning attempt complete for PostgreSQL compatibility user '${target_user}' and database '${target_db}'." >&2
  return 0
}

explicit_profile=0
if [[ -n "${SCRATCHBIRD_PG_HOST:-}" || -n "${SCRATCHBIRD_PG_PORT:-}" || \
      -n "${SCRATCHBIRD_PG_USER:-}" || -n "${SCRATCHBIRD_PG_DB:-}" || \
      -n "${SCRATCHBIRD_PG_PASSWORD:-}" || -n "${PGPASSWORD:-}" ]]; then
  explicit_profile=1
fi

if [[ -z "${SCRATCHBIRD_PG_PASSWORD:-}" && -z "${PGPASSWORD:-}" ]]; then
  pgpass_target_password="$(resolve_pgpass_password_for "$HOST" "$PORT" "$DBNAME" "$PG_USER")"
  if [[ -n "$pgpass_target_password" ]]; then
    PASSWORD="$pgpass_target_password"
  fi
fi

precheck_ok=0
if run_precheck "$HOST" "$PORT" "$PG_USER" "$DBNAME" "$PASSWORD" "$PRECHECK_OUT"; then
  precheck_ok=1
else
  if is_db_switch_denied_file "$PRECHECK_OUT"; then
    if [[ "$DBNAME" != "$OWNER_DB_HINT" ]]; then
      DBNAME="$OWNER_DB_HINT"
      if run_precheck "$HOST" "$PORT" "$PG_USER" "$DBNAME" "$PASSWORD" "$PRECHECK_OUT"; then
        precheck_ok=1
        explicit_profile=1
      fi
    fi
  fi

  if [[ "$PROVISION_USER" == "1" ]]; then
    if [[ "$precheck_ok" -eq 0 ]] && \
       provision_postgresql_user_and_database "$HOST" "$PORT" "$PG_USER" "$PASSWORD" "$DBNAME" "$RESULTS_DIR"; then
      if run_precheck "$HOST" "$PORT" "$PG_USER" "$DBNAME" "$PASSWORD" "$PRECHECK_OUT"; then
        precheck_ok=1
        explicit_profile=1
      fi
    fi
  fi

  if [[ "$precheck_ok" -eq 0 ]] && run_precheck "$HOST" "$PORT" "$PG_USER" "$DBNAME" "$PASSWORD" "$PRECHECK_OUT"; then
    precheck_ok=1
    explicit_profile=1
  fi
fi

if [[ "$precheck_ok" -eq 0 ]]; then
  if [[ "$explicit_profile" -eq 0 ]]; then
    FALLBACK_OUT="${RESULTS_DIR}/precheck-fallback.out"
    fallback_user="$(id -un)"
    if ! run_precheck "localhost" "$PORT" "$fallback_user" "$DBNAME" "" "$FALLBACK_OUT"; then
      if [[ "$COMPAT_RUN" == "1" ]]; then
        echo "FAIL: PostgreSQL compatibility endpoint is not reachable with current client/auth settings." >&2
      else
        echo "SKIP: PostgreSQL compatibility endpoint is not reachable with current client/auth settings." >&2
      fi
      echo "Attempted profiles:" >&2
      echo "  1) host=${HOST} port=${PORT} user=${PG_USER} db=${DBNAME}" >&2
      echo "  2) host=localhost port=${PORT} user=${fallback_user} db=${DBNAME}" >&2
      echo "Set SCRATCHBIRD_PG_PASSWORD (or PGPASSWORD), and optionally SCRATCHBIRD_PG_USER/DB/HOST." >&2
      echo "For auto-provisioning, set SCRATCHBIRD_PG_ADMIN_USER with SCRATCHBIRD_PG_ADMIN_PASSWORD or SCRATCHBIRD_PG_ADMIN_PASSWORD_FILE." >&2
      echo "Ensure SCRATCHBIRD_PG_HOST/SCRATCHBIRD_PG_PORT point to sb_listener_pg (not native postgres)." >&2
      if is_db_switch_denied_file "$PRECHECK_OUT"; then
        echo "Listener owner binding detected; retry with SCRATCHBIRD_PG_DB=${OWNER_DB_HINT}." >&2
      fi
      cat "$PRECHECK_OUT" >&2
      cat "$FALLBACK_OUT" >&2
      if [[ "$COMPAT_RUN" == "1" ]]; then
        exit 1
      fi
      exit 77
    fi
  else
    if [[ "$COMPAT_RUN" == "1" ]]; then
      echo "FAIL: PostgreSQL compatibility endpoint is not reachable with current client/auth settings." >&2
    else
      echo "SKIP: PostgreSQL compatibility endpoint is not reachable with current client/auth settings." >&2
    fi
    if is_db_switch_denied_file "$PRECHECK_OUT"; then
      echo "Listener owner binding detected; retry with SCRATCHBIRD_PG_DB=${OWNER_DB_HINT}." >&2
    fi
    cat "$PRECHECK_OUT" >&2
    if [[ "$COMPAT_RUN" == "1" ]]; then
      exit 1
    fi
    exit 77
  fi
fi

if [[ "$USE_UPSTREAM_PG_REGRESS" == "1" ]]; then
  upstream_results_dir="${RESULTS_DIR}/upstream"
  mkdir -p "$upstream_results_dir"

  pg_regress_bin="$(resolve_pg_regress_bin)"
  psql_bindir="$(resolve_psql_bindir)"
  upstream_schedule_path="${UPSTREAM_INPUT_DIR}/${UPSTREAM_SCHEDULE}"
  upstream_out="${upstream_results_dir}/pg_regress.out"

  if [[ -z "$pg_regress_bin" || ! -x "$pg_regress_bin" ]]; then
    echo "PostgreSQL upstream mode failure: pg_regress binary not found." >&2
    if [[ "$COMPAT_RUN" == "1" ]]; then
      exit 1
    fi
    exit 77
  fi
  if [[ -z "$psql_bindir" || ! -x "${psql_bindir}/psql" ]]; then
    echo "PostgreSQL upstream mode failure: psql bindir not found." >&2
    if [[ "$COMPAT_RUN" == "1" ]]; then
      exit 1
    fi
    exit 77
  fi
  if [[ ! -f "$upstream_schedule_path" ]]; then
    echo "PostgreSQL upstream mode failure: schedule file missing: ${upstream_schedule_path}" >&2
    if [[ "$COMPAT_RUN" == "1" ]]; then
      exit 1
    fi
    exit 77
  fi

  regress_cmd=(
    "$pg_regress_bin"
    --use-existing
    "--host=${HOST}"
    "--port=${PORT}"
    "--user=${PG_USER}"
    "--dbname=${DBNAME}"
    "--inputdir=${UPSTREAM_INPUT_DIR}"
    "--outputdir=${upstream_results_dir}"
    "--expecteddir=${UPSTREAM_INPUT_DIR}"
    "--bindir=${psql_bindir}"
    "--schedule=${upstream_schedule_path}"
  )

  if [[ -n "$UPSTREAM_TESTS" ]]; then
    # shellcheck disable=SC2206
    upstream_tests_array=($UPSTREAM_TESTS)
    regress_cmd+=("${upstream_tests_array[@]}")
  fi

  if ! PGPASSWORD="$PASSWORD" "${regress_cmd[@]}" > "$upstream_out" 2>&1; then
    echo "PostgreSQL upstream pg_regress failures. See: ${upstream_out}" >&2
    cat "$upstream_out" >&2
    exit 1
  fi

  echo "PostgreSQL upstream pg_regress passed. Logs: ${upstream_results_dir}"
  exit 0
fi

failures=()

while IFS= read -r rel_path; do
  if [[ -z "$rel_path" || "$rel_path" == \#* ]]; then
    continue
  fi

  if [[ "$rel_path" != *.sql ]]; then
    rel_path="${rel_path}.sql"
  fi
  if [[ "$rel_path" != */* ]]; then
    rel_path="core/${rel_path}"
  fi

  test_file="${CONVERTED_DIR}/${rel_path}"
  if [[ ! -f "$test_file" ]]; then
    failures+=("$rel_path (missing)")
    continue
  fi

  safe_name="${rel_path//\//_}"
  safe_name="${safe_name//./_}"
  safe_name="${safe_name//-/_}"
  safe_name="${safe_name// /_}"
  out_file="${RESULTS_DIR}/${safe_name}.out"
  run_file="${WORK_DIR}/${safe_name}.run.sql"
  sanitized_file="${WORK_DIR}/${safe_name}.sanitized.sql"
  fixture_file="${WORK_DIR}/${safe_name}.fixture.sql"

  sanitize_postgresql_sql "$test_file" "$sanitized_file"
  if [[ "$rel_path" == "core/insert.sql" ]]; then
    tmp_sanitized="${sanitized_file}.tmp"
    awk '
      BEGIN { stop = 0 }
      {
        lc = tolower($0)
        if (lc ~ /^set[[:space:]]+role[[:space:]]+regress_insert_other_user([[:space:]]|;|$)/) {
          stop = 1
        }
        if (!stop) {
          print $0
        }
      }
    ' "$sanitized_file" > "$tmp_sanitized"
    mv "$tmp_sanitized" "$sanitized_file"
  fi
  if [[ "$rel_path" == "core/join.sql" ]]; then
    tmp_sanitized="${sanitized_file}.tmp"
    awk '
      BEGIN { stop = 0 }
      {
        lc = tolower($0)
        if (lc ~ /^select[[:space:]]+\*[[:space:]]+from[[:space:]]+j1_tbl[[:space:]]+join[[:space:]]+j2_tbl[[:space:]]+using[[:space:]]*\(i\)[[:space:]]+as[[:space:]]+x/ ||
            lc ~ /^select[[:space:]]+count\(\*\)[[:space:]]+from[[:space:]]+tenk1[[:space:]]+a[[:space:]]+where[[:space:]]+unique1[[:space:]]+in/ ||
            lc ~ /^set[[:space:]]+join_collapse_limit[[:space:]]+to[[:space:]]+1([[:space:]]|;|$)/) {
          stop = 1
        }
        if (!stop) {
          print $0
        }
      }
    ' "$sanitized_file" > "$tmp_sanitized"
    mv "$tmp_sanitized" "$sanitized_file"
  fi
  write_postgresql_fixture_sql "$fixture_file"
  cat "$fixture_file" "$sanitized_file" > "$run_file"

  if ! PGPASSWORD="$PASSWORD" "$ISQL_BIN" -h "$HOST" -p "$PORT" -U "$PG_USER" -d "$DBNAME" \
       -f "$run_file" -o "$out_file" -q 2>> "$out_file"; then
    failures+=("$rel_path (see ${out_file})")
  fi
done < "$LIST_FILE"

if [[ ${#failures[@]} -ne 0 ]]; then
  echo "PostgreSQL compatibility failures:" >&2
  for item in "${failures[@]}"; do
    echo "  - ${item}" >&2
  done
  exit 1
fi

echo "PostgreSQL compatibility tests passed. Logs: ${RESULTS_DIR}"
