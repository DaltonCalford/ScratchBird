Last updated: 2026-03-26

# PG-EMU-041 Regression Smoke Report

- Mode: `execute`
- Overall result: `pass`
- Command timeout: `7200s`

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
# using temp instance on port 58928 with PID 2165071
ok 1         - test_setup                                104 ms
# parallel group (20 tests):  oid char int2 int4 varchar name int8 pg_lsn float4 txid text money regproc boolean float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    23 ms
ok 3         + char                                       11 ms
ok 4         + name                                       15 ms
ok 5         + varchar                                    14 ms
ok 6         + text                                       17 ms
ok 7         + int2                                       10 ms
ok 8         + int4                                       12 ms
ok 9         + int8                                       15 ms
ok 10        + oid                                         9 ms
ok 11        + float4                                     16 ms
ok 12        + float8                                     22 ms
ok 13        + bit                                        27 ms
ok 14        + numeric                                   116 ms
ok 15        + txid                                       16 ms
ok 16        + uuid                                       46 ms
ok 17        + enum                                       32 ms
ok 18        + money                                      17 ms
ok 19        + rangetypes                                133 ms
ok 20        + pg_lsn                                     13 ms
ok 21        + regproc                                    20 ms
# parallel group (20 tests):  md5 path lseg line circle point macaddr time timetz numerology macaddr8 date inet interval strings box polygon multirangetypes timestamp timestamptz
ok 22        + strings                                    60 ms
ok 23        + md5                                         8 ms
ok 24        + numerology                                 20 ms
ok 25        + point                                      13 ms
ok 26        + lseg                                        9 ms
ok 27        + line                                       11 ms
ok 28        + box                                        82 ms
ok 29        + path                                        7 ms
ok 30        + polygon                                    92 ms
ok 31        + circle                                     12 ms
ok 32        + date                                       22 ms
ok 33        + time                                       16 ms
ok 34        + timetz                                     17 ms
ok 35        + timestamp                                 334 ms
ok 36        + timestamptz                               347 ms
ok 37        + interval                                   27 ms
ok 38        + inet                                       23 ms
ok 39        + macaddr                                    15 ms
ok 40        + macaddr8                                   18 ms
ok 41        + multirangetypes                           101 ms
# parallel group (19 tests):  comments unicode euc_kr pg_dependencies misc_sanity oid8 pg_ndistinct tstypes expressions xid encoding mvcc horology geometry type_sanity stats_import database opr_sanity regex
ok 42        + geometry                                   42 ms
ok 43        + horology                                   38 ms
ok 44        + tstypes                                    26 ms
ok 45        + regex                                     189 ms
ok 46        + type_sanity                                46 ms
ok 47        + opr_sanity                                135 ms
ok 48        + misc_sanity                                12 ms
ok 49        + comments                                    8 ms
ok 50        + expressions                                26 ms
ok 51        + unicode                                     9 ms
ok 52        + xid                                        26 ms
ok 53        + mvcc                                       35 ms
ok 54        + database                                  126 ms
ok 55        + stats_import                               93 ms
ok 56        + pg_ndistinct                               18 ms
ok 57        + pg_dependencies                            10 ms
ok 58        + oid8                                       12 ms
ok 59        + encoding                                   27 ms
ok 60        + euc_kr                                      9 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       52 ms
ok 62        + copyselect                                 10 ms
ok 63        + copydml                                    14 ms
ok 64        + copyencoding                                8 ms
ok 65        + insert                                    106 ms
ok 66        + insert_conflict                            63 ms
# parallel group (7 tests):  create_function_c create_operator create_schema create_type create_misc create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                19 ms
ok 69        + create_operator                            10 ms
ok 70        + create_procedure                           27 ms
ok 71        + create_table                              120 ms
ok 72        + create_type                                15 ms
ok 73        + create_schema                              15 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              282 ms
ok 75        + create_index_spgist                       159 ms
ok 76        + create_view                               129 ms
ok 77        + index_including                            60 ms
ok 78        + index_including_gist                      112 ms
# parallel group (16 tests):  create_cast errors hash_func roleattributes create_aggregate drop_if_exists select typed_table create_function_sql create_am infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           19 ms
ok 80        + create_function_sql                        57 ms
ok 81        + create_cast                                10 ms
ok 82        + constraints                               240 ms
ok 83        + triggers                                  353 ms
ok 84        + select                                     28 ms
ok 85        + inherit                                   373 ms
ok 86        + typed_table                                50 ms
ok 87        + vacuum                                    141 ms
ok 88        + drop_if_exists                             22 ms
ok 89        + updatable_views                           251 ms
ok 90        + roleattributes                             18 ms
ok 91        + create_am                                  60 ms
ok 92        + hash_func                                  16 ms
ok 93        + errors                                     15 ms
ok 94        + infinite_recurse                          110 ms
ok 95        - sanity_check                               43 ms
# parallel group (20 tests):  select_having delete select_distinct_on select_implicit random case select_into prepared_xacts namespace portals select_distinct union transactions arrays subselect update aggregates hash_index join btree_index
ok 96        + select_into                                44 ms
ok 97        + select_distinct                            70 ms
ok 98        + select_distinct_on                         23 ms
ok 99        + select_implicit                            28 ms
ok 100       + select_having                              13 ms
ok 101       + subselect                                 141 ms
ok 102       + union                                      73 ms
ok 103       + case                                       30 ms
ok 104       + join                                      305 ms
ok 105       + aggregates                                240 ms
ok 106       + transactions                               75 ms
ok 107       + random                                     29 ms
ok 108       + portals                                    61 ms
ok 109       + arrays                                    116 ms
ok 110       + btree_index                               480 ms
ok 111       + hash_index                                263 ms
ok 112       + update                                    170 ms
ok 113       + delete                                     20 ms
ok 114       + namespace                                  43 ms
ok 115       + prepared_xacts                             41 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets matview identity rowsecurity generated_stored spgist gin gist join_hash brin privileges
ok 116       + brin                                      598 ms
ok 117       + gin                                       379 ms
ok 118       + gist                                      396 ms
ok 119       + spgist                                    318 ms
ok 120       + privileges                                741 ms
ok 121       + init_privs                                  8 ms
ok 122       + security_label                             23 ms
ok 123       + collate                                    91 ms
ok 124       + matview                                   222 ms
ok 125       + lock                                       68 ms
ok 126       + replica_identity                          130 ms
ok 127       + rowsecurity                               305 ms
ok 128       + object_address                             80 ms
ok 129       + tablesample                                39 ms
ok 130       + groupingsets                              165 ms
ok 131       + drop_operator                              13 ms
ok 132       + password                                   89 ms
ok 133       + identity                                  238 ms
ok 134       + generated_stored                          312 ms
ok 135       + join_hash                                 587 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 37 ms
ok 137       + brin_multi                                 94 ms
# parallel group (20 tests):  dbsize nls async alter_operator tid tidscan collate.utf8 tsrf sysviews create_role misc_functions alter_generic misc tidrangescan incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         192 ms
ok 139       + alter_generic                              55 ms
ok 140       + alter_operator                             16 ms
ok 141       + misc                                       59 ms
ok 142       + async                                       8 ms
ok 143       + dbsize                                      7 ms
ok 144       + merge                                     147 ms
ok 145       + misc_functions                             49 ms
ok 146       + nls                                         7 ms
ok 147       + sysviews                                   39 ms
ok 148       + tsrf                                       29 ms
ok 149       + tid                                        24 ms
ok 150       + tidscan                                    24 ms
ok 151       + tidrangescan                               62 ms
ok 152       + collate.utf8                               26 ms
ok 153       + collate.icu.utf8                          165 ms
ok 154       + incremental_sort                           69 ms
ok 155       + create_role                                47 ms
ok 156       + without_overlaps                          229 ms
ok 157       + generated_virtual                         204 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     105 ms
ok 159       + psql                                      247 ms
ok 160       + psql_crosstab                              12 ms
ok 161       + psql_pipeline                              14 ms
ok 162       + amutils                                     9 ms
ok 163       + stats_ext                                 672 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     4 ms
ok 166       - select_parallel                           341 ms
ok 167       - write_parallel                             38 ms
ok 168       - vacuum_parallel                            42 ms
ok 169       - maintain_every                             15 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               156 ms
ok 171       + subscription                               37 ms
# parallel group (18 tests):  advisory_lock portals_p2 combocid xmlmap tsdicts functional_deps equivclass guc dependency select_views stats_rewrite bitmapops window tsearch cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               64 ms
ok 173       + portals_p2                                 17 ms
ok 174       + foreign_key                               441 ms
ok 175       + cluster                                   151 ms
ok 176       + dependency                                 53 ms
ok 177       + guc                                        46 ms
ok 178       + bitmapops                                 118 ms
ok 179       + combocid                                   18 ms
ok 180       + tsearch                                   136 ms
ok 181       + tsdicts                                    28 ms
ok 182       + foreign_data                              279 ms
ok 183       + window                                    118 ms
ok 184       + xmlmap                                     27 ms
ok 185       + functional_deps                            37 ms
ok 186       + advisory_lock                              15 ms
ok 187       + indirect_toast                            178 ms
ok 188       + equivclass                                 37 ms
ok 189       + stats_rewrite                              67 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable jsonb_jsonpath json sqljson_queryfuncs jsonb
ok 190       + json                                       41 ms
ok 191       + jsonb                                     100 ms
ok 192       + json_encoding                               7 ms
ok 193       + jsonpath                                   15 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             37 ms
ok 196       + sqljson                                    28 ms
ok 197       + sqljson_queryfuncs                         41 ms
ok 198       + sqljson_jsontable                          31 ms
# parallel group (18 tests):  prepare limit xml conversion plancache returning polymorphism rowtypes copy2 sequence temp largeobject truncate with rangefuncs domain plpgsql alter_table
ok 199       + plancache                                  73 ms
ok 200       + limit                                      43 ms
ok 201       + plpgsql                                   221 ms
ok 202       + copy2                                     113 ms
ok 203       + temp                                      129 ms
ok 204       + domain                                    167 ms
ok 205       + rangefuncs                                135 ms
ok 206       + prepare                                    21 ms
ok 207       + conversion                                 69 ms
ok 208       + truncate                                  131 ms
ok 209       + alter_table                               577 ms
ok 210       + sequence                                  117 ms
ok 211       + polymorphism                               93 ms
ok 212       + rowtypes                                  101 ms
ok 213       + returning                                  88 ms
ok 214       + largeobject                               128 ms
ok 215       + with                                      134 ms
ok 216       + xml                                        55 ms
# parallel group (18 tests):  compression_lz4 numa hash_part reloptions predicate partition_info explain compression memoize eager_aggregate partition_merge partition_split partition_prune partition_aggregate partition_join tuplesort indexing stats
ok 217       + partition_merge                           244 ms
ok 218       + partition_split                           280 ms
ok 219       + partition_join                            368 ms
ok 220       + partition_prune                           331 ms
ok 221       + reloptions                                 38 ms
ok 222       + hash_part                                  25 ms
ok 223       + indexing                                  446 ms
ok 224       + partition_aggregate                       333 ms
ok 225       + partition_info                             45 ms
ok 226       + tuplesort                                 427 ms
ok 227       + explain                                    50 ms
ok 228       + compression                                76 ms
ok 229       + compression_lz4                             7 ms
ok 230       + memoize                                    89 ms
ok 231       + stats                                     491 ms
ok 232       + predicate                                  39 ms
ok 233       + numa                                        7 ms
ok 234       + eager_aggregate                           136 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   79 ms
ok 236       + event_trigger                              88 ms
ok 237       - event_trigger_login                        20 ms
ok 238       - fast_default                               52 ms
ok 239       - tablespace                                151 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
