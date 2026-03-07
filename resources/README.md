# ScratchBird Resources Directory

This directory contains data files and configuration resources used by ScratchBird.

## Directory Structure

```
resources/
├── bootstrap/       # Development/test auth bootstrap manifest
├── config/          # Configuration files
├── timezones/       # IANA timezone database
├── charsets/        # Character set definitions
├── collations/      # Collation (sorting) rules
└── i18n/            # i18n resource version marker
```

---

## Bootstrap Auth (`bootstrap/`)

### `default_auth_manifest.json`

Installable development/test credential manifest for native ScratchBird and
the emulated engine compatibility surfaces.

Runtime use:
- `src/core/database.cpp` loads the ScratchBird entries from this manifest
  during database open/bootstrap.
- Only entries marked with `"seed_on_database_bootstrap": true` are created
  automatically at core database bootstrap time.
- Additional engine defaults remain here for example/test build scripts and
  compatibility harnesses.

See also `docs/DEFAULT_TEST_ENGINE_CREDENTIALS.md`.

---

## Configuration Files (`config/`)

### `sb_config.ini`
Main ScratchBird configuration file.

**Usage:**
```bash
# Specify config file when starting server
./sb_server --config resources/config/sb_config.ini

# Or use default location
cp resources/config/sb_config.ini ~/.scratchbird/config.ini
```

---

## Timezone Data (`timezones/`)

### IANA Timezone Database

Contains the official IANA timezone database (tzdata) used for temporal operations.

**Files:**
- `tzdata2024b.tar.gz` - IANA timezone data (source format)
- `tzcode2024b.tar.gz` - IANA timezone compiler
- `africa`, `asia`, `europe`, etc. - Regional timezone definitions
- `leapseconds` - Leap second data
- `zone.tab` - Timezone coordinate table

**Version:** 2025c

**Loading timezones:**
```bash
# Load from source files
./sb_timezone_loader /path/to/database.sb --from resources/timezones

# Or use system zoneinfo (if available)
./sb_timezone_loader /path/to/database.sb --from /usr/share/zoneinfo
```

**Updating:**
```bash
cd resources/timezones
# Replace 2025c with the latest release from the IANA directory listing.
curl -L -O https://data.iana.org/time-zones/releases/tzdata2025c.tar.gz
curl -L -O https://data.iana.org/time-zones/releases/tzcode2025c.tar.gz
tar -xzf tzdata2025c.tar.gz
tar -xzf tzcode2025c.tar.gz

# Reload into database
./sb_timezone_loader /path/to/database.sb --from resources/timezones --stats
```

**References:**
- IANA Time Zone Database: https://www.iana.org/time-zones
- tzdata releases: https://data.iana.org/time-zones/releases/

---

## Character Sets (`charsets/`)

### Supported Character Sets

Contains the merged Firebird/MySQL/PostgreSQL charset baseline. The resource
JSON is generated via `ScratchBird/resources/scripts/update_i18n_resources.py`.

**Categories:**
- **Unicode:** UTF-8, UTF-16, UTF-32
- **Western European:** ISO-8859-1/15, Windows-1252, MacRoman
- **Central/Eastern European:** ISO-8859-2/3/4, Windows-1250/1251
- **Asian:** Shift_JIS, EUC-JP, GB2312, GBK, GB18030, Big5, EUC-KR
- **Other:** KOI8-R/U (Russian), TIS-620 (Thai), Arabic, Hebrew

**Files:**
- `charsets.json` - Master character set definitions
- `README.md` - Detailed character set documentation

**Loading character sets:**
```bash
# Load built-in character sets only
./sb_charset_loader /path/to/database.sb --builtin

# Load all character sets + collations from resources/
./sb_charset_loader /path/to/database.sb --all
```

**Database Engine Coverage (Target Baseline):**
- PostgreSQL: 25+ character sets
- MySQL/MariaDB: 15+ character sets
- SQL Server: 20+ code pages
- Oracle: 30+ character sets
- Firebird: 10+ character sets
See `ScratchBird/docs/specifications/types/character_sets_and_collations.md`
and `ScratchBird/docs/findings/RESOURCES_I18N_TIMEZONE_AUDIT.md`.

---

## Collations (`collations/`)

### Collation Rules

Contains collation (sorting/comparison) rules for text processing, including
Firebird/MySQL baselines plus PostgreSQL locale collations.

**Files:**
- `collations.json` - Master collation definitions

**Collation Types:**
- **Case-insensitive (CI):** `utf8_general_ci`, `latin1_general_ci`
- **Binary:** `utf8_bin`, `latin1_bin`
- **Accent-insensitive (AI):** `utf8mb4_0900_ai_ci`
- **Locale-specific:** `en_US.UTF-8`, `ja_JP.UTF-8`

**Database Engine Collations:**
- MySQL: `utf8mb4_general_ci`, `utf8mb4_unicode_ci`, `utf8mb4_0900_ai_ci`
- PostgreSQL: `en_US.UTF-8`, `C`, `POSIX`
- SQL Server: `Latin1_General_CI_AS`, `SQL_Latin1_General_CP1_CI_AS`
- Firebird: `UTF8_UNICODE`, `UTF8_UNICODE_CI`, `WIN1252_UNICODE`
- Oracle: `AL32UTF8`, `WE8MSWIN1252`, `WE8ISO8859P1`

