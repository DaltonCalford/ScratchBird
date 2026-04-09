# Fulltext Ranking Modes

## Purpose
Define deterministic ranking algorithms for each dialect and native mode.

## Modes
- `sb_tfidf`: ScratchBird native TF-IDF ranking.
- `mysql_nl`: MySQL natural language ranking (InnoDB).
- `mysql_bool`: MySQL boolean mode ranking (InnoDB).
- `pg_ts_rank`: PostgreSQL `ts_rank` algorithm.
- `pg_ts_rank_cd`: PostgreSQL `ts_rank_cd` algorithm.

## ScratchBird Native (`sb_tfidf`)
- `tf = 1 + log(token_count_in_doc)`
- `idf = log(1 + total_docs / (1 + doc_freq))`
- `score = sum(tf * idf)` for query tokens.
- No normalization by default. Normalization is applied only when the query includes normalization flags.

## MySQL Natural Language (`mysql_nl`)
Definitions:
- `total_docs`: total number of indexed documents.
- `doc_count(term)`: number of documents containing the term.
- `freq(term, doc)`: number of occurrences of the term in the document.
- `idf(term)`:
  - If `doc_count(term) == total_docs`, `idf = log10(1.0001)`.
  - Else `idf = log10(total_docs / doc_count(term))`.

Algorithm:
1. Compute `idf(term)` for each query term.
2. For each matching document:
   - `score = sum( freq(term, doc) * idf(term) * idf(term) )`.
3. No additional normalization is applied.

Optimization for single-term queries:
- If the query is a single term with no boolean operators:
  - `score = freq(term, doc) * idf(term) * idf(term)`.
  - This is identical to the general algorithm but computed without building term bitsets.

## MySQL Boolean (`mysql_bool`)
Definitions:
- `base_score` starts at 0.
- `RANK_UPGRADE = +1.0`, `RANK_DOWNGRADE = -1.0`.
- Clamp `base_score` to `[-1.0, +1.0]` after each adjustment.

Algorithm:
1. Evaluate boolean query:
   - `+term`: intersection with current set; apply `RANK_UPGRADE` on match.
   - `-term`: exclude documents that match this term.
   - `term` without prefix: union into result set with no base adjustment.
   - `>term`: union into result set and apply `RANK_UPGRADE`.
   - `<term`: union into result set and apply `RANK_DOWNGRADE`.
   - `~term`: apply `RANK_DOWNGRADE` without removing the document.
   - Phrase/proximity: include only documents satisfying positional constraints.
2. For each resulting document, compute:
   - `score = base_score + sum( freq(term, doc) * idf(term) * idf(term) )`.
3. Apply exclusions (`-term`) after scoring.

Notes:
- This algorithm matches InnoDB fulltext ranking:
  - IDF uses `log10(total_docs / doc_count)` with the `doc_count == total_docs` special case.
  - Ranking sums `freq * idf^2` for each term.

## MySQL Query Expansion
Algorithm:
1. Perform the first pass using the user query.
2. Fetch and tokenize all documents returned in pass 1.
3. Remove terms already present in the original query:
   - If the original query contains wildcard terms, remove any expanded term that has the wildcard as prefix.
4. For each remaining term:
   - Add term to the query term set.
   - Union search results into the result set.
5. Recompute `idf` and ranking using the combined term set.

## PostgreSQL `ts_rank` (`pg_ts_rank`)
Definitions:
- Weights `w = [0.1, 0.2, 0.4, 1.0]` for weights A, B, C, D.
- `wpos(pos)` maps each lexeme position weight to its weight value.
- `word_distance(d) = 1 / (1.005 + 0.05 * exp(d/1.5 - 2))` for `d <= 100`, else `1e-30`.
- `MAXENTRYPOS` is the maximum position value (as in PostgreSQL).

OR ranking:
1. For each query term, collect all positions in the document.
2. For each term:
   - `resj = sum( wpos(pos_j) / (j+1)^2 )` over term occurrences ordered by position.
   - `wjm` is maximum `wpos`, `jm` is its index.
   - `term_score = (wjm + resj - wjm / (jm+1)^2) / 1.64493406685`.
3. `score = average(term_score over all query terms)`.

AND ranking:
1. For every pair of query terms, compute distances between each occurrence.
2. For each distance `d`:
   - `cur = sqrt(wpos(pos_i) * wpos(pos_k) * word_distance(d))`.
   - `score = 1 - (1 - score) * (1 - cur)` (probabilistic OR).

Normalization:
- `RANK_NO_NORM` (default): no normalization.
- `RANK_NORM_LOGLENGTH`: divide by `log2(doc_length + 1)`.
- `RANK_NORM_LENGTH`: divide by `doc_length`.
- `RANK_NORM_UNIQ`: divide by number of unique lexemes.
- `RANK_NORM_LOGUNIQ`: divide by `log2(unique_lexemes + 1)`.
- `RANK_NORM_RDIVRPLUS1`: divide by `score + 1`.

## PostgreSQL `ts_rank_cd` (`pg_ts_rank_cd`)
Cover density ranking uses minimal spans that cover the query terms.

Definitions:
- `DocRepresentation` is a sorted list of positions where query operands occur.
- Each entry contains:
  - `pos` (position with weight)
  - `items[]` list of query operands that match at this position.
- `CoverExt` contains:
  - `begin`, `end` pointers into `DocRepresentation`.
  - `p` and `q` as the minimum and maximum positions in the cover.
  - `pos` the starting index for the next cover search.

Document representation:
1. For each query operand, find all positions in the tsvector.
2. Filter positions by operand weight mask.
3. Create `DocRepresentation` entries `(pos, operand)`.
4. Sort by `pos`.
5. Merge entries with same `(pos, entry)` into a single `DocRepresentation` entry whose `items[]` list contains all operands.

Cover selection:
1. Start at `ext.pos` and scan forward to find the first position where the query is satisfied.
2. Record this as `ext.q` and `ext.end`.
3. Scan backward from `ext.end` to find the earliest position still satisfying the query.
4. Record as `ext.p` and `ext.begin`.
5. Set `ext.pos` to the position immediately after `ext.begin` for the next cover.
6. Repeat until no cover is found.

Score accumulation:
1. For each cover:
   - `InvSum = sum( 1 / weight(pos) )` over positions in the cover.
   - `Cpos = (cover_length) / InvSum`, where `cover_length = ext.end - ext.begin + 1`.
   - `nNoise = (ext.q - ext.p) - (ext.end - ext.begin)`.
   - If `nNoise < 0`, set `nNoise = (ext.end - ext.begin) / 2`.
   - `Wdoc += Cpos / (1 + nNoise)`.
2. Track cover center distances:
   - `CurExtPos = (ext.q + ext.p) / 2`.
   - If previous cover exists, `SumDist += 1 / (CurExtPos - PrevExtPos)`.
   - `PrevExtPos = CurExtPos`.
3. Apply normalization flags as in `ts_rank`.
4. If `RANK_NORM_EXTDIST` is set:
   - `Wdoc /= (NExtent / SumDist)` when `SumDist > 0`.

## Mode Selection
- ScratchBird native:
  - Default `sb_tfidf`.
- `fulltext_ranking_mode` option overrides the default when provided.
- MySQL emulation:
  - `MATCH ... AGAINST` chooses `mysql_nl`, `mysql_bool`, or query expansion.
- PostgreSQL emulation:
  - `ts_rank` and `ts_rank_cd` functions map to `pg_ts_rank` and `pg_ts_rank_cd`.
