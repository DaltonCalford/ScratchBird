### Lexical Structure and Literals

Tokens and kinds are defined in `include/scratchbird/engine/lexer.h` and implemented in `src/engine/lexer.cpp`.

- Token kinds: Identifier, QuotedIdentifier, Integer, Decimal, String, Date, Time, Timestamp, Uuid, Symbol, Keyword, End
- Whitespace and comments: spaces/tabs/newlines are skipped; line comments start with `--`; block comments `/* ... */` are skipped.
- String literals:
  - `'text'` with doubled quotes for escaping: `''`
  - Dollar-quoted strings: `$tag$ ... $tag$` (no nested openers). Tag may be empty (`$$...$$`) or alnum/underscore.
  - Charset-prefixed: `N'...'` and `_CHARSET '...'` set `Token.charset` and emit a warning for unknown tags.
- Date/time/timestamp/uuid:
  - If immediately preceded by the keyword `DATE`, `TIME`, `TIMESTAMP`, or `UUID`, single-quoted text is recognized and token kind is updated accordingly.
  - TIME accepts optional seconds, fractional seconds, and timezone `Z`, `+/-HH[:MM|HHMM]`.
- Multi-character symbols: `!=`, `<>`, `<=`, `>=`, `::`, `||` are emitted as a single `Symbol` token.

Warnings: the lexer accumulates warnings (thread-local) for unknown charset tags and other recoveries.

Code anchors: `src/engine/lexer.cpp` (lex(), lex_string(), lex_dollar_string(), lex_symbol())

