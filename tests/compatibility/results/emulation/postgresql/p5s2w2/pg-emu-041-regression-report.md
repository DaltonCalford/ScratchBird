Last updated: 2026-03-11

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
# using temp instance on port 58928 with PID 2397535
ok 1         - test_setup                                193 ms
# parallel group (20 tests):  text int2 int4 varchar oid char name txid pg_lsn boolean float4 regproc bit float8 money int8 enum uuid numeric rangetypes
ok 2         + boolean                                    37 ms
ok 3         + char                                       23 ms
ok 4         + name                                       29 ms
ok 5         + varchar                                    20 ms
ok 6         + text                                       15 ms
ok 7         + int2                                       16 ms
ok 8         + int4                                       19 ms
ok 9         + int8                                       51 ms
ok 10        + oid                                        20 ms
ok 11        + float4                                     36 ms
ok 12        + float8                                     45 ms
ok 13        + bit                                        45 ms
ok 14        + numeric                                   195 ms
ok 15        + txid                                       28 ms
ok 16        + uuid                                       77 ms
ok 17        + enum                                       49 ms
ok 18        + money                                      43 ms
ok 19        + rangetypes                                267 ms
ok 20        + pg_lsn                                     31 ms
ok 21        + regproc                                    36 ms
# parallel group (20 tests):  lseg line md5 time path circle point macaddr numerology timetz date macaddr8 inet interval strings box polygon multirangetypes timestamp timestamptz
ok 22        + strings                                   106 ms
ok 23        + md5                                        17 ms
ok 24        + numerology                                 37 ms
ok 25        + point                                      30 ms
ok 26        + lseg                                       15 ms
ok 27        + line                                       14 ms
ok 28        + box                                       143 ms
ok 29        + path                                       20 ms
ok 30        + polygon                                   156 ms
ok 31        + circle                                     28 ms
ok 32        + date                                       37 ms
ok 33        + time                                       16 ms
ok 34        + timetz                                     36 ms
ok 35        + timestamp                                 342 ms
ok 36        + timestamptz                               363 ms
ok 37        + interval                                   66 ms
ok 38        + inet                                       48 ms
ok 39        + macaddr                                    29 ms
ok 40        + macaddr8                                   41 ms
ok 41        + multirangetypes                           183 ms
# parallel group (19 tests):  comments unicode misc_sanity euc_kr pg_ndistinct oid8 tstypes pg_dependencies encoding expressions xid horology mvcc geometry type_sanity stats_import opr_sanity database regex
ok 42        + geometry                                   92 ms
ok 43        + horology                                   72 ms
ok 44        + tstypes                                    41 ms
ok 45        + regex                                     390 ms
ok 46        + type_sanity                                94 ms
ok 47        + opr_sanity                                250 ms
ok 48        + misc_sanity                                14 ms
ok 49        + comments                                   10 ms
ok 50        + expressions                                52 ms
ok 51        + unicode                                    13 ms
ok 52        + xid                                        58 ms
ok 53        + mvcc                                       76 ms
ok 54        + database                                  256 ms
ok 55        + stats_import                              178 ms
ok 56        + pg_ndistinct                               30 ms
ok 57        + pg_dependencies                            39 ms
ok 58        + oid8                                       35 ms
ok 59        + encoding                                   43 ms
ok 60        + euc_kr                                     15 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       99 ms
ok 62        + copyselect                                 23 ms
ok 63        + copydml                                    23 ms
ok 64        + copyencoding                               16 ms
ok 65        + insert                                    271 ms
ok 66        + insert_conflict                           121 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_schema create_misc create_procedure create_table
ok 67        + create_function_c                           9 ms
ok 68        + create_misc                                43 ms
ok 69        + create_operator                            21 ms
ok 70        + create_procedure                           59 ms
ok 71        + create_table                              305 ms
ok 72        + create_type                                33 ms
ok 73        + create_schema                              34 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              705 ms
ok 75        + create_index_spgist                       381 ms
ok 76        + create_view                               255 ms
ok 77        + index_including                           118 ms
ok 78        + index_including_gist                      197 ms
# parallel group (16 tests):  create_cast errors create_aggregate roleattributes drop_if_exists select hash_func create_function_sql typed_table create_am infinite_recurse vacuum updatable_views constraints triggers inherit
ok 79        + create_aggregate                           30 ms
ok 80        + create_function_sql                       111 ms
ok 81        + create_cast                                16 ms
ok 82        + constraints                               528 ms
ok 83        + triggers                                  737 ms
ok 84        + select                                     50 ms
ok 85        + inherit                                   874 ms
ok 86        + typed_table                               114 ms
ok 87        + vacuum                                    326 ms
ok 88        + drop_if_exists                             49 ms
ok 89        + updatable_views                           502 ms
ok 90        + roleattributes                             36 ms
ok 91        + create_am                                 140 ms
ok 92        + hash_func                                  59 ms
ok 93        + errors                                     27 ms
ok 94        + infinite_recurse                          219 ms
ok 95        - sanity_check                              110 ms
# parallel group (20 tests):  select_distinct_on select_having case select_implicit delete random prepared_xacts namespace select_into portals union transactions select_distinct arrays subselect update hash_index aggregates join btree_index
ok 96        + select_into                               103 ms
ok 97        + select_distinct                           152 ms
ok 98        + select_distinct_on                         41 ms
ok 99        + select_implicit                            45 ms
ok 100       + select_having                              44 ms
ok 101       + subselect                                 300 ms
ok 102       + union                                     136 ms
ok 103       + case                                       43 ms
ok 104       + join                                      657 ms
ok 105       + aggregates                                574 ms
ok 106       + transactions                              145 ms
ok 107       + random                                     53 ms
ok 108       + portals                                   106 ms
ok 109       + arrays                                    294 ms
ok 110       + btree_index                              1097 ms
ok 111       + hash_index                                560 ms
ok 112       + update                                    357 ms
ok 113       + delete                                     44 ms
ok 114       + namespace                                  85 ms
ok 115       + prepared_xacts                             67 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets matview identity spgist rowsecurity generated_stored gin gist join_hash brin privileges
ok 116       + brin                                     1357 ms
ok 117       + gin                                       903 ms
ok 118       + gist                                      910 ms
ok 119       + spgist                                    691 ms
ok 120       + privileges                               1642 ms
ok 121       + init_privs                                 20 ms
ok 122       + security_label                             41 ms
ok 123       + collate                                   201 ms
ok 124       + matview                                   522 ms
ok 125       + lock                                      107 ms
ok 126       + replica_identity                          259 ms
ok 127       + rowsecurity                               773 ms
ok 128       + object_address                            126 ms
ok 129       + tablesample                                89 ms
ok 130       + groupingsets                              345 ms
ok 131       + drop_operator                              26 ms
ok 132       + password                                  172 ms
ok 133       + identity                                  580 ms
ok 134       + generated_stored                          780 ms
ok 135       + join_hash                                1345 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 59 ms
ok 137       + brin_multi                                192 ms
# parallel group (20 tests):  async nls dbsize tsrf tidscan collate.utf8 tid alter_operator sysviews create_role misc_functions alter_generic misc tidrangescan incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         394 ms
ok 139       + alter_generic                             141 ms
ok 140       + alter_operator                             70 ms
ok 141       + misc                                      145 ms
ok 142       + async                                      12 ms
ok 143       + dbsize                                     17 ms
ok 144       + merge                                     298 ms
ok 145       + misc_functions                            108 ms
ok 146       + nls                                        15 ms
ok 147       + sysviews                                   70 ms
ok 148       + tsrf                                       52 ms
ok 149       + tid                                        66 ms
ok 150       + tidscan                                    52 ms
ok 151       + tidrangescan                              146 ms
ok 152       + collate.utf8                               56 ms
ok 153       + collate.icu.utf8                          372 ms
ok 154       + incremental_sort                          170 ms
ok 155       + create_role                                99 ms
ok 156       + without_overlaps                          622 ms
ok 157       + generated_virtual                         482 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     260 ms
ok 159       + psql                                      470 ms
ok 160       + psql_crosstab                              27 ms
ok 161       + psql_pipeline                              33 ms
ok 162       + amutils                                    15 ms
ok 163       + stats_ext                                1374 ms
ok 164       + collate.linux.utf8                         10 ms
ok 165       + collate.windows.win1252                     8 ms
ok 166       - select_parallel                           574 ms
ok 167       - write_parallel                             59 ms
ok 168       - vacuum_parallel                            89 ms
ok 169       - maintain_every                             20 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               360 ms
ok 171       + subscription                               48 ms
# parallel group (18 tests):  portals_p2 advisory_lock tsdicts xmlmap combocid guc equivclass dependency functional_deps select_views stats_rewrite window bitmapops tsearch cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                              110 ms
ok 173       + portals_p2                                 26 ms
ok 174       + foreign_key                              1028 ms
ok 175       + cluster                                   293 ms
ok 176       + dependency                                 83 ms
ok 177       + guc                                        73 ms
ok 178       + bitmapops                                 252 ms
ok 179       + combocid                                   52 ms
ok 180       + tsearch                                   270 ms
ok 181       + tsdicts                                    39 ms
ok 182       + foreign_data                              500 ms
ok 183       + window                                    214 ms
ok 184       + xmlmap                                     42 ms
ok 185       + functional_deps                            91 ms
ok 186       + advisory_lock                              31 ms
ok 187       + indirect_toast                            300 ms
ok 188       + equivclass                                 78 ms
ok 189       + stats_rewrite                             126 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable json sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                       79 ms
ok 191       + jsonb                                     209 ms
ok 192       + json_encoding                              12 ms
ok 193       + jsonpath                                   28 ms
ok 194       + jsonpath_encoding                          10 ms
ok 195       + jsonb_jsonpath                             93 ms
ok 196       + sqljson                                    54 ms
ok 197       + sqljson_queryfuncs                         81 ms
ok 198       + sqljson_jsontable                          55 ms
# parallel group (18 tests):  prepare limit xml plancache conversion polymorphism returning rowtypes sequence copy2 largeobject rangefuncs temp truncate with domain plpgsql alter_table
ok 199       + plancache                                 124 ms
ok 200       + limit                                      82 ms
ok 201       + plpgsql                                   598 ms
ok 202       + copy2                                     233 ms
ok 203       + temp                                      294 ms
ok 204       + domain                                    350 ms
ok 205       + rangefuncs                                288 ms
ok 206       + prepare                                    41 ms
ok 207       + conversion                                123 ms
ok 208       + truncate                                  303 ms
ok 209       + alter_table                              1394 ms
ok 210       + sequence                                  228 ms
ok 211       + polymorphism                              155 ms
ok 212       + rowtypes                                  211 ms
ok 213       + returning                                 186 ms
ok 214       + largeobject                               256 ms
ok 215       + with                                      305 ms
ok 216       + xml                                        87 ms
# parallel group (18 tests):  compression_lz4 numa hash_part reloptions predicate explain partition_info compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_prune partition_join tuplesort stats indexing
ok 217       + partition_merge                           567 ms
ok 218       + partition_split                           595 ms
ok 219       + partition_join                            830 ms
ok 220       + partition_prune                           824 ms
ok 221       + reloptions                                 78 ms
ok 222       + hash_part                                  46 ms
ok 223       + indexing                                  977 ms
ok 224       + partition_aggregate                       696 ms
ok 225       + partition_info                            111 ms
ok 226       + tuplesort                                 867 ms
ok 227       + explain                                    96 ms
ok 228       + compression                               158 ms
ok 229       + compression_lz4                            15 ms
ok 230       + memoize                                   162 ms
ok 231       + stats                                     897 ms
ok 232       + predicate                                  86 ms
ok 233       + numa                                       13 ms
ok 234       + eager_aggregate                           263 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                  163 ms
ok 236       + event_trigger                             187 ms
ok 237       - event_trigger_login                        25 ms
ok 238       - fast_default                              110 ms
ok 239       - tablespace                                356 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
