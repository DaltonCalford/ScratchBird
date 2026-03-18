Last updated: 2026-03-18

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
ake -j1  checkprep >>'<outside-tree-path>'/tmp_install/log/install.log 2>&1
PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  initdb --auth trust --no-sync --no-instructions --lc-messages=C --no-clean '<outside-tree-path>'/tmp_install/initdb-template >>'<outside-tree-path>'/tmp_install/log/initdb-template.log 2>&1
echo "# +++ regress check in src/test/regress +++" && PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  ../../../src/test/regress/pg_regress --temp-instance=./tmp_check --inputdir=<outside-tree-path> --bindir=     --dlpath=. --max-concurrent-tests=20  --schedule=<outside-tree-path>  
# +++ regress check in src/test/regress +++
# initializing database system by copying initdb template
# using temp instance on port 58928 with PID 464888
ok 1         - test_setup                                236 ms
# parallel group (20 tests):  char varchar text int2 int4 name oid money int8 boolean pg_lsn regproc txid float4 float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    43 ms
ok 3         + char                                       21 ms
ok 4         + name                                       26 ms
ok 5         + varchar                                    20 ms
ok 6         + text                                       20 ms
ok 7         + int2                                       20 ms
ok 8         + int4                                       22 ms
ok 9         + int8                                       36 ms
ok 10        + oid                                        25 ms
ok 11        + float4                                     44 ms
ok 12        + float8                                     56 ms
ok 13        + bit                                        65 ms
ok 14        + numeric                                   272 ms
ok 15        + txid                                       43 ms
ok 16        + uuid                                      110 ms
ok 17        + enum                                       71 ms
ok 18        + money                                      33 ms
ok 19        + rangetypes                                383 ms
ok 20        + pg_lsn                                     40 ms
ok 21        + regproc                                    39 ms
# parallel group (20 tests):  md5 line lseg point time path numerology circle macaddr timetz date macaddr8 inet interval strings polygon box multirangetypes timestamp timestamptz
ok 22        + strings                                   148 ms
ok 23        + md5                                        13 ms
ok 24        + numerology                                 36 ms
ok 25        + point                                      26 ms
ok 26        + lseg                                       20 ms
ok 27        + line                                       17 ms
ok 28        + box                                       185 ms
ok 29        + path                                       31 ms
ok 30        + polygon                                   180 ms
ok 31        + circle                                     41 ms
ok 32        + date                                       51 ms
ok 33        + time                                       27 ms
ok 34        + timetz                                     47 ms
ok 35        + timestamp                                 374 ms
ok 36        + timestamptz                               401 ms
ok 37        + interval                                   93 ms
ok 38        + inet                                       59 ms
ok 39        + macaddr                                    39 ms
ok 40        + macaddr8                                   47 ms
ok 41        + multirangetypes                           237 ms
# parallel group (19 tests):  comments unicode oid8 euc_kr tstypes misc_sanity pg_ndistinct pg_dependencies encoding xid expressions mvcc geometry horology type_sanity stats_import database opr_sanity regex
ok 42        + geometry                                   95 ms
ok 43        + horology                                  100 ms
ok 44        + tstypes                                    43 ms
ok 45        + regex                                     457 ms
ok 46        + type_sanity                               120 ms
ok 47        + opr_sanity                                331 ms
ok 48        + misc_sanity                                48 ms
ok 49        + comments                                   13 ms
ok 50        + expressions                                78 ms
ok 51        + unicode                                    13 ms
ok 52        + xid                                        73 ms
ok 53        + mvcc                                       88 ms
ok 54        + database                                  315 ms
ok 55        + stats_import                              193 ms
ok 56        + pg_ndistinct                               54 ms
ok 57        + pg_dependencies                            56 ms
ok 58        + oid8                                       33 ms
ok 59        + encoding                                   60 ms
ok 60        + euc_kr                                     28 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                      126 ms
ok 62        + copyselect                                 28 ms
ok 63        + copydml                                    32 ms
ok 64        + copyencoding                               20 ms
ok 65        + insert                                    358 ms
ok 66        + insert_conflict                           146 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_schema create_misc create_procedure create_table
ok 67        + create_function_c                          10 ms
ok 68        + create_misc                                45 ms
ok 69        + create_operator                            23 ms
ok 70        + create_procedure                           47 ms
ok 71        + create_table                              332 ms
ok 72        + create_type                                29 ms
ok 73        + create_schema                              38 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              624 ms
ok 75        + create_index_spgist                       349 ms
ok 76        + create_view                               272 ms
ok 77        + index_including                           116 ms
ok 78        + index_including_gist                      222 ms
# parallel group (16 tests):  create_cast errors roleattributes hash_func create_aggregate drop_if_exists select typed_table create_am create_function_sql infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           43 ms
ok 80        + create_function_sql                       126 ms
ok 81        + create_cast                                28 ms
ok 82        + constraints                               520 ms
ok 83        + triggers                                  747 ms
ok 84        + select                                     70 ms
ok 85        + inherit                                   921 ms
ok 86        + typed_table                                99 ms
ok 87        + vacuum                                    372 ms
ok 88        + drop_if_exists                             57 ms
ok 89        + updatable_views                           555 ms
ok 90        + roleattributes                             33 ms
ok 91        + create_am                                 123 ms
ok 92        + hash_func                                  37 ms
ok 93        + errors                                     28 ms
ok 94        + infinite_recurse                          184 ms
ok 95        - sanity_check                               73 ms
# parallel group (20 tests):  select_having select_distinct_on select_implicit delete case random prepared_xacts namespace select_into portals transactions union select_distinct arrays subselect update aggregates hash_index join btree_index
ok 96        + select_into                               119 ms
ok 97        + select_distinct                           168 ms
ok 98        + select_distinct_on                         52 ms
ok 99        + select_implicit                            58 ms
ok 100       + select_having                              27 ms
ok 101       + subselect                                 341 ms
ok 102       + union                                     153 ms
ok 103       + case                                       61 ms
ok 104       + join                                      663 ms
ok 105       + aggregates                                582 ms
ok 106       + transactions                              146 ms
ok 107       + random                                     83 ms
ok 108       + portals                                   135 ms
ok 109       + arrays                                    278 ms
ok 110       + btree_index                              1269 ms
ok 111       + hash_index                                602 ms
ok 112       + update                                    383 ms
ok 113       + delete                                     58 ms
ok 114       + namespace                                 109 ms
ok 115       + prepared_xacts                             92 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets matview identity rowsecurity spgist generated_stored gin gist join_hash brin privileges
ok 116       + brin                                     1410 ms
ok 117       + gin                                       884 ms
ok 118       + gist                                      933 ms
ok 119       + spgist                                    734 ms
ok 120       + privileges                               1727 ms
ok 121       + init_privs                                 28 ms
ok 122       + security_label                             54 ms
ok 123       + collate                                   239 ms
ok 124       + matview                                   508 ms
ok 125       + lock                                      111 ms
ok 126       + replica_identity                          278 ms
ok 127       + rowsecurity                               691 ms
ok 128       + object_address                            193 ms
ok 129       + tablesample                                92 ms
ok 130       + groupingsets                              390 ms
ok 131       + drop_operator                              26 ms
ok 132       + password                                  191 ms
ok 133       + identity                                  530 ms
ok 134       + generated_stored                          746 ms
ok 135       + join_hash                                1389 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 92 ms
ok 137       + brin_multi                                289 ms
# parallel group (20 tests):  async dbsize nls alter_operator collate.utf8 tsrf tid tidscan sysviews create_role misc misc_functions alter_generic tidrangescan incremental_sort merge collate.icu.utf8 create_table_like generated_virtual without_overlaps
ok 138       + create_table_like                         435 ms
ok 139       + alter_generic                             153 ms
ok 140       + alter_operator                             38 ms
ok 141       + misc                                      120 ms
ok 142       + async                                      24 ms
ok 143       + dbsize                                     24 ms
ok 144       + merge                                     319 ms
ok 145       + misc_functions                            129 ms
ok 146       + nls                                        30 ms
ok 147       + sysviews                                   70 ms
ok 148       + tsrf                                       61 ms
ok 149       + tid                                        61 ms
ok 150       + tidscan                                    60 ms
ok 151       + tidrangescan                              151 ms
ok 152       + collate.utf8                               53 ms
ok 153       + collate.icu.utf8                          417 ms
ok 154       + incremental_sort                          182 ms
ok 155       + create_role                               112 ms
ok 156       + without_overlaps                          732 ms
ok 157       + generated_virtual                         433 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     362 ms
ok 159       + psql                                      570 ms
ok 160       + psql_crosstab                              29 ms
ok 161       + psql_pipeline                              33 ms
ok 162       + amutils                                    18 ms
ok 163       + stats_ext                                1530 ms
ok 164       + collate.linux.utf8                         11 ms
ok 165       + collate.windows.win1252                    10 ms
ok 166       - select_parallel                           670 ms
ok 167       - write_parallel                             75 ms
ok 168       - vacuum_parallel                            88 ms
ok 169       - maintain_every                             23 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               431 ms
ok 171       + subscription                               52 ms
# parallel group (18 tests):  portals_p2 xmlmap combocid advisory_lock tsdicts functional_deps equivclass guc dependency select_views stats_rewrite window tsearch bitmapops cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                              141 ms
ok 173       + portals_p2                                 20 ms
ok 174       + foreign_key                              1244 ms
ok 175       + cluster                                   349 ms
ok 176       + dependency                                126 ms
ok 177       + guc                                       112 ms
ok 178       + bitmapops                                 306 ms
ok 179       + combocid                                   49 ms
ok 180       + tsearch                                   296 ms
ok 181       + tsdicts                                    74 ms
ok 182       + foreign_data                              589 ms
ok 183       + window                                    273 ms
ok 184       + xmlmap                                     45 ms
ok 185       + functional_deps                            80 ms
ok 186       + advisory_lock                              51 ms
ok 187       + indirect_toast                            406 ms
ok 188       + equivclass                                 97 ms
ok 189       + stats_rewrite                             148 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson_jsontable json sqljson sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                       63 ms
ok 191       + jsonb                                     229 ms
ok 192       + json_encoding                              14 ms
ok 193       + jsonpath                                   26 ms
ok 194       + jsonpath_encoding                          11 ms
ok 195       + jsonb_jsonpath                            101 ms
ok 196       + sqljson                                    70 ms
ok 197       + sqljson_queryfuncs                         78 ms
ok 198       + sqljson_jsontable                          61 ms
# parallel group (18 tests):  prepare limit xml plancache conversion returning polymorphism copy2 rowtypes sequence with rangefuncs largeobject truncate temp domain plpgsql alter_table
ok 199       + plancache                                 120 ms
ok 200       + limit                                      91 ms
ok 201       + plpgsql                                   619 ms
ok 202       + copy2                                     236 ms
ok 203       + temp                                      313 ms
ok 204       + domain                                    337 ms
ok 205       + rangefuncs                                276 ms
ok 206       + prepare                                    33 ms
ok 207       + conversion                                135 ms
ok 208       + truncate                                  303 ms
ok 209       + alter_table                              1396 ms
ok 210       + sequence                                  254 ms
ok 211       + polymorphism                              222 ms
ok 212       + rowtypes                                  237 ms
ok 213       + returning                                 178 ms
ok 214       + largeobject                               293 ms
ok 215       + with                                      268 ms
ok 216       + xml                                        96 ms
# parallel group (18 tests):  numa compression_lz4 hash_part partition_info explain predicate reloptions memoize compression eager_aggregate partition_merge partition_split partition_aggregate stats partition_join partition_prune tuplesort indexing
ok 217       + partition_merge                           583 ms
ok 218       + partition_split                           629 ms
ok 219       + partition_join                            880 ms
ok 220       + partition_prune                           897 ms
ok 221       + reloptions                                110 ms
ok 222       + hash_part                                  52 ms
ok 223       + indexing                                 1039 ms
ok 224       + partition_aggregate                       735 ms
ok 225       + partition_info                             91 ms
ok 226       + tuplesort                                 910 ms
ok 227       + explain                                    93 ms
ok 228       + compression                               213 ms
ok 229       + compression_lz4                            17 ms
ok 230       + memoize                                   187 ms
ok 231       + stats                                     833 ms
ok 232       + predicate                                 102 ms
ok 233       + numa                                       13 ms
ok 234       + eager_aggregate                           319 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                  176 ms
ok 236       + event_trigger                             210 ms
ok 237       - event_trigger_login                        36 ms
ok 238       - fast_default                              179 ms
ok 239       - tablespace                                485 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
