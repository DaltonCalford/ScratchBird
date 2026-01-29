# Character Set Definitions

This directory contains character set definition files used by ScratchBird for text processing and storage.

## Supported Character Sets

Note: This list represents the current baseline. The resource JSON is generated
from Firebird Appendix H plus MySQL/PostgreSQL sources via
`ScratchBird/resources/scripts/update_i18n_resources.py`. See
`ScratchBird/docs/findings/RESOURCES_I18N_TIMEZONE_AUDIT.md` and
`ScratchBird/docs/specifications/types/character_sets_and_collations.md` for
coverage requirements and maintenance guidance.

### Unicode Encodings (Built-in - No files required)
- **UTF-8** - Variable width (1-4 bytes), default for PostgreSQL, MySQL 8.0+
- **UTF-16** - Variable width (2 or 4 bytes)
- **UTF-32** - Fixed width (4 bytes)
- **UNICODE_FSS** - Firebird legacy UTF-8 variant (1-3 bytes), treated as UTF-8 with a 3-byte ceiling

### Western European
- **ASCII** - 7-bit ASCII (0x00-0x7F), built-in
- **ISO-8859-1** (Latin-1) - Western European, built-in
- **ISO-8859-15** (Latin-9) - Western European with Euro symbol
- **Windows-1252** (CP1252) - Microsoft Western European
- **MacRoman** - Classic Macintosh encoding

### Central/Eastern European
- **ISO-8859-2** (Latin-2) - Central European
- **ISO-8859-3** (Latin-3) - South European
- **ISO-8859-4** (Latin-4) - North European
- **Windows-1250** (CP1250) - Microsoft Central European
- **Windows-1251** (CP1251) - Microsoft Cyrillic

### Asian Encodings
- **Shift_JIS** - Japanese (Microsoft/IBM)
- **EUC-JP** - Japanese (Unix)
- **ISO-2022-JP** - Japanese (email)
- **GB2312** - Simplified Chinese
- **GBK** - Extended GB2312
- **GB18030** - Chinese national standard
- **Big5** - Traditional Chinese (Taiwan)
- **EUC-KR** - Korean
- **EUC-TW** - Traditional Chinese (Taiwan)

### Other
- **KOI8-R** - Russian (Cyrillic)
- **KOI8-U** - Ukrainian (Cyrillic)
- **TIS-620** - Thai
- **Windows-1256** (CP1256) - Arabic
- **Windows-1255** (CP1255) - Hebrew

## Database Engine Support

### PostgreSQL
- Default: UTF-8
- Supports: All Unicode encodings, SQL_ASCII, ISO-8859-*, Windows-*, EUC-*, KOI8-*

### MySQL/MariaDB
- Default: utf8mb4 (MySQL 8.0+), latin1 (older)
- Supports: utf8, utf8mb4, latin1, ascii, ucs2, utf16, utf32, binary

### Microsoft SQL Server
- Default: UTF-16 (nvarchar), Windows-1252 (varchar)
- Supports: All Windows code pages, UTF-8, UTF-16

### Firebird
- Default: NONE (bytes), UTF-8, WIN1252
- Supports: UTF-8, ISO-8859-*, Windows-*, DOS code pages

### Oracle
- Default: AL32UTF8 (UTF-8), WE8MSWIN1252
- Supports: All Unicode, ISO-8859-*, Windows-*, national character sets

## File Format

Character set files use the following JSON format:

```json
{
  "name": "ISO-8859-1",
  "description": "Latin-1 Western European",
  "aliases": ["latin1", "ISO_8859-1", "CP819"],
  "max_bytes": 1,
  "min_bytes": 1,
  "is_variable_width": false,
  "mappings": [
    {
      "byte_sequence": "0x00",
      "unicode_codepoint": "U+0000",
      "description": "NULL"
    },
    ...
  ]
}
```

### Mapping Tables

Mapping tables live in `resources/charsets/mappings/` and follow the schema in
`charset_mapping.schema.json`. Each file includes byte-to-Unicode mappings for
non-Unicode encodings (e.g., `iso-8859-1.map.json`, `windows-1252.map.json`).

## Loading Character Sets

```bash
# Load built-in character sets (UTF-8, ASCII, ISO-8859-1, UTF-16, UTF-32)
./sb_charset_loader /path/to/database.sb --builtin

# Load additional character sets from this directory
./sb_charset_loader /path/to/database.sb resources/charsets
```

## References

- Unicode Standard: https://www.unicode.org/
- IANA Character Sets: https://www.iana.org/assignments/character-sets/
- ICU (International Components for Unicode): https://icu.unicode.org/
- PostgreSQL Character Sets: https://www.postgresql.org/docs/current/multibyte.html
- MySQL Character Sets: https://dev.mysql.com/doc/refman/8.0/en/charset.html
