Last updated: 2026-02-26

# PG-EMU-041 Regression Smoke Report

- Mode: `execute`
- Overall result: `pass`
- Command timeout: `1200s`

## Command Results

### Command 1
- `cwd`: `/home/dcalford/CliWork/ScratchBird/tests/compatibility/postgresql/repos/postgres/src/test/regress`
- `cmd`: `bash -lc 'test -f GNUmakefile && test -d sql && echo pg_regress_snapshot_present'`
- `exit_code`: `0`
- `timed_out`: `false`

```text
pg_regress_snapshot_present
```

### Command 2
- `cwd`: `<outside-tree-path>`
- `cmd`: `make -C src/test/regress check`
- `exit_code`: `0`
- `timed_out`: `false`

```text
ke -j1  checkprep >>'<outside-tree-path>'/tmp_install/log/install.log 2>&1
PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  initdb --auth trust --no-sync --no-instructions --lc-messages=C --no-clean '<outside-tree-path>'/tmp_install/initdb-template >>'<outside-tree-path>'/tmp_install/log/initdb-template.log 2>&1
echo "# +++ regress check in src/test/regress +++" && PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  ../../../src/test/regress/pg_regress --temp-instance=./tmp_check --inputdir=<outside-tree-path> --bindir=     --dlpath=. --max-concurrent-tests=20  --schedule=<outside-tree-path>  
# +++ regress check in src/test/regress +++
# initializing database system by copying initdb template
# using temp instance on port 58928 with PID 1384420
ok 1         - test_setup                                102 ms
# parallel group (20 tests):  char int2 oid name text pg_lsn varchar float4 int4 money int8 txid boolean regproc float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    21 ms
ok 3         + char                                       11 ms
ok 4         + name                                       12 ms
ok 5         + varchar                                    15 ms
ok 6         + text                                       12 ms
ok 7         + int2                                       10 ms
ok 8         + int4                                       17 ms
ok 9         + int8                                       17 ms
ok 10        + oid                                        10 ms
ok 11        + float4                                     15 ms
ok 12        + float8                                     23 ms
ok 13        + bit                                        29 ms
ok 14        + numeric                                   117 ms
ok 15        + txid                                       19 ms
ok 16        + uuid                                       45 ms
ok 17        + enum                                       29 ms
ok 18        + money                                      16 ms
ok 19        + rangetypes                                131 ms
ok 20        + pg_lsn                                     12 ms
ok 21        + regproc                                    19 ms
# parallel group (20 tests):  md5 path lseg circle line point timetz numerology time macaddr8 macaddr inet date interval strings multirangetypes polygon box timestamp timestamptz
ok 22        + strings                                    57 ms
ok 23        + md5                                         7 ms
ok 24        + numerology                                 16 ms
ok 25        + point                                      14 ms
ok 26        + lseg                                        9 ms
ok 27        + line                                       12 ms
ok 28        + box                                        91 ms
ok 29        + path                                        8 ms
ok 30        + polygon                                    83 ms
ok 31        + circle                                     10 ms
ok 32        + date                                       24 ms
ok 33        + time                                       15 ms
ok 34        + timetz                                     14 ms
ok 35        + timestamp                                 315 ms
ok 36        + timestamptz                               338 ms
ok 37        + interval                                   31 ms
ok 38        + inet                                       21 ms
ok 39        + macaddr                                    16 ms
ok 40        + macaddr8                                   14 ms
ok 41        + multirangetypes                            72 ms
# parallel group (19 tests):  unicode comments misc_sanity pg_ndistinct euc_kr pg_dependencies tstypes oid8 expressions xid encoding horology geometry mvcc type_sanity stats_import database opr_sanity regex
ok 42        + geometry                                   35 ms
ok 43        + horology                                   34 ms
ok 44        + tstypes                                    23 ms
ok 45        + regex                                     174 ms
ok 46        + type_sanity                                47 ms
ok 47        + opr_sanity                                145 ms
ok 48        + misc_sanity                                10 ms
ok 49        + comments                                    9 ms
ok 50        + expressions                                27 ms
ok 51        + unicode                                     8 ms
ok 52        + xid                                        28 ms
ok 53        + mvcc                                       33 ms
ok 54        + database                                  112 ms
ok 55        + stats_import                               88 ms
ok 56        + pg_ndistinct                                9 ms
ok 57        + pg_dependencies                            12 ms
ok 58        + oid8                                       23 ms
ok 59        + encoding                                   28 ms
ok 60        + euc_kr                                     10 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       36 ms
ok 62        + copyselect                                 13 ms
ok 63        + copydml                                    15 ms
ok 64        + copyencoding                                9 ms
ok 65        + insert                                    117 ms
ok 66        + insert_conflict                            51 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_misc create_schema create_procedure create_table
ok 67        + create_function_c                           6 ms
ok 68        + create_misc                                20 ms
ok 69        + create_operator                             8 ms
ok 70        + create_procedure                           26 ms
ok 71        + create_table                              123 ms
ok 72        + create_type                                14 ms
ok 73        + create_schema                              19 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              276 ms
ok 75        + create_index_spgist                       152 ms
ok 76        + create_view                               136 ms
ok 77        + index_including                            73 ms
ok 78        + index_including_gist                       85 ms
# parallel group (16 tests):  create_cast errors hash_func roleattributes create_aggregate select drop_if_exists typed_table create_am create_function_sql infinite_recurse vacuum updatable_views constraints triggers inherit
ok 79        + create_aggregate                           23 ms
ok 80        + create_function_sql                        60 ms
ok 81        + create_cast                                13 ms
ok 82        + constraints                               251 ms
ok 83        + triggers                                  301 ms
ok 84        + select                                     26 ms
ok 85        + inherit                                   341 ms
ok 86        + typed_table                                43 ms
ok 87        + vacuum                                    143 ms
ok 88        + drop_if_exists                             27 ms
ok 89        + updatable_views                           245 ms
ok 90        + roleattributes                             19 ms
ok 91        + create_am                                  58 ms
ok 92        + hash_func                                  14 ms
ok 93        + errors                                     14 ms
ok 94        + infinite_recurse                           95 ms
ok 95        - sanity_check                               39 ms
# parallel group (20 tests):  select_having case delete select_distinct_on select_implicit random namespace select_into prepared_xacts select_distinct union portals transactions arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                                39 ms
ok 97        + select_distinct                            67 ms
ok 98        + select_distinct_on                         22 ms
ok 99        + select_implicit                            24 ms
ok 100       + select_having                              16 ms
ok 101       + subselect                                 126 ms
ok 102       + union                                      70 ms
ok 103       + case                                       19 ms
ok 104       + join                                      293 ms
ok 105       + aggregates                                242 ms
ok 106       + transactions                               88 ms
ok 107       + random                                     28 ms
ok 108       + portals                                    78 ms
ok 109       + arrays                                    111 ms
ok 110       + btree_index                               481 ms
ok 111       + hash_index                                233 ms
ok 112       + update                                    160 ms
ok 113       + delete                                     20 ms
ok 114       + namespace                                  37 ms
ok 115       + prepared_xacts                             41 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address collate password replica_identity groupingsets matview identity spgist rowsecurity generated_stored gist gin join_hash brin privileges
ok 116       + brin                                      554 ms
ok 117       + gin                                       317 ms
ok 118       + gist                                      316 ms
ok 119       + spgist                                    274 ms
ok 120       + privileges                                686 ms
ok 121       + init_privs                                  8 ms
ok 122       + security_label                             28 ms
ok 123       + collate                                    80 ms
ok 124       + matview                                   202 ms
ok 125       + lock                                       46 ms
ok 126       + replica_identity                          109 ms
ok 127       + rowsecurity                               279 ms
ok 128       + object_address                             66 ms
ok 129       + tablesample                                42 ms
ok 130       + groupingsets                              161 ms
ok 131       + drop_operator                              20 ms
ok 132       + password                                   80 ms
ok 133       + identity                                  205 ms
ok 134       + generated_stored                          305 ms
ok 135       + join_hash                                 541 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 46 ms
ok 137       + brin_multi                                 85 ms
# parallel group (20 tests):  nls async dbsize tidscan collate.utf8 tsrf alter_operator tid sysviews create_role alter_generic misc_functions tidrangescan incremental_sort misc merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         169 ms
ok 139       + alter_generic                              54 ms
ok 140       + alter_operator                             29 ms
ok 141       + misc                                       78 ms
ok 142       + async                                      10 ms
ok 143       + dbsize                                     10 ms
ok 144       + merge                                     136 ms
ok 145       + misc_functions                             53 ms
ok 146       + nls                                         7 ms
ok 147       + sysviews                                   39 ms
ok 148       + tsrf                                       27 ms
ok 149       + tid                                        35 ms
ok 150       + tidscan                                    23 ms
ok 151       + tidrangescan                               56 ms
ok 152       + collate.utf8                               23 ms
ok 153       + collate.icu.utf8                          160 ms
ok 154       + incremental_sort                           74 ms
ok 155       + create_role                                46 ms
ok 156       + without_overlaps                          268 ms
ok 157       + generated_virtual                         223 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     102 ms
ok 159       + psql                                      214 ms
ok 160       + psql_crosstab                              11 ms
ok 161       + psql_pipeline                              13 ms
ok 162       + amutils                                     9 ms
ok 163       + stats_ext                                 629 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     4 ms
ok 166       - select_parallel                           336 ms
ok 167       - write_parallel                             37 ms
ok 168       - vacuum_parallel                            47 ms
ok 169       - maintain_every                             12 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               146 ms
ok 171       + subscription                               37 ms
# parallel group (18 tests):  portals_p2 advisory_lock combocid xmlmap tsdicts equivclass guc functional_deps dependency select_views stats_rewrite window tsearch bitmapops cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               66 ms
ok 173       + portals_p2                                 12 ms
ok 174       + foreign_key                               465 ms
ok 175       + cluster                                   139 ms
ok 176       + dependency                                 50 ms
ok 177       + guc                                        36 ms
ok 178       + bitmapops                                 137 ms
ok 179       + combocid                                   21 ms
ok 180       + tsearch                                   135 ms
ok 181       + tsdicts                                    31 ms
ok 182       + foreign_data                              218 ms
ok 183       + window                                    113 ms
ok 184       + xmlmap                                     24 ms
ok 185       + functional_deps                            43 ms
ok 186       + advisory_lock                              13 ms
ok 187       + indirect_toast                            180 ms
ok 188       + equivclass                                 31 ms
ok 189       + stats_rewrite                              67 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_queryfuncs sqljson_jsontable json jsonb_jsonpath jsonb
ok 190       + json                                       41 ms
ok 191       + jsonb                                     107 ms
ok 192       + json_encoding                               8 ms
ok 193       + jsonpath                                   17 ms
ok 194       + jsonpath_encoding                           7 ms
ok 195       + jsonb_jsonpath                             49 ms
ok 196       + sqljson                                    29 ms
ok 197       + sqljson_queryfuncs                         32 ms
ok 198       + sqljson_jsontable                          33 ms
# parallel group (18 tests):  prepare xml limit plancache conversion returning sequence rowtypes polymorphism copy2 with rangefuncs truncate temp largeobject domain plpgsql alter_table
ok 199       + plancache                                  60 ms
ok 200       + limit                                      46 ms
ok 201       + plpgsql                                   214 ms
ok 202       + copy2                                     105 ms
ok 203       + temp                                      129 ms
ok 204       + domain                                    143 ms
ok 205       + rangefuncs                                119 ms
ok 206       + prepare                                    15 ms
ok 207       + conversion                                 66 ms
ok 208       + truncate                                  121 ms
ok 209       + alter_table                               540 ms
ok 210       + sequence                                   93 ms
ok 211       + polymorphism                               97 ms
ok 212       + rowtypes                                   94 ms
ok 213       + returning                                  81 ms
ok 214       + largeobject                               133 ms
ok 215       + with                                      111 ms
ok 216       + xml                                        37 ms
# parallel group (18 tests):  numa compression_lz4 hash_part predicate reloptions partition_info explain compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_join partition_prune indexing tuplesort stats
ok 217       + partition_merge                           235 ms
ok 218       + partition_split                           270 ms
ok 219       + partition_join                            308 ms
ok 220       + partition_prune                           381 ms
ok 221       + reloptions                                 42 ms
ok 222       + hash_part                                  17 ms
ok 223       + indexing                                  400 ms
ok 224       + partition_aggregate                       306 ms
ok 225       + partition_info                             47 ms
ok 226       + tuplesort                                 406 ms
ok 227       + explain                                    48 ms
ok 228       + compression                                81 ms
ok 229       + compression_lz4                             7 ms
ok 230       + memoize                                    96 ms
ok 231       + stats                                     484 ms
ok 232       + predicate                                  30 ms
ok 233       + numa                                        7 ms
ok 234       + eager_aggregate                           137 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   83 ms
ok 236       + event_trigger                              96 ms
ok 237       - event_trigger_login                        18 ms
ok 238       - fast_default                               52 ms
ok 239       - tablespace                                150 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
