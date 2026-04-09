# PostgreSQL 18 Text Search Configuration Specification

## Purpose
Define the full PostgreSQL text search configuration pipeline, including the default parser, dictionary templates, and the built-in language configurations defined in this specification. This specification is authoritative for PostgreSQL 18 emulation.

## Scope
This spec defines:
- The `default` text search parser token categories and deterministic tokenization rules.
- Dictionary templates: `simple`, `snowball`, `synonym`, `thesaurus`, and `ispell` (including Hunspell affix parsing).
- The `simple` and Snowball language configurations, including exact mapping of token types to dictionaries.
- Stopword handling and internal catalog storage rules.

## Execution Location
- All PostgreSQL text search parsing and dictionary processing runs in the **parser layer** for PG emulation.
- The engine receives canonical lexemes and positions via SBLR and does not implement SQL dialect logic.

## Parser: `default` (pg_catalog.default)
The `default` parser emits token types listed below. Tokenization uses maximal munch and left-to-right scanning. If multiple token patterns match at the same position, the **priority order** is:
`email` → `url` → `host` → `file` → `version` → `sfloat` → `float` → `int` → `uint` → `numhword` → `hword` → `asciihword` → `numword` → `word` → `asciiword` → `xmlentity` → `tag` → `space`.

### Character Classes
- `ALPHA_ASCII`: `[A-Za-z]`
- `DIGIT_ASCII`: `[0-9]`
- `ALNUM_ASCII`: `[A-Za-z0-9]`
- `ALPHA_UNI`: Unicode letters (general category `L`).
- `DIGIT_UNI`: Unicode decimal digits (general category `Nd`).
- `ALNUM_UNI`: `ALPHA_UNI` or `DIGIT_UNI`.
- `HYPHEN`: `-`
- `DOT`: `.`
- `UNDERSCORE`: `_`
- `APOSTROPHE`: `'`
- `AT`: `@`
- `COLON`: `:`
- `SLASH`: `/`
- `BACKSLASH`: `\\`

### Token Types (alias names)
Token type aliases are fixed:
`asciiword`, `word`, `numword`, `email`, `url`, `host`, `sfloat`, `version`,
`hword_numpart`, `hword_part`, `hword_asciipart`, `blank`, `tag`, `protocol`,
`numhword`, `asciihword`, `hword`, `url_path`, `file`, `float`, `int`, `uint`, `entity`.

### Regex Definitions (Deterministic)
All regexes are anchored at the current scan position. Implementers must scan left-to-right and emit the **longest** matching token for the highest-priority rule.

- `email`:  
  `local@domain`  
  `local = (ALNUM_ASCII|[._%+-])+`  
  `domain = label (DOT label)+`  
  `label = ALNUM_ASCII (ALNUM_ASCII|HYPHEN){0,61} ALNUM_ASCII`  
- `url`:  
  `scheme://host[:port][/path]`  
  `scheme = ALPHA_ASCII (ALNUM_ASCII|[+.-])*`  
  `port = DIGIT_ASCII+`  
  `path = (SLASH [^\\s]*)?`  
- `protocol`:  
  `scheme://` portion of a URL; emitted as a separate token when `url` matches.  
- `url_path`:  
  The `/path` portion of a URL; emitted as a separate token when `url` matches and a path exists.  
- `host`:  
  `label (DOT label)+` with label defined above  
- `file`:  
  `([A-Za-z]:)?(SLASH|BACKSLASH)([^\\s]+)` or `~/(.+)`  
- `version`:  
  `DIGIT_ASCII+ (DOT DIGIT_ASCII+)+`  
- `sfloat`:  
  `[+-]? DIGIT_ASCII+ (DOT DIGIT_ASCII+)? [eE] [+-]? DIGIT_ASCII+`  
- `float`:  
  `[+-]? DIGIT_ASCII+ DOT DIGIT_ASCII+`  
- `int`:  
  `[+-]? DIGIT_ASCII+`  
- `uint`:  
  `DIGIT_ASCII+` (only when no leading sign and not captured by `version`/`float`)  
- `numword`:  
  `ALNUM_UNI+` containing at least one digit and at least one letter  
- `word`:  
  `ALPHA_UNI+` with at least one non-ASCII letter  
- `asciiword`:  
  `ALPHA_ASCII+` only  
