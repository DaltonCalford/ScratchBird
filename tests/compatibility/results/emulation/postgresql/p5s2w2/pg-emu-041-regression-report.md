Last updated: 2026-03-03

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
# using temp instance on port 58928 with PID 623944
ok 1         - test_setup                                107 ms
# parallel group (20 tests):  name char oid text int4 int2 varchar txid money pg_lsn boolean regproc float4 float8 int8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    20 ms
ok 3         + char                                       10 ms
ok 4         + name                                       10 ms
ok 5         + varchar                                    15 ms
ok 6         + text                                       11 ms
ok 7         + int2                                       12 ms
ok 8         + int4                                       12 ms
ok 9         + int8                                       24 ms
ok 10        + oid                                        10 ms
ok 11        + float4                                     20 ms
ok 12        + float8                                     22 ms
ok 13        + bit                                        30 ms
ok 14        + numeric                                   119 ms
ok 15        + txid                                       15 ms
ok 16        + uuid                                       38 ms
ok 17        + enum                                       31 ms
ok 18        + money                                      17 ms
ok 19        + rangetypes                                158 ms
ok 20        + pg_lsn                                     16 ms
ok 21        + regproc                                    18 ms
# parallel group (20 tests):  md5 lseg line circle time path point macaddr timetz macaddr8 numerology date inet interval strings polygon box multirangetypes timestamp timestamptz
ok 22        + strings                                    61 ms
ok 23        + md5                                         7 ms
ok 24        + numerology                                 20 ms
ok 25        + point                                      17 ms
ok 26        + lseg                                       10 ms
ok 27        + line                                       10 ms
ok 28        + box                                        82 ms
ok 29        + path                                       13 ms
ok 30        + polygon                                    69 ms
ok 31        + circle                                     11 ms
ok 32        + date                                       24 ms
ok 33        + time                                       11 ms
ok 34        + timetz                                     16 ms
ok 35        + timestamp                                 330 ms
ok 36        + timestamptz                               330 ms
ok 37        + interval                                   37 ms
ok 38        + inet                                       26 ms
ok 39        + macaddr                                    15 ms
ok 40        + macaddr8                                   16 ms
ok 41        + multirangetypes                           101 ms
# parallel group (19 tests):  comments euc_kr pg_ndistinct unicode misc_sanity tstypes pg_dependencies xid encoding expressions geometry mvcc oid8 type_sanity horology stats_import opr_sanity database regex
ok 42        + geometry                                   47 ms
ok 43        + horology                                   68 ms
ok 44        + tstypes                                    26 ms
ok 45        + regex                                     204 ms
ok 46        + type_sanity                                61 ms
ok 47        + opr_sanity                                135 ms
ok 48        + misc_sanity                                17 ms
ok 49        + comments                                   10 ms
ok 50        + expressions                                41 ms
ok 51        + unicode                                    13 ms
ok 52        + xid                                        39 ms
ok 53        + mvcc                                       48 ms
ok 54        + database                                  148 ms
ok 55        + stats_import                              101 ms
ok 56        + pg_ndistinct                               12 ms
ok 57        + pg_dependencies                            24 ms
ok 58        + oid8                                       49 ms
ok 59        + encoding                                   40 ms
ok 60        + euc_kr                                     10 ms
# parallel group (6 tests):  copyencoding copyselect copydml insert_conflict copy insert
ok 61        + copy                                       50 ms
ok 62        + copyselect                                 10 ms
ok 63        + copydml                                    15 ms
ok 64        + copyencoding                                9 ms
ok 65        + insert                                    129 ms
ok 66        + insert_conflict                            49 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_schema create_misc create_procedure create_table
ok 67        + create_function_c                           5 ms
ok 68        + create_misc                                19 ms
ok 69        + create_operator                            10 ms
ok 70        + create_procedure                           27 ms
ok 71        + create_table                              134 ms
ok 72        + create_type                                13 ms
ok 73        + create_schema                              18 ms
# parallel group (5 tests):  index_including_gist index_including create_view create_index_spgist create_index
ok 74        + create_index                              295 ms
ok 75        + create_index_spgist                       165 ms
ok 76        + create_view                               132 ms
ok 77        + index_including                            75 ms
ok 78        + index_including_gist                       73 ms
# parallel group (16 tests):  create_cast errors create_aggregate roleattributes hash_func drop_if_exists select typed_table create_function_sql create_am infinite_recurse vacuum updatable_views constraints triggers inherit
ok 79        + create_aggregate                           14 ms
ok 80        + create_function_sql                        57 ms
ok 81        + create_cast                                10 ms
ok 82        + constraints                               273 ms
ok 83        + triggers                                  329 ms
ok 84        + select                                     28 ms
ok 85        + inherit                                   376 ms
ok 86        + typed_table                                42 ms
ok 87        + vacuum                                    166 ms
ok 88        + drop_if_exists                             26 ms
ok 89        + updatable_views                           217 ms
ok 90        + roleattributes                             17 ms
ok 91        + create_am                                  61 ms
ok 92        + hash_func                                  19 ms
ok 93        + errors                                     11 ms
ok 94        + infinite_recurse                          107 ms
ok 95        - sanity_check                               41 ms
# parallel group (20 tests):  delete select_having select_distinct_on case select_implicit random select_into prepared_xacts namespace portals union transactions select_distinct arrays subselect update aggregates hash_index join btree_index
ok 96        + select_into                                43 ms
ok 97        + select_distinct                            83 ms
ok 98        + select_distinct_on                         22 ms
ok 99        + select_implicit                            34 ms
ok 100       + select_having                              19 ms
ok 101       + subselect                                 124 ms
ok 102       + union                                      74 ms
ok 103       + case                                       32 ms
ok 104       + join                                      284 ms
ok 105       + aggregates                                228 ms
ok 106       + transactions                               74 ms
ok 107       + random                                     40 ms
ok 108       + portals                                    65 ms
ok 109       + arrays                                    117 ms
ok 110       + btree_index                               467 ms
ok 111       + hash_index                                240 ms
ok 112       + update                                    167 ms
ok 113       + delete                                     15 ms
ok 114       + namespace                                  44 ms
ok 115       + prepared_xacts                             41 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets identity matview rowsecurity spgist generated_stored gist gin join_hash brin privileges
ok 116       + brin                                      570 ms
ok 117       + gin                                       338 ms
ok 118       + gist                                      327 ms
ok 119       + spgist                                    288 ms
ok 120       + privileges                                709 ms
ok 121       + init_privs                                 10 ms
ok 122       + security_label                             26 ms
ok 123       + collate                                    97 ms
ok 124       + matview                                   224 ms
ok 125       + lock                                       61 ms
ok 126       + replica_identity                          126 ms
ok 127       + rowsecurity                               276 ms
ok 128       + object_address                             77 ms
ok 129       + tablesample                                49 ms
ok 130       + groupingsets                              146 ms
ok 131       + drop_operator                              15 ms
ok 132       + password                                   80 ms
ok 133       + identity                                  206 ms
ok 134       + generated_stored                          298 ms
ok 135       + join_hash                                 561 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 46 ms
ok 137       + brin_multi                                 93 ms
# parallel group (20 tests):  async dbsize nls tidscan alter_operator collate.utf8 tsrf tid sysviews create_role misc_functions tidrangescan alter_generic misc incremental_sort create_table_like merge collate.icu.utf8 generated_virtual without_overlaps
ok 138       + create_table_like                         138 ms
ok 139       + alter_generic                              63 ms
ok 140       + alter_operator                             22 ms
ok 141       + misc                                       77 ms
ok 142       + async                                       8 ms
ok 143       + dbsize                                      8 ms
ok 144       + merge                                     146 ms
ok 145       + misc_functions                             56 ms
ok 146       + nls                                         9 ms
ok 147       + sysviews                                   41 ms
ok 148       + tsrf                                       29 ms
ok 149       + tid                                        32 ms
ok 150       + tidscan                                    19 ms
ok 151       + tidrangescan                               60 ms
ok 152       + collate.utf8                               21 ms
ok 153       + collate.icu.utf8                          176 ms
ok 154       + incremental_sort                           77 ms
ok 155       + create_role                                41 ms
ok 156       + without_overlaps                          282 ms
ok 157       + generated_virtual                         211 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     154 ms
ok 159       + psql                                      257 ms
ok 160       + psql_crosstab                              12 ms
ok 161       + psql_pipeline                              18 ms
ok 162       + amutils                                     9 ms
ok 163       + stats_ext                                 613 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           329 ms
ok 167       - write_parallel                             39 ms
ok 168       - vacuum_parallel                            44 ms
ok 169       - maintain_every                             12 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               210 ms
ok 171       + subscription                               33 ms
# parallel group (18 tests):  portals_p2 advisory_lock tsdicts xmlmap combocid equivclass functional_deps guc dependency select_views stats_rewrite window tsearch bitmapops cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               67 ms
ok 173       + portals_p2                                 15 ms
ok 174       + foreign_key                               590 ms
ok 175       + cluster                                   159 ms
ok 176       + dependency                                 60 ms
ok 177       + guc                                        57 ms
ok 178       + bitmapops                                 145 ms
ok 179       + combocid                                   36 ms
ok 180       + tsearch                                   139 ms
ok 181       + tsdicts                                    30 ms
ok 182       + foreign_data                              298 ms
ok 183       + window                                    117 ms
ok 184       + xmlmap                                     34 ms
ok 185       + functional_deps                            48 ms
ok 186       + advisory_lock                              19 ms
ok 187       + indirect_toast                            176 ms
ok 188       + equivclass                                 41 ms
ok 189       + stats_rewrite                              73 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable json sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                       37 ms
ok 191       + jsonb                                     121 ms
ok 192       + json_encoding                               7 ms
ok 193       + jsonpath                                   15 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             50 ms
ok 196       + sqljson                                    31 ms
ok 197       + sqljson_queryfuncs                         46 ms
ok 198       + sqljson_jsontable                          32 ms
# parallel group (18 tests):  prepare limit xml conversion plancache returning polymorphism rowtypes copy2 sequence rangefuncs largeobject temp with truncate domain plpgsql alter_table
ok 199       + plancache                                  69 ms
ok 200       + limit                                      47 ms
ok 201       + plpgsql                                   230 ms
ok 202       + copy2                                     108 ms
ok 203       + temp                                      128 ms
ok 204       + domain                                    159 ms
ok 205       + rangefuncs                                122 ms
ok 206       + prepare                                    23 ms
ok 207       + conversion                                 66 ms
ok 208       + truncate                                  134 ms
ok 209       + alter_table                               730 ms
ok 210       + sequence                                  112 ms
ok 211       + polymorphism                               93 ms
ok 212       + rowtypes                                   95 ms
ok 213       + returning                                  79 ms
ok 214       + largeobject                               123 ms
ok 215       + with                                      131 ms
ok 216       + xml                                        48 ms
# parallel group (18 tests):  compression_lz4 numa hash_part reloptions predicate explain partition_info compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_join partition_prune tuplesort indexing stats
ok 217       + partition_merge                           231 ms
ok 218       + partition_split                           275 ms
ok 219       + partition_join                            360 ms
ok 220       + partition_prune                           374 ms
ok 221       + reloptions                                 42 ms
ok 222       + hash_part                                  18 ms
ok 223       + indexing                                  446 ms
ok 224       + partition_aggregate                       322 ms
ok 225       + partition_info                             51 ms
ok 226       + tuplesort                                 418 ms
ok 227       + explain                                    47 ms
ok 228       + compression                                72 ms
ok 229       + compression_lz4                             6 ms
ok 230       + memoize                                    99 ms
ok 231       + stats                                     495 ms
ok 232       + predicate                                  42 ms
ok 233       + numa                                        7 ms
ok 234       + eager_aggregate                           163 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   86 ms
ok 236       + event_trigger                             101 ms
ok 237       - event_trigger_login                        19 ms
ok 238       - fast_default                               47 ms
ok 239       - tablespace                                157 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
