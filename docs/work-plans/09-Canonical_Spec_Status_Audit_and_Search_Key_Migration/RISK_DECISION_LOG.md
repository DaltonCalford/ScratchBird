# Risk Decision Log

| Risk ID | Risk | Current Handling |
| --- | --- | --- |
| SV-09-R01 | the authoritative inventory may change while the package is active | freeze a scope snapshot at `SV-09-002` and record later additions explicitly |
| SV-09-R02 | some canonical specs may point to third-party or generated files that cannot safely receive new audit anchors | use owned wrapper or integration files and record the limitation in notes |
| SV-09-R03 | status verification may uncover broad overclaim drift across many sections | preserve explicit discrepancy records and downgrade status claims rather than hiding the drift |
| SV-09-R04 | sibling repositories referenced by canon may be dirty or evolve independently | audit against current truth and record repo-local limits without reverting unrelated work |
| SV-09-R05 | the final finished/partial/outstanding rollups may drift if they are edited manually | generate or maintain them strictly from the current audit matrix and classification ledger |