- `hword`, `numhword`, `asciihword`:  
  `part (HYPHEN part)+`  
  - `hword`: each `part` is `ALPHA_UNI+`, at least one part contains non-ASCII  
  - `asciihword`: each `part` is `ALPHA_ASCII+`  
  - `numhword`: parts use `ALNUM_UNI+` with at least one digit  
- `hword_part`, `hword_asciipart`, `hword_numpart`:  
  When a `hword` token is emitted, also emit each part as a separate token with the corresponding part type. The part text is the substring for that segment.
- `tag`:  
  `<...>` XML tag contents, emitted without `<` or `>`  
- `entity`:  
  XML entities `&name;` or `&#nnn;` or `&#xhhhh;`  
- `blank`:  
  contiguous whitespace

### Token Output Rules
- `blank` tokens are ignored by the dictionary pipeline.
- Hyphenated tokens emit both the whole token (`hword`/`asciihword`/`numhword`) and each part (`hword_part`/`hword_asciipart`/`hword_numpart`).
- When a `url` is matched:
  - Emit `protocol` token for the scheme (without `://`).
  - Emit `url` token for the full URL.
  - Emit `url_path` token for the path portion if present.

## Dictionary Templates

### `simple` Dictionary
Options:
- `StopWords`: language key in `tsearch_stopwords` (default empty stoplist).
- `Accept` (bool, default true).

Algorithm:
1. Lowercase input with `str_tolower` (locale-aware).
2. If `StopWords` is not set, use an empty stoplist.
3. If empty or in stoplist, return a **stopword** (lexeme array with first element NULL).
4. If `Accept=true`, return lexeme array containing the lowercased token.
5. If `Accept=false`, return NULL (unrecognized).

### `snowball` Dictionary
Options:
- `Language` (required): one of the supported Snowball languages listed below.
- `StopWords`: language key in `tsearch_stopwords` (default empty stoplist).

Algorithm:
1. Lowercase input with `str_tolower` (locale-aware).
2. If `StopWords` is not set, use an empty stoplist.
3. If empty or in stoplist, return stopword (lexeme array with first element NULL).
4. Apply Snowball stemmer for the specified language and database encoding.
5. Return lexeme array containing the stem.

Supported languages (from PostgreSQL 18 Snowball build):
`arabic, armenian, basque, catalan, danish, dutch, english, esperanto, estonian, finnish, french, german, greek, hindi, hungarian, indonesian, irish, italian, lithuanian, nepali, norwegian, polish, portuguese, romanian, russian, serbian, spanish, swedish, tamil, turkish, yiddish`.

Stopword files exist for:
`danish, dutch, english, finnish, french, german, hungarian, italian, nepali, norwegian, portuguese, russian, spanish, swedish, turkish`.

### `synonym` Dictionary
Options:
- `Synonyms` (required): `dict_name` key in `tsearch_synonyms`.
- `CaseSensitive` (bool, default false).

File format (`.syn`):
- The catalog stores each line of the `.syn` format as a row in `tsearch_synonyms`.
- Each non-empty, non-comment line contains two whitespace-separated words:
  - `input output`
- If `output` ends with `*`, set `TSL_PREFIX` flag for that synonym.
- Only the first two tokens on the line are used; extra tokens are ignored.
- Trailing comments after the second token are permitted and ignored.

Algorithm:
1. Read `.syn` file, build sorted array by `input`.
2. On lookup:
   - If `CaseSensitive=false`, lowercase input before lookup.
   - If match found, return output lexeme (with `TSL_PREFIX` if flagged).
   - If no match, return NULL (unrecognized).

### `thesaurus` Dictionary
Options:
- `DictFile` (required): `dict_name` key in `tsearch_thesaurus`.
- `Dictionary` (required): subdictionary name used for normalization.

File format (`.ths`):
- The catalog stores each raw line of the `.ths` format as a row in `tsearch_thesaurus`.
- `sample_phrase : substitute_phrase`
- `sample_phrase` is a sequence of lexemes separated by whitespace.
- `substitute_phrase` is a sequence of lexemes separated by whitespace.
- Lines starting with `#` or empty lines are ignored.
- Special markers:
  - `?` in `sample_phrase` represents a stopword placeholder.
  - A `*` prefix on a substitute lexeme means **use-as-is** (no subdictionary lexize).
  - A `\\` prefix forces normal lexize for lexemes that would otherwise be ambiguous.

