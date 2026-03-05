Last updated: 2026-03-05

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
ake -j1  checkprep >>'<outside-tree-path>'/tmp_install/log/install.log 2>&1
PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  initdb --auth trust --no-sync --no-instructions --lc-messages=C --no-clean '<outside-tree-path>'/tmp_install/initdb-template >>'<outside-tree-path>'/tmp_install/log/initdb-template.log 2>&1
echo "# +++ regress check in src/test/regress +++" && PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  ../../../src/test/regress/pg_regress --temp-instance=./tmp_check --inputdir=<outside-tree-path> --bindir=     --dlpath=. --max-concurrent-tests=20  --schedule=<outside-tree-path>  
# +++ regress check in src/test/regress +++
# initializing database system by copying initdb template
# using temp instance on port 58928 with PID 508540
ok 1         - test_setup                                115 ms
# parallel group (20 tests):  char oid txid varchar text float4 money int2 name int4 pg_lsn regproc int8 boolean float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    22 ms
ok 3         + char                                       12 ms
ok 4         + name                                       17 ms
ok 5         + varchar                                    15 ms
ok 6         + text                                       15 ms
ok 7         + int2                                       16 ms
ok 8         + int4                                       18 ms
ok 9         + int8                                       20 ms
ok 10        + oid                                        12 ms
ok 11        + float4                                     15 ms
ok 12        + float8                                     28 ms
ok 13        + bit                                        29 ms
ok 14        + numeric                                    87 ms
ok 15        + txid                                       12 ms
ok 16        + uuid                                       51 ms
ok 17        + enum                                       32 ms
ok 18        + money                                      14 ms
ok 19        + rangetypes                                140 ms
ok 20        + pg_lsn                                     16 ms
ok 21        + regproc                                    17 ms
# parallel group (20 tests):  md5 line path circle lseg time point macaddr macaddr8 timetz numerology inet date interval strings multirangetypes polygon box timestamp timestamptz
ok 22        + strings                                    50 ms
ok 23        + md5                                         8 ms
ok 24        + numerology                                 21 ms
ok 25        + point                                      17 ms
ok 26        + lseg                                       13 ms
ok 27        + line                                       11 ms
ok 28        + box                                        91 ms
ok 29        + path                                       11 ms
ok 30        + polygon                                    88 ms
ok 31        + circle                                     11 ms
ok 32        + date                                       24 ms
ok 33        + time                                       13 ms
ok 34        + timetz                                     17 ms
ok 35        + timestamp                                 318 ms
ok 36        + timestamptz                               332 ms
ok 37        + interval                                   34 ms
ok 38        + inet                                       23 ms
ok 39        + macaddr                                    14 ms
ok 40        + macaddr8                                   15 ms
ok 41        + multirangetypes                            80 ms
# parallel group (19 tests):  euc_kr comments unicode misc_sanity pg_ndistinct xid pg_dependencies tstypes oid8 expressions encoding mvcc horology type_sanity geometry stats_import database opr_sanity regex
ok 42        + geometry                                   50 ms
ok 43        + horology                                   41 ms
ok 44        + tstypes                                    24 ms
ok 45        + regex                                     190 ms
ok 46        + type_sanity                                49 ms
ok 47        + opr_sanity                                151 ms
ok 48        + misc_sanity                                14 ms
ok 49        + comments                                    8 ms
ok 50        + expressions                                27 ms
ok 51        + unicode                                     8 ms
ok 52        + xid                                        15 ms
ok 53        + mvcc                                       40 ms
ok 54        + database                                  120 ms
ok 55        + stats_import                               81 ms
ok 56        + pg_ndistinct                               14 ms
ok 57        + pg_dependencies                            15 ms
ok 58        + oid8                                       25 ms
ok 59        + encoding                                   32 ms
ok 60        + euc_kr                                      6 ms
# parallel group (6 tests):  copyencoding copydml copyselect copy insert_conflict insert
ok 61        + copy                                       44 ms
ok 62        + copyselect                                 13 ms
ok 63        + copydml                                    13 ms
ok 64        + copyencoding                                9 ms
ok 65        + insert                                    130 ms
ok 66        + insert_conflict                            61 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_schema create_misc create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                20 ms
ok 69        + create_operator                            10 ms
ok 70        + create_procedure                           20 ms
ok 71        + create_table                              135 ms
ok 72        + create_type                                16 ms
ok 73        + create_schema                              18 ms
# parallel group (5 tests):  index_including_gist index_including create_view create_index_spgist create_index
ok 74        + create_index                              294 ms
ok 75        + create_index_spgist                       173 ms
ok 76        + create_view                               119 ms
ok 77        + index_including                            77 ms
ok 78        + index_including_gist                       73 ms
# parallel group (16 tests):  create_cast errors hash_func roleattributes create_aggregate drop_if_exists select typed_table create_function_sql create_am infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           22 ms
ok 80        + create_function_sql                        53 ms
ok 81        + create_cast                                10 ms
ok 82        + constraints                               258 ms
ok 83        + triggers                                  325 ms
ok 84        + select                                     32 ms
ok 85        + inherit                                   371 ms
ok 86        + typed_table                                47 ms
ok 87        + vacuum                                    163 ms
ok 88        + drop_if_exists                             23 ms
ok 89        + updatable_views                           264 ms
ok 90        + roleattributes                             16 ms
ok 91        + create_am                                  63 ms
ok 92        + hash_func                                  13 ms
ok 93        + errors                                     12 ms
ok 94        + infinite_recurse                           82 ms
ok 95        - sanity_check                               40 ms
# parallel group (20 tests):  select_distinct_on select_having select_implicit delete case namespace random prepared_xacts portals select_into select_distinct transactions union subselect arrays update aggregates hash_index join btree_index
ok 96        + select_into                                61 ms
ok 97        + select_distinct                            67 ms
ok 98        + select_distinct_on                         22 ms
ok 99        + select_implicit                            25 ms
ok 100       + select_having                              24 ms
ok 101       + subselect                                 118 ms
ok 102       + union                                      81 ms
ok 103       + case                                       26 ms
ok 104       + join                                      308 ms
ok 105       + aggregates                                258 ms
ok 106       + transactions                               78 ms
ok 107       + random                                     39 ms
ok 108       + portals                                    52 ms
ok 109       + arrays                                    123 ms
ok 110       + btree_index                               456 ms
ok 111       + hash_index                                266 ms
ok 112       + update                                    170 ms
ok 113       + delete                                     25 ms
ok 114       + namespace                                  35 ms
ok 115       + prepared_xacts                             43 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets identity matview rowsecurity generated_stored spgist gin gist join_hash brin privileges
ok 116       + brin                                      585 ms
ok 117       + gin                                       376 ms
ok 118       + gist                                      407 ms
ok 119       + spgist                                    344 ms
ok 120       + privileges                                725 ms
ok 121       + init_privs                                  9 ms
ok 122       + security_label                             46 ms
ok 123       + collate                                   115 ms
ok 124       + matview                                   247 ms
ok 125       + lock                                       71 ms
ok 126       + replica_identity                          145 ms
ok 127       + rowsecurity                               329 ms
ok 128       + object_address                             86 ms
ok 129       + tablesample                                50 ms
ok 130       + groupingsets                              173 ms
ok 131       + drop_operator                              24 ms
ok 132       + password                                  107 ms
ok 133       + identity                                  242 ms
ok 134       + generated_stored                          339 ms
ok 135       + join_hash                                 577 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 39 ms
ok 137       + brin_multi                                 82 ms
# parallel group (20 tests):  async dbsize nls tsrf alter_operator tidscan collate.utf8 tid sysviews create_role misc_functions alter_generic tidrangescan misc incremental_sort merge create_table_like collate.icu.utf8 generated_virtual without_overlaps
ok 138       + create_table_like                         152 ms
ok 139       + alter_generic                              60 ms
ok 140       + alter_operator                             26 ms
ok 141       + misc                                       77 ms
ok 142       + async                                       7 ms
ok 143       + dbsize                                      9 ms
ok 144       + merge                                     144 ms
ok 145       + misc_functions                             52 ms
ok 146       + nls                                        11 ms
ok 147       + sysviews                                   38 ms
ok 148       + tsrf                                       21 ms
ok 149       + tid                                        36 ms
ok 150       + tidscan                                    31 ms
ok 151       + tidrangescan                               60 ms
ok 152       + collate.utf8                               34 ms
ok 153       + collate.icu.utf8                          174 ms
ok 154       + incremental_sort                           77 ms
ok 155       + create_role                                45 ms
ok 156       + without_overlaps                          281 ms
ok 157       + generated_virtual                         214 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_pipeline psql_crosstab rules psql stats_ext
ok 158       + rules                                     114 ms
ok 159       + psql                                      260 ms
ok 160       + psql_crosstab                              13 ms
ok 161       + psql_pipeline                              13 ms
ok 162       + amutils                                     8 ms
ok 163       + stats_ext                                 682 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     4 ms
ok 166       - select_parallel                           330 ms
ok 167       - write_parallel                             35 ms
ok 168       - vacuum_parallel                            39 ms
ok 169       - maintain_every                             12 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               179 ms
ok 171       + subscription                               29 ms
# parallel group (18 tests):  portals_p2 advisory_lock xmlmap combocid functional_deps tsdicts guc equivclass dependency stats_rewrite select_views window bitmapops tsearch cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               70 ms
ok 173       + portals_p2                                 11 ms
ok 174       + foreign_key                               451 ms
ok 175       + cluster                                   148 ms
ok 176       + dependency                                 55 ms
ok 177       + guc                                        48 ms
ok 178       + bitmapops                                 132 ms
ok 179       + combocid                                   34 ms
ok 180       + tsearch                                   141 ms
ok 181       + tsdicts                                    47 ms
ok 182       + foreign_data                              266 ms
ok 183       + window                                    113 ms
ok 184       + xmlmap                                     24 ms
ok 185       + functional_deps                            46 ms
ok 186       + advisory_lock                              19 ms
ok 187       + indirect_toast                            169 ms
ok 188       + equivclass                                 47 ms
ok 189       + stats_rewrite                              66 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable json sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                       38 ms
ok 191       + jsonb                                     103 ms
ok 192       + json_encoding                               7 ms
ok 193       + jsonpath                                   15 ms
ok 194       + jsonpath_encoding                           7 ms
ok 195       + jsonb_jsonpath                             49 ms
ok 196       + sqljson                                    30 ms
ok 197       + sqljson_queryfuncs                         39 ms
ok 198       + sqljson_jsontable                          31 ms
# parallel group (18 tests):  prepare limit plancache xml conversion returning polymorphism rowtypes sequence copy2 with largeobject temp truncate rangefuncs domain plpgsql alter_table
ok 199       + plancache                                  54 ms
ok 200       + limit                                      47 ms
ok 201       + plpgsql                                   226 ms
ok 202       + copy2                                     112 ms
ok 203       + temp                                      127 ms
ok 204       + domain                                    164 ms
ok 205       + rangefuncs                                135 ms
ok 206       + prepare                                    17 ms
ok 207       + conversion                                 80 ms
ok 208       + truncate                                  133 ms
ok 209       + alter_table                               522 ms
ok 210       + sequence                                  109 ms
ok 211       + polymorphism                               93 ms
ok 212       + rowtypes                                   97 ms
ok 213       + returning                                  79 ms
ok 214       + largeobject                               122 ms
ok 215       + with                                      117 ms
ok 216       + xml                                        60 ms
# parallel group (18 tests):  numa compression_lz4 hash_part partition_info reloptions explain predicate compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_join partition_prune tuplesort indexing stats
ok 217       + partition_merge                           230 ms
ok 218       + partition_split                           255 ms
ok 219       + partition_join                            334 ms
ok 220       + partition_prune                           370 ms
ok 221       + reloptions                                 46 ms
ok 222       + hash_part                                  21 ms
ok 223       + indexing                                  429 ms
ok 224       + partition_aggregate                       312 ms
ok 225       + partition_info                             44 ms
ok 226       + tuplesort                                 425 ms
ok 227       + explain                                    49 ms
ok 228       + compression                                74 ms
ok 229       + compression_lz4                            10 ms
ok 230       + memoize                                    93 ms
ok 231       + stats                                     483 ms
ok 232       + predicate                                  50 ms
ok 233       + numa                                        7 ms
ok 234       + eager_aggregate                           143 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   81 ms
ok 236       + event_trigger                              95 ms
ok 237       - event_trigger_login                        19 ms
ok 238       - fast_default                               57 ms
ok 239       - tablespace                                182 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
