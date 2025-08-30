### Lexical Structure and Literals

What it is
- The tokenizer rules: how input splits into tokens (identifiers, numbers, strings, symbols, keywords).

Why it matters
- Understanding tokens avoids surprises in parsing (e.g., how dollar-quoted strings or charset-prefixed strings behave).
- Diagnostics (lexer warnings) help catch issues early.

How to use it
- Use the examples below to form valid literals and symbols. When in doubt, check warnings via `lexer_warnings()` in code.

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

See also
- [Operators](./sql-operators.md) · [Data types](./sql-data-types.md) · [Reserved words](./sql-reserved-words.md)