Algorithm:
1. Parse rules into sample lexeme sequences and substitution sequences.
2. Normalize sample lexemes through the subdictionary:
   - If a sample lexeme is not recognized, error.
   - If a sample lexeme is a stopword, use `?` instead.
3. Normalize substitute lexemes:
   - If `use-as-is`, keep the literal lexeme.
   - Otherwise lexize through subdictionary; error if unrecognized or stopword.
4. During lexize, match multi-token sequences against rules and substitute when a rule matches.
5. Preserve positional flags using `TSL_ADDPOS` for multiword substitutions.

### `ispell` Dictionary (Ispell/Hunspell)
Template: `ispell`

Options:
- `DictFile` (required): `dict_name` key in `tsearch_ispell_dict`.
- `AffFile` (required): `dict_name` key in `tsearch_ispell_affix`.
- `StopWords`: language key in `tsearch_stopwords` (default empty stoplist).

#### `.dict` File Format
- The catalog stores each `.dict` line as a row in `tsearch_ispell_dict`.
- Each non-empty line: `word[/flags]`
- `word` is the base lexeme.
- `flags` is a sequence of affix flags (single characters in Ispell mode, or multi-character/number in Hunspell mode).
- Trailing comments after whitespace are permitted and ignored.
- Dictionary words are lowercased during import.

#### `.affix` File Formats
Two formats are supported. The parser must detect which format is used; mixing formats in one file is an error.
- The catalog stores each raw `.affix` line as a row in `tsearch_ispell_affix` and preserves line order by `line_no`.

##### Affix Mask Engines
Affix masks are evaluated using one of two engines:
1. **Regis subset** if the mask matches the `RS_isRegis` rules (see below).
2. **Regex subset** otherwise.

#### Regis Pattern Syntax (used by Ispell)
Regis is a minimal pattern language used for affix masks.
- Pattern is a sequence of units.
- Each unit is either:
  - A single alphabetic character (Unicode letter).
  - A bracket class: `[abc]` or `[^abc]` where `abc` are alphabetic characters.
- No ranges, no quantifiers, no wildcards, no escapes.
- Matching is by exact-length comparison against the **prefix** (for prefixes) or **suffix** (for suffixes).

#### Regex Subset (used when mask is not regis)
If a mask is not regis-compatible, apply this regex subset:
- Literals: alphabetic characters.
- Character classes: `[abc]` and `[^abc]`.
- Wildcard: `.` matches exactly one Unicode letter.
- Anchors: `^` and `$` are permitted and must match start/end of the word segment.
- No quantifiers (`*`, `+`, `?`, `{}`), no alternation (`|`), no groups.
If a mask contains unsupported regex features, reject it with configuration error.

##### Ispell (Old) Format
Sections and commands:
- `compoundwords controlled <flag>`: enables compound word support with flag.
- `prefixes` / `suffixes`: switches rule section.
- `flag <flag>:`: declares a new rule group; `*` indicates crossproduct; `~` indicates compound-only.
- Rule line format:  
  `<mask> > [-<strip>,]<add>`  
  - `<mask>` is a character class or pattern.
  - `<strip>` is removed from the base word.
  - `<add>` is added.

##### Hunspell (New) Format
Header directives:
- `FLAG <default|long|num>` sets flag mode.
- `COMPOUNDFLAG`, `COMPOUNDBEGIN`, `COMPOUNDMIDDLE`, `COMPOUNDLAST`, `COMPOUNDEND`, `ONLYINCOMPOUND`, `COMPOUNDPERMITFLAG`, `COMPOUNDFORBIDFLAG` define compound rules.
- `AF <N>` declares `N` flag aliases; following `N` lines define aliases.

Affix rules:
- Header: `PFX <flag> <cross> <count>` or `SFX <flag> <cross> <count>`
- Rule: `PFX <flag> <strip> <add> <condition>` or `SFX <flag> <strip> <add> <condition>`
- `<strip>` or `<add>` is `0` to indicate empty string.
- If `<add>` includes `/flags`, treat those flags as compound flags to be applied.

#### Ispell/Hunspell Lexize Algorithm (Deterministic)
1. Lowercase input with locale-aware `str_tolower`.
2. If `StopWords` is not set, use an empty stoplist.
3. If input is a stopword, return stopword (lexeme array with first element NULL).
4. Check if input exists in dictionary trie with compatible affix flags; if yes, emit the base word.
5. Generate candidates by applying matching prefix and suffix rules:
   - A rule matches when the word satisfies the rule mask and strip/replacement criteria.
   - If crossproduct is enabled for a flag, generate all valid prefix+suffix combinations for that flag.