**Loading collations:**
```bash
# Collations are loaded automatically with character sets
./sb_charset_loader /path/to/database.sb --builtin
```

---

## i18n Version (`i18n/`)

`resources/i18n/version` records the current resource bundle version used for
catalog version checks. The loader tools and database bootstrap record this
value in catalog metadata for drift detection.

---

## Data File Formats

### Timezone Files (TZif Format)

IANA timezone files use the TZif (Time Zone Information Format) binary format:

```
Header (44 bytes)
├── Magic: "TZif"
├── Version: 0x32 (version 2) or 0x33 (version 3)
├── Transition times (int64_t array)
├── Transition types (offset, DST flag, abbreviation)
├── Leap seconds
└── POSIX TZ string (for future dates)
```

See `docs/planning/RESOURCES_I18N_TIMEZONE_REMEDIATION_PLAN.md` for the
current resource-loading remediation plan.

### Character Set Files (JSON Format)

```json
{
  "name": "UTF-8",
  "description": "Unicode Transformation Format, 8-bit",
  "aliases": ["utf8", "UTF8"],
  "max_bytes": 4,
  "min_bytes": 1,
  "is_variable_width": true,
  "encoding_type": "unicode",
  "iana_name": "UTF-8",
  "supported_by": ["PostgreSQL", "MySQL", "SQL Server", "Oracle", "Firebird"]
}
```

### Collation Files (JSON Format)

```json
{
  "name": "utf8_general_ci",
  "charset": "UTF-8",
  "case_insensitive": true,
  "accent_insensitive": false,
  "language": "",
  "description": "UTF-8 Unicode general case-insensitive",
  "supported_by": ["MySQL", "MariaDB"]
}
```

### UCA Weight Tables

Unicode Collation Algorithm weight tables live under:

```
resources/collations/uca/allkeys.txt
resources/collations/uca/uca_manifest.json
```

Tailoring data (MySQL/Firebird/locale-specific) is expected under
`resources/collations/tailorings/`. Current files include:
- MySQL collation XML definitions (from MySQL 8.x source)
- Firebird collation tables (from Firebird 6 source)
- Stub list files for defaults (legacy loader tests)

---

## Usage Examples

### Loading All Data on Database Initialization

```bash
# Create new database
./sb_init /data/mydb.sb

# Load timezones
./sb_timezone_loader /data/mydb.sb --from resources/timezones

# Load character sets and collations
./sb_charset_loader /data/mydb.sb --builtin

# Verify data loaded
./sb_isql /data/mydb.sb
> SELECT COUNT(*) FROM pg_timezone;
> SELECT COUNT(*) FROM pg_charsets;
> SELECT COUNT(*) FROM pg_collations;
```

### Querying Loaded Data

```sql
-- List all timezones
SELECT tz_name, posix_string FROM pg_timezone ORDER BY tz_name;

-- Find timezone transitions (DST changes)
SELECT tz.tz_name, tt.transition_time, tt.utc_offset, tt.is_dst, tt.abbreviation
FROM pg_timezone tz
JOIN pg_timezone_transitions tt ON tz.tz_id = tt.tz_id
WHERE tz.tz_name = 'America/New_York'
ORDER BY tt.transition_time DESC
LIMIT 10;

-- List all character sets
SELECT charset_name, description, max_bytes, is_variable_width
FROM pg_charsets
ORDER BY charset_name;

-- List all collations for UTF-8
SELECT c.collation_name, c.case_insensitive, c.accent_insensitive
FROM pg_collations c
JOIN pg_charsets cs ON c.charset_id = cs.charset_id
WHERE cs.charset_name = 'UTF-8'
ORDER BY c.collation_name;
```

---

## Maintenance

### Updating Timezone Data

IANA releases timezone updates several times per year. To update:

```bash
cd resources/timezones

# Download latest version
curl -L -O https://data.iana.org/time-zones/releases/tzdata2025c.tar.gz

# Extract
tar -xzf tzdata2025c.tar.gz

# Reload into database
./sb_timezone_loader /path/to/database.sb --from resources/timezones --stats
```

**Update frequency:** Check https://www.iana.org/time-zones 2-4 times per year

### Adding Custom Character Sets

To add custom character sets not included in the default set:

1. Create a JSON file in `resources/charsets/` following the format in `charsets.json`
2. Run the loader:
   ```bash
   ./sb_charset_loader /path/to/database.sb --json resources/charsets/custom_charset.json
   ```

### Mapping Validation

Validate system-derived mapping tables before loader implementation:

```bash
./resources/scripts/validate_charset_mappings.py
```

---

## References

- IANA Time Zone Database: https://www.iana.org/time-zones
- IANA Character Sets: https://www.iana.org/assignments/character-sets/
- Unicode Standard: https://www.unicode.org/
- ICU Project: https://icu.unicode.org/
- PostgreSQL Character Sets: https://www.postgresql.org/docs/current/multibyte.html
- MySQL Character Sets: https://dev.mysql.com/doc/refman/8.0/en/charset.html
- SQL Server Collations: https://learn.microsoft.com/en-us/sql/relational-databases/collations/
- Oracle Character Sets: https://docs.oracle.com/en/database/oracle/oracle-database/19/nlspg/
- Firebird Character Sets: https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref40/firebird-40-language-reference.html#fblangref40-appx-charsets

---

**Last Updated:** January 28, 2026
