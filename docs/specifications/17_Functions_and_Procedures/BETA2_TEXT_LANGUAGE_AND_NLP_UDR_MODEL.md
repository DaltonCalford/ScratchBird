# Beta 2 Text Language And NLP UDR Model

## Purpose

This document defines the text/language UDR family for tokenization,
normalization, linguistic annotation, entity extraction, and language-aware
analytical preprocessing.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `spaCy`.

## Owning package

- `sb_pkg_text_udr`

## Dependencies

This package depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_expr_udr`
- `sb_pkg_ml_udr`

## Mandatory surfaces

The package shall provide:

- text normalization
- tokenization and sentence segmentation
- stopword filtering
- stemming and lemmatization for the admitted language set
- part-of-speech tagging for the admitted language set
- named-entity extraction for the admitted entity set
- keyword extraction
- embedding/prevectorization preparation hooks for the admitted subset

## Required routine families

- `sb_text.normalize(...)`
- `sb_text.tokenize(...)`
- `sb_text.sentences(...)`
- `sb_text.lemmatize(...)`
- `sb_text.pos_tags(...)`
- `sb_text.entities(...)`
- `sb_text.keywords(...)`

## Example contract

```sql
select *
from sb_text.entities('Alice wired 2500 USD to Acme Corp on 2026-04-03');
```

## Operational rules

1. Language model families admitted in Beta 2 must be explicitly packaged and
   versioned; no runtime model downloads are allowed.
2. Text routines shall expose language id, model id, and normalization policy.
3. Unsupported-language requests shall fail closed unless a documented fallback
   is selected.

## Explicit exclusions

- generative LLM chat or completion surfaces
- live external inference APIs
- arbitrary model download/install inside the engine