6. For each candidate base word, verify it exists in the dictionary trie and that the required affix flags are present.
7. If compound rules are enabled, attempt to split the word into components:
   - Each component must be valid by steps 3–5.
   - Compound flags must satisfy `COMPOUNDBEGIN`, `COMPOUNDMIDDLE`, `COMPOUNDLAST` rules.
8. Emit all validated base forms as lexemes.
9. If no candidates match, return NULL (unrecognized).

#### Hunspell Flag Modes
When `FLAG` is present:
- `FLAG default`: flags are single characters.
- `FLAG long`: flags are two-character sequences.
- `FLAG num`: flags are numeric strings, 0..65535.

#### Hunspell `AF` Flag Alias Handling
- If `AF N` is present, then the next `N` lines define flag aliases.
- Alias index 0 is reserved for the empty flag set.
- When a dictionary entry uses a numeric alias, map it to its alias string before processing.

## Built-in Configurations (Snowball)
All Snowball language configurations are generated from deterministic bootstrap seed rules in this specification and use the same mapping rules.

### Mapping Rules (for each language config)
1. `email, url, url_path, host, file, version, sfloat, float, int, uint, numword, hword_numpart, numhword` → `simple`
2. `asciiword, hword_asciipart, asciihword` → `<ascii_lang>_stem`
3. `word, hword_part, hword` → `<lang>_stem`

`<ascii_lang>` equals `<lang>` for all languages except:
- `hindi` → `english`
- `russian` → `english`

### Language Configurations
Each language creates:
- Dictionary: `<lang>_stem` with `Language = <lang>` and `StopWords = <lang>` (if omitted, stoplist is empty).
- Configuration: `<lang>` with `PARSER = default` and the mapping rules above.

### `simple` Configuration
`pg_catalog.simple` is defined as:
- `PARSER = default`
- All token types map to `simple`.

## File Location and Naming Rules
ScratchBird does **not** read external files for PG emulation. Instead, all dictionary data is stored in internal catalog tables and loaded during database creation.

### Internal Catalog Tables
- `tsearch_stopwords`:
  - `lang` (text), `word` (text).
- `tsearch_synonyms`:
  - `dict_name` (text), `input` (text), `output` (text), `prefix_flag` (bool), `case_sensitive` (bool).
- `tsearch_thesaurus`:
  - `dict_name` (text), `sample_phrase` (text), `substitute_phrase` (text).
- `tsearch_ispell_dict`:
  - `dict_name` (text), `entry` (text), `flags` (text).
- `tsearch_ispell_affix`:
  - `dict_name` (text), `line_no` (u32), `raw_line` (text).

### Built-in Seed Data
- Stopword lists are embedded and loaded from `FULLTEXT_PG_STOPWORDS_DATA.md`.
- Snowball language configs and dictionaries are created at database initialization and stored in `tsearch_*` tables.
- Synonym and thesaurus dictionaries are not created by default; they are user-loaded via DDL that inserts into the catalog tables.

### Name Validation
- Dictionary and configuration names are restricted to `[a-z0-9_]+` for PG emulation compatibility.

## Error Handling
- Missing configuration data in catalog tables raises `ERRCODE_CONFIG_FILE_ERROR`.
- Unknown dictionary parameters raise `ERRCODE_INVALID_PARAMETER_VALUE`.
- Mixed old/new affix formats in one dataset raise `ERRCODE_CONFIG_FILE_ERROR`.
- Thesaurus rules with unrecognized or stopword lexemes raise `ERRCODE_CONFIG_FILE_ERROR`.
- Missing catalog data for a referenced `dict_name` or `lang` raises `ERRCODE_CONFIG_FILE_ERROR`.

## Test Contract (PostgreSQL Emulation)
- `english` config uses `english_stem` with `StopWords=english` and the standard mapping rules.
- `hindi` config maps ASCII tokens to `english_stem`.
- Hyphenated tokens emit both `hword` and part tokens.
- `synonym` dictionary respects `CaseSensitive` and `*` prefix handling.
- `thesaurus` dictionary supports `?` stopword placeholders and `*` use-as-is.
- Ispell/Hunspell sample dictionaries parse and produce lexemes consistent with PostgreSQL 18 dictionary behavior for equivalent inputs.
