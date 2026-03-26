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
# using temp instance on port 58928 with PID 1940453
ok 1         - test_setup                                105 ms
# parallel group (20 tests):  char text name int2 varchar pg_lsn int4 oid float4 txid int8 money boolean regproc bit float8 enum uuid numeric rangetypes
ok 2         + boolean                                    21 ms
ok 3         + char                                       10 ms
ok 4         + name                                       12 ms
ok 5         + varchar                                    12 ms
ok 6         + text                                       12 ms
ok 7         + int2                                       12 ms
ok 8         + int4                                       14 ms
ok 9         + int8                                       18 ms
ok 10        + oid                                        16 ms
ok 11        + float4                                     17 ms
ok 12        + float8                                     26 ms
ok 13        + bit                                        25 ms
ok 14        + numeric                                   114 ms
ok 15        + txid                                       17 ms
ok 16        + uuid                                       46 ms
ok 17        + enum                                       29 ms
ok 18        + money                                      17 ms
ok 19        + rangetypes                                142 ms
ok 20        + pg_lsn                                     12 ms
ok 21        + regproc                                    21 ms
# parallel group (20 tests):  md5 lseg line path time point circle macaddr8 macaddr numerology timetz date inet interval strings polygon box multirangetypes timestamp timestamptz
ok 22        + strings                                    56 ms
ok 23        + md5                                         8 ms
ok 24        + numerology                                 22 ms
ok 25        + point                                      14 ms
ok 26        + lseg                                        9 ms
ok 27        + line                                        9 ms
ok 28        + box                                        87 ms
ok 29        + path                                       10 ms
ok 30        + polygon                                    76 ms
ok 31        + circle                                     14 ms
ok 32        + date                                       22 ms
ok 33        + time                                       12 ms
ok 34        + timetz                                     21 ms
ok 35        + timestamp                                 333 ms
ok 36        + timestamptz                               348 ms
ok 37        + interval                                   35 ms
ok 38        + inet                                       25 ms
ok 39        + macaddr                                    18 ms
ok 40        + macaddr8                                   13 ms
ok 41        + multirangetypes                            95 ms
# parallel group (19 tests):  unicode comments euc_kr misc_sanity pg_ndistinct tstypes pg_dependencies expressions encoding oid8 xid mvcc horology geometry type_sanity stats_import database opr_sanity regex
ok 42        + geometry                                   46 ms
ok 43        + horology                                   39 ms
ok 44        + tstypes                                    19 ms
ok 45        + regex                                     180 ms
ok 46        + type_sanity                                47 ms
ok 47        + opr_sanity                                126 ms
ok 48        + misc_sanity                                11 ms
ok 49        + comments                                   10 ms
ok 50        + expressions                                21 ms
ok 51        + unicode                                     7 ms
ok 52        + xid                                        29 ms
ok 53        + mvcc                                       34 ms
ok 54        + database                                  123 ms
ok 55        + stats_import                               88 ms
ok 56        + pg_ndistinct                               15 ms
ok 57        + pg_dependencies                            18 ms
ok 58        + oid8                                       27 ms
ok 59        + encoding                                   25 ms
ok 60        + euc_kr                                      9 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       49 ms
ok 62        + copyselect                                 12 ms
ok 63        + copydml                                    14 ms
ok 64        + copyencoding                                9 ms
ok 65        + insert                                    102 ms
ok 66        + insert_conflict                            63 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_misc create_schema create_procedure create_table
ok 67        + create_function_c                           6 ms
ok 68        + create_misc                                17 ms
ok 69        + create_operator                            10 ms
ok 70        + create_procedure                           26 ms
ok 71        + create_table                              112 ms
ok 72        + create_type                                15 ms
ok 73        + create_schema                              18 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              307 ms
ok 75        + create_index_spgist                       186 ms
ok 76        + create_view                               122 ms
ok 77        + index_including                            64 ms
ok 78        + index_including_gist                      104 ms
# parallel group (16 tests):  create_cast roleattributes errors hash_func create_aggregate drop_if_exists select typed_table create_am create_function_sql infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           21 ms
ok 80        + create_function_sql                        61 ms
ok 81        + create_cast                                10 ms
ok 82        + constraints                               227 ms
ok 83        + triggers                                  327 ms
ok 84        + select                                     29 ms
ok 85        + inherit                                   381 ms
ok 86        + typed_table                                49 ms
ok 87        + vacuum                                    160 ms
ok 88        + drop_if_exists                             27 ms
ok 89        + updatable_views                           254 ms
ok 90        + roleattributes                             12 ms
ok 91        + create_am                                  59 ms
ok 92        + hash_func                                  18 ms
ok 93        + errors                                     12 ms
ok 94        + infinite_recurse                          112 ms
ok 95        - sanity_check                               43 ms
# parallel group (20 tests):  select_having delete case select_distinct_on select_implicit namespace random select_into prepared_xacts portals transactions select_distinct union arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                                38 ms
ok 97        + select_distinct                            67 ms
ok 98        + select_distinct_on                         22 ms
ok 99        + select_implicit                            30 ms
ok 100       + select_having                              17 ms
ok 101       + subselect                                 127 ms
ok 102       + union                                      71 ms
ok 103       + case                                       20 ms
ok 104       + join                                      322 ms
ok 105       + aggregates                                275 ms
ok 106       + transactions                               60 ms
ok 107       + random                                     36 ms
ok 108       + portals                                    53 ms
ok 109       + arrays                                    108 ms
ok 110       + btree_index                               453 ms
ok 111       + hash_index                                243 ms
ok 112       + update                                    174 ms
ok 113       + delete                                     18 ms
ok 114       + namespace                                  32 ms
ok 115       + prepared_xacts                             41 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets matview identity rowsecurity generated_stored spgist gin gist join_hash brin privileges
ok 116       + brin                                      590 ms
ok 117       + gin                                       362 ms
ok 118       + gist                                      370 ms
ok 119       + spgist                                    301 ms
ok 120       + privileges                                741 ms
ok 121       + init_privs                                  9 ms
ok 122       + security_label                             25 ms
ok 123       + collate                                   101 ms
ok 124       + matview                                   216 ms
ok 125       + lock                                       53 ms
ok 126       + replica_identity                          123 ms
ok 127       + rowsecurity                               290 ms
ok 128       + object_address                             94 ms
ok 129       + tablesample                                46 ms
ok 130       + groupingsets                              142 ms
ok 131       + drop_operator                              17 ms
ok 132       + password                                   94 ms
ok 133       + identity                                  219 ms
ok 134       + generated_stored                          289 ms
ok 135       + join_hash                                 579 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 31 ms
ok 137       + brin_multi                                 90 ms
# parallel group (20 tests):  async nls dbsize alter_operator tidscan collate.utf8 tsrf tid sysviews create_role alter_generic misc_functions tidrangescan misc incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         187 ms
ok 139       + alter_generic                              54 ms
ok 140       + alter_operator                             23 ms
ok 141       + misc                                       65 ms
ok 142       + async                                      10 ms
ok 143       + dbsize                                     12 ms
ok 144       + merge                                     147 ms
ok 145       + misc_functions                             54 ms
ok 146       + nls                                         9 ms
ok 147       + sysviews                                   33 ms
ok 148       + tsrf                                       29 ms
ok 149       + tid                                        33 ms
ok 150       + tidscan                                    24 ms
ok 151       + tidrangescan                               60 ms
ok 152       + collate.utf8                               25 ms
ok 153       + collate.icu.utf8                          173 ms
ok 154       + incremental_sort                           68 ms
ok 155       + create_role                                38 ms
ok 156       + without_overlaps                          255 ms
ok 157       + generated_virtual                         202 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     159 ms
ok 159       + psql                                      260 ms
ok 160       + psql_crosstab                              10 ms
ok 161       + psql_pipeline                              15 ms
ok 162       + amutils                                     9 ms
ok 163       + stats_ext                                 647 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           329 ms
ok 167       - write_parallel                             36 ms
ok 168       - vacuum_parallel                            49 ms
ok 169       - maintain_every                             12 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               177 ms
ok 171       + subscription                               30 ms
# parallel group (18 tests):  combocid advisory_lock portals_p2 xmlmap tsdicts functional_deps guc equivclass dependency select_views stats_rewrite window tsearch bitmapops cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               58 ms
ok 173       + portals_p2                                 21 ms
ok 174       + foreign_key                               448 ms
ok 175       + cluster                                   145 ms
ok 176       + dependency                                 52 ms
ok 177       + guc                                        39 ms
ok 178       + bitmapops                                 138 ms
ok 179       + combocid                                   19 ms
ok 180       + tsearch                                   134 ms
ok 181       + tsdicts                                    35 ms
ok 182       + foreign_data                              245 ms
ok 183       + window                                    115 ms
ok 184       + xmlmap                                     26 ms
ok 185       + functional_deps                            37 ms
ok 186       + advisory_lock                              19 ms
ok 187       + indirect_toast                            155 ms
ok 188       + equivclass                                 42 ms
ok 189       + stats_rewrite                              62 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson_jsontable sqljson_queryfuncs sqljson json jsonb_jsonpath jsonb
ok 190       + json                                       42 ms
ok 191       + jsonb                                     107 ms
ok 192       + json_encoding                               7 ms
ok 193       + jsonpath                                   12 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             42 ms
ok 196       + sqljson                                    40 ms
ok 197       + sqljson_queryfuncs                         37 ms
ok 198       + sqljson_jsontable                          32 ms
# parallel group (18 tests):  prepare limit xml plancache conversion returning rowtypes polymorphism sequence copy2 with largeobject rangefuncs temp truncate domain plpgsql alter_table
ok 199       + plancache                                  83 ms
ok 200       + limit                                      41 ms
ok 201       + plpgsql                                   292 ms
ok 202       + copy2                                     135 ms
ok 203       + temp                                      160 ms
ok 204       + domain                                    200 ms
ok 205       + rangefuncs                                152 ms
ok 206       + prepare                                    22 ms
ok 207       + conversion                                 84 ms
ok 208       + truncate                                  173 ms
ok 209       + alter_table                               610 ms
ok 210       + sequence                                  132 ms
ok 211       + polymorphism                              120 ms
ok 212       + rowtypes                                  112 ms
ok 213       + returning                                  88 ms
ok 214       + largeobject                               144 ms
ok 215       + with                                      137 ms
ok 216       + xml                                        44 ms
# parallel group (18 tests):  compression_lz4 numa hash_part predicate reloptions partition_info explain compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_join partition_prune tuplesort indexing stats
ok 217       + partition_merge                           269 ms
ok 218       + partition_split                           304 ms
ok 219       + partition_join                            409 ms
ok 220       + partition_prune                           427 ms
ok 221       + reloptions                                 51 ms
ok 222       + hash_part                                  26 ms
ok 223       + indexing                                  464 ms
ok 224       + partition_aggregate                       350 ms
ok 225       + partition_info                             52 ms
ok 226       + tuplesort                                 445 ms
ok 227       + explain                                    58 ms
ok 228       + compression                                79 ms
ok 229       + compression_lz4                             8 ms
ok 230       + memoize                                    91 ms
ok 231       + stats                                     496 ms
ok 232       + predicate                                  38 ms
ok 233       + numa                                        9 ms
ok 234       + eager_aggregate                           165 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   81 ms
ok 236       + event_trigger                              94 ms
ok 237       - event_trigger_login                        17 ms
ok 238       - fast_default                               52 ms
ok 239       - tablespace                                163 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
