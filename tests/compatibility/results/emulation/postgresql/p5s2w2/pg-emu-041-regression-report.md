Last updated: 2026-03-24

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
# using temp instance on port 58928 with PID 1142971
ok 1         - test_setup                                147 ms
# parallel group (20 tests):  char int2 varchar int4 name text money txid float4 int8 pg_lsn oid boolean regproc bit enum float8 uuid numeric rangetypes
ok 2         + boolean                                    27 ms
ok 3         + char                                       11 ms
ok 4         + name                                       17 ms
ok 5         + varchar                                    12 ms
ok 6         + text                                       17 ms
ok 7         + int2                                       11 ms
ok 8         + int4                                       12 ms
ok 9         + int8                                       23 ms
ok 10        + oid                                        26 ms
ok 11        + float4                                     19 ms
ok 12        + float8                                     38 ms
ok 13        + bit                                        36 ms
ok 14        + numeric                                   137 ms
ok 15        + txid                                       18 ms
ok 16        + uuid                                       52 ms
ok 17        + enum                                       36 ms
ok 18        + money                                      18 ms
ok 19        + rangetypes                                166 ms
ok 20        + pg_lsn                                     22 ms
ok 21        + regproc                                    26 ms
# parallel group (20 tests):  md5 lseg line path circle macaddr time point timetz macaddr8 numerology inet date interval strings polygon box multirangetypes timestamp timestamptz
ok 22        + strings                                    55 ms
ok 23        + md5                                         6 ms
ok 24        + numerology                                 20 ms
ok 25        + point                                      17 ms
ok 26        + lseg                                        9 ms
ok 27        + line                                        9 ms
ok 28        + box                                        88 ms
ok 29        + path                                        8 ms
ok 30        + polygon                                    84 ms
ok 31        + circle                                     10 ms
ok 32        + date                                       29 ms
ok 33        + time                                       11 ms
ok 34        + timetz                                     18 ms
ok 35        + timestamp                                 327 ms
ok 36        + timestamptz                               340 ms
ok 37        + interval                                   37 ms
ok 38        + inet                                       27 ms
ok 39        + macaddr                                     9 ms
ok 40        + macaddr8                                   17 ms
ok 41        + multirangetypes                           109 ms
# parallel group (19 tests):  comments euc_kr unicode misc_sanity pg_ndistinct pg_dependencies oid8 tstypes encoding mvcc xid expressions type_sanity horology geometry stats_import database opr_sanity regex
ok 42        + geometry                                   49 ms
ok 43        + horology                                   48 ms
ok 44        + tstypes                                    25 ms
ok 45        + regex                                     197 ms
ok 46        + type_sanity                                48 ms
ok 47        + opr_sanity                                150 ms
ok 48        + misc_sanity                                12 ms
ok 49        + comments                                    8 ms
ok 50        + expressions                                42 ms
ok 51        + unicode                                    10 ms
ok 52        + xid                                        42 ms
ok 53        + mvcc                                       39 ms
ok 54        + database                                  129 ms
ok 55        + stats_import                               97 ms
ok 56        + pg_ndistinct                               12 ms
ok 57        + pg_dependencies                            17 ms
ok 58        + oid8                                       20 ms
ok 59        + encoding                                   35 ms
ok 60        + euc_kr                                      9 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       58 ms
ok 62        + copyselect                                 13 ms
ok 63        + copydml                                    15 ms
ok 64        + copyencoding                               10 ms
ok 65        + insert                                    152 ms
ok 66        + insert_conflict                            62 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_misc create_schema create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                19 ms
ok 69        + create_operator                            10 ms
ok 70        + create_procedure                           29 ms
ok 71        + create_table                              125 ms
ok 72        + create_type                                13 ms
ok 73        + create_schema                              20 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              327 ms
ok 75        + create_index_spgist                       190 ms
ok 76        + create_view                               125 ms
ok 77        + index_including                            96 ms
ok 78        + index_including_gist                      104 ms
# parallel group (16 tests):  create_cast errors hash_func create_aggregate roleattributes select drop_if_exists typed_table create_function_sql create_am infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           19 ms
ok 80        + create_function_sql                        66 ms
ok 81        + create_cast                                12 ms
ok 82        + constraints                               319 ms
ok 83        + triggers                                  466 ms
ok 84        + select                                     28 ms
ok 85        + inherit                                   472 ms
ok 86        + typed_table                                54 ms
ok 87        + vacuum                                    225 ms
ok 88        + drop_if_exists                             32 ms
ok 89        + updatable_views                           327 ms
ok 90        + roleattributes                             20 ms
ok 91        + create_am                                  77 ms
ok 92        + hash_func                                  16 ms
ok 93        + errors                                     15 ms
ok 94        + infinite_recurse                           92 ms
ok 95        - sanity_check                               56 ms
# parallel group (20 tests):  case select_having delete select_distinct_on select_implicit random prepared_xacts namespace select_into portals transactions select_distinct union arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                                67 ms
ok 97        + select_distinct                            92 ms
ok 98        + select_distinct_on                         38 ms
ok 99        + select_implicit                            38 ms
ok 100       + select_having                              25 ms
ok 101       + subselect                                 158 ms
ok 102       + union                                      97 ms
ok 103       + case                                       23 ms
ok 104       + join                                      351 ms
ok 105       + aggregates                                322 ms
ok 106       + transactions                               76 ms
ok 107       + random                                     39 ms
ok 108       + portals                                    68 ms
ok 109       + arrays                                    151 ms
ok 110       + btree_index                               658 ms
ok 111       + hash_index                                301 ms
ok 112       + update                                    213 ms
ok 113       + delete                                     24 ms
ok 114       + namespace                                  53 ms
ok 115       + prepared_xacts                             50 ms
# parallel group (20 tests):  init_privs drop_operator tablesample security_label lock password object_address collate replica_identity groupingsets identity matview spgist rowsecurity generated_stored gin gist brin join_hash privileges
ok 116       + brin                                      854 ms
ok 117       + gin                                       497 ms
ok 118       + gist                                      567 ms
ok 119       + spgist                                    415 ms
ok 120       + privileges                               1051 ms
ok 121       + init_privs                                 17 ms
ok 122       + security_label                             78 ms
ok 123       + collate                                   153 ms
ok 124       + matview                                   316 ms
ok 125       + lock                                       79 ms
ok 126       + replica_identity                          179 ms
ok 127       + rowsecurity                               439 ms
ok 128       + object_address                            120 ms
ok 129       + tablesample                                71 ms
ok 130       + groupingsets                              227 ms
ok 131       + drop_operator                              32 ms
ok 132       + password                                  115 ms
ok 133       + identity                                  307 ms
ok 134       + generated_stored                          453 ms
ok 135       + join_hash                                 863 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 47 ms
ok 137       + brin_multi                                102 ms
# parallel group (20 tests):  async nls dbsize tsrf tidscan collate.utf8 alter_operator sysviews tid create_role misc_functions tidrangescan misc alter_generic incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         237 ms
ok 139       + alter_generic                              75 ms
ok 140       + alter_operator                             37 ms
ok 141       + misc                                       74 ms
ok 142       + async                                      10 ms
ok 143       + dbsize                                     13 ms
ok 144       + merge                                     160 ms
ok 145       + misc_functions                             58 ms
ok 146       + nls                                        12 ms
ok 147       + sysviews                                   40 ms
ok 148       + tsrf                                       33 ms
ok 149       + tid                                        41 ms
ok 150       + tidscan                                    34 ms
ok 151       + tidrangescan                               69 ms
ok 152       + collate.utf8                               34 ms
ok 153       + collate.icu.utf8                          190 ms
ok 154       + incremental_sort                           84 ms
ok 155       + create_role                                49 ms
ok 156       + without_overlaps                          317 ms
ok 157       + generated_virtual                         273 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     179 ms
ok 159       + psql                                      256 ms
ok 160       + psql_crosstab                              24 ms
ok 161       + psql_pipeline                              27 ms
ok 162       + amutils                                    11 ms
ok 163       + stats_ext                                 764 ms
ok 164       + collate.linux.utf8                          6 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           365 ms
ok 167       - write_parallel                             43 ms
ok 168       - vacuum_parallel                            66 ms
ok 169       - maintain_every                             19 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               231 ms
ok 171       + subscription                               35 ms
# parallel group (18 tests):  portals_p2 advisory_lock combocid tsdicts xmlmap functional_deps guc dependency equivclass select_views stats_rewrite window bitmapops tsearch cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               76 ms
ok 173       + portals_p2                                 16 ms
ok 174       + foreign_key                               568 ms
ok 175       + cluster                                   175 ms
ok 176       + dependency                                 57 ms
ok 177       + guc                                        46 ms
ok 178       + bitmapops                                 160 ms
ok 179       + combocid                                   22 ms
ok 180       + tsearch                                   167 ms
ok 181       + tsdicts                                    36 ms
ok 182       + foreign_data                              316 ms
ok 183       + window                                    147 ms
ok 184       + xmlmap                                     39 ms
ok 185       + functional_deps                            41 ms
ok 186       + advisory_lock                              19 ms
ok 187       + indirect_toast                            205 ms
ok 188       + equivclass                                 55 ms
ok 189       + stats_rewrite                              78 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable sqljson_queryfuncs json jsonb_jsonpath jsonb
ok 190       + json                                       59 ms
ok 191       + jsonb                                     143 ms
ok 192       + json_encoding                               8 ms
ok 193       + jsonpath                                   18 ms
ok 194       + jsonpath_encoding                           7 ms
ok 195       + jsonb_jsonpath                             59 ms
ok 196       + sqljson                                    35 ms
ok 197       + sqljson_queryfuncs                         47 ms
ok 198       + sqljson_jsontable                          36 ms
# parallel group (18 tests):  prepare limit xml plancache conversion polymorphism returning copy2 rowtypes largeobject sequence rangefuncs with truncate temp domain plpgsql alter_table
ok 199       + plancache                                  91 ms
ok 200       + limit                                      71 ms
ok 201       + plpgsql                                   342 ms
ok 202       + copy2                                     146 ms
ok 203       + temp                                      211 ms
ok 204       + domain                                    211 ms
ok 205       + rangefuncs                                171 ms
ok 206       + prepare                                    19 ms
ok 207       + conversion                                 95 ms
ok 208       + truncate                                  186 ms
ok 209       + alter_table                               781 ms
ok 210       + sequence                                  169 ms
ok 211       + polymorphism                              124 ms
ok 212       + rowtypes                                  146 ms
ok 213       + returning                                 129 ms
ok 214       + largeobject                               167 ms
ok 215       + with                                      174 ms
ok 216       + xml                                        68 ms
# parallel group (18 tests):  compression_lz4 numa hash_part predicate reloptions partition_info explain compression memoize eager_aggregate partition_split partition_merge partition_aggregate partition_join partition_prune tuplesort indexing stats
ok 217       + partition_merge                           361 ms
ok 218       + partition_split                           335 ms
ok 219       + partition_join                            482 ms
ok 220       + partition_prune                           550 ms
ok 221       + reloptions                                 53 ms
ok 222       + hash_part                                  25 ms
ok 223       + indexing                                  586 ms
ok 224       + partition_aggregate                       437 ms
ok 225       + partition_info                             63 ms
ok 226       + tuplesort                                 560 ms
ok 227       + explain                                    72 ms
ok 228       + compression                               103 ms
ok 229       + compression_lz4                            12 ms
ok 230       + memoize                                   107 ms
ok 231       + stats                                     610 ms
ok 232       + predicate                                  47 ms
ok 233       + numa                                       12 ms
ok 234       + eager_aggregate                           180 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                  111 ms
ok 236       + event_trigger                             126 ms
ok 237       - event_trigger_login                        21 ms
ok 238       - fast_default                               96 ms
ok 239       - tablespace                                226 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
