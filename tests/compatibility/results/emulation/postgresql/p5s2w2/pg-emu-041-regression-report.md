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
ake -j1  checkprep >>'<outside-tree-path>'/tmp_install/log/install.log 2>&1
PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  initdb --auth trust --no-sync --no-instructions --lc-messages=C --no-clean '<outside-tree-path>'/tmp_install/initdb-template >>'<outside-tree-path>'/tmp_install/log/initdb-template.log 2>&1
echo "# +++ regress check in src/test/regress +++" && PATH="<outside-tree-path>" LD_LIBRARY_PATH="<outside-tree-path>" INITDB_TEMPLATE='<outside-tree-path>'/tmp_install/initdb-template  ../../../src/test/regress/pg_regress --temp-instance=./tmp_check --inputdir=<outside-tree-path> --bindir=     --dlpath=. --max-concurrent-tests=20  --schedule=<outside-tree-path>  
# +++ regress check in src/test/regress +++
# initializing database system by copying initdb template
# using temp instance on port 58928 with PID 897430
ok 1         - test_setup                                116 ms
# parallel group (20 tests):  char text int4 int2 name txid money varchar float4 oid pg_lsn boolean int8 regproc float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    20 ms
ok 3         + char                                        8 ms
ok 4         + name                                       12 ms
ok 5         + varchar                                    16 ms
ok 6         + text                                        9 ms
ok 7         + int2                                       11 ms
ok 8         + int4                                       10 ms
ok 9         + int8                                       21 ms
ok 10        + oid                                        15 ms
ok 11        + float4                                     15 ms
ok 12        + float8                                     25 ms
ok 13        + bit                                        26 ms
ok 14        + numeric                                   119 ms
ok 15        + txid                                       13 ms
ok 16        + uuid                                       42 ms
ok 17        + enum                                       29 ms
ok 18        + money                                      14 ms
ok 19        + rangetypes                                129 ms
ok 20        + pg_lsn                                     16 ms
ok 21        + regproc                                    20 ms
# parallel group (20 tests):  md5 lseg circle path line time macaddr point timetz numerology macaddr8 date inet interval strings polygon box multirangetypes timestamp timestamptz
ok 22        + strings                                    58 ms
ok 23        + md5                                         6 ms
ok 24        + numerology                                 17 ms
ok 25        + point                                      14 ms
ok 26        + lseg                                        8 ms
ok 27        + line                                       12 ms
ok 28        + box                                        91 ms
ok 29        + path                                       11 ms
ok 30        + polygon                                    73 ms
ok 31        + circle                                      8 ms
ok 32        + date                                       21 ms
ok 33        + time                                       13 ms
ok 34        + timetz                                     14 ms
ok 35        + timestamp                                 324 ms
ok 36        + timestamptz                               327 ms
ok 37        + interval                                   31 ms
ok 38        + inet                                       30 ms
ok 39        + macaddr                                    12 ms
ok 40        + macaddr8                                   17 ms
ok 41        + multirangetypes                            95 ms
# parallel group (19 tests):  euc_kr unicode pg_ndistinct misc_sanity comments pg_dependencies tstypes oid8 expressions encoding xid horology geometry mvcc type_sanity stats_import opr_sanity database regex
ok 42        + geometry                                   36 ms
ok 43        + horology                                   29 ms
ok 44        + tstypes                                    21 ms
ok 45        + regex                                     186 ms
ok 46        + type_sanity                                42 ms
ok 47        + opr_sanity                                123 ms
ok 48        + misc_sanity                                11 ms
ok 49        + comments                                   11 ms
ok 50        + expressions                                23 ms
ok 51        + unicode                                     8 ms
ok 52        + xid                                        26 ms
ok 53        + mvcc                                       38 ms
ok 54        + database                                  130 ms
ok 55        + stats_import                               85 ms
ok 56        + pg_ndistinct                               10 ms
ok 57        + pg_dependencies                            15 ms
ok 58        + oid8                                       20 ms
ok 59        + encoding                                   25 ms
ok 60        + euc_kr                                      6 ms
# parallel group (6 tests):  copyencoding copyselect copydml copy insert_conflict insert
ok 61        + copy                                       33 ms
ok 62        + copyselect                                 12 ms
ok 63        + copydml                                    13 ms
ok 64        + copyencoding                                6 ms
ok 65        + insert                                    108 ms
ok 66        + insert_conflict                            62 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_schema create_misc create_procedure create_table
ok 67        + create_function_c                           6 ms
ok 68        + create_misc                                19 ms
ok 69        + create_operator                            10 ms
ok 70        + create_procedure                           27 ms
ok 71        + create_table                              122 ms
ok 72        + create_type                                14 ms
ok 73        + create_schema                              16 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              280 ms
ok 75        + create_index_spgist                       177 ms
ok 76        + create_view                               128 ms
ok 77        + index_including                            57 ms
ok 78        + index_including_gist                      118 ms
# parallel group (16 tests):  create_cast hash_func errors create_aggregate roleattributes select drop_if_exists typed_table create_function_sql create_am infinite_recurse vacuum constraints updatable_views triggers inherit
ok 79        + create_aggregate                           17 ms
ok 80        + create_function_sql                        56 ms
ok 81        + create_cast                                 9 ms
ok 82        + constraints                               227 ms
ok 83        + triggers                                  340 ms
ok 84        + select                                     27 ms
ok 85        + inherit                                   382 ms
ok 86        + typed_table                                53 ms
ok 87        + vacuum                                    160 ms
ok 88        + drop_if_exists                             28 ms
ok 89        + updatable_views                           245 ms
ok 90        + roleattributes                             19 ms
ok 91        + create_am                                  62 ms
ok 92        + hash_func                                  15 ms
ok 93        + errors                                     15 ms
ok 94        + infinite_recurse                           88 ms
ok 95        - sanity_check                               44 ms
# parallel group (20 tests):  select_having select_implicit delete case select_distinct_on random namespace prepared_xacts select_into portals transactions select_distinct union arrays subselect update hash_index join aggregates btree_index
ok 96        + select_into                                51 ms
ok 97        + select_distinct                            70 ms
ok 98        + select_distinct_on                         23 ms
ok 99        + select_implicit                            19 ms
ok 100       + select_having                              14 ms
ok 101       + subselect                                 130 ms
ok 102       + union                                      75 ms
ok 103       + case                                       21 ms
ok 104       + join                                      257 ms
ok 105       + aggregates                                265 ms
ok 106       + transactions                               66 ms
ok 107       + random                                     30 ms
ok 108       + portals                                    60 ms
ok 109       + arrays                                    121 ms
ok 110       + btree_index                               477 ms
ok 111       + hash_index                                229 ms
ok 112       + update                                    170 ms
ok 113       + delete                                     18 ms
ok 114       + namespace                                  40 ms
ok 115       + prepared_xacts                             44 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets matview identity rowsecurity generated_stored spgist gist gin join_hash brin privileges
ok 116       + brin                                      561 ms
ok 117       + gin                                       363 ms
ok 118       + gist                                      353 ms
ok 119       + spgist                                    312 ms
ok 120       + privileges                                693 ms
ok 121       + init_privs                                 11 ms
ok 122       + security_label                             34 ms
ok 123       + collate                                    91 ms
ok 124       + matview                                   204 ms
ok 125       + lock                                       58 ms
ok 126       + replica_identity                          123 ms
ok 127       + rowsecurity                               280 ms
ok 128       + object_address                             68 ms
ok 129       + tablesample                                38 ms
ok 130       + groupingsets                              159 ms
ok 131       + drop_operator                              14 ms
ok 132       + password                                   79 ms
ok 133       + identity                                  218 ms
ok 134       + generated_stored                          284 ms
ok 135       + join_hash                                 552 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 29 ms
ok 137       + brin_multi                                 83 ms
# parallel group (20 tests):  async nls dbsize collate.utf8 tidscan tsrf alter_operator tid create_role sysviews misc_functions alter_generic tidrangescan misc incremental_sort merge create_table_like collate.icu.utf8 generated_virtual without_overlaps
ok 138       + create_table_like                         180 ms
ok 139       + alter_generic                              62 ms
ok 140       + alter_operator                             26 ms
ok 141       + misc                                       73 ms
ok 142       + async                                       7 ms
ok 143       + dbsize                                      8 ms
ok 144       + merge                                     149 ms
ok 145       + misc_functions                             51 ms
ok 146       + nls                                         7 ms
ok 147       + sysviews                                   44 ms
ok 148       + tsrf                                       23 ms
ok 149       + tid                                        31 ms
ok 150       + tidscan                                    22 ms
ok 151       + tidrangescan                               65 ms
ok 152       + collate.utf8                               21 ms
ok 153       + collate.icu.utf8                          189 ms
ok 154       + incremental_sort                           88 ms
ok 155       + create_role                                39 ms
ok 156       + without_overlaps                          275 ms
ok 157       + generated_virtual                         211 ms
# parallel group (8 tests):  collate.linux.utf8 collate.windows.win1252 amutils psql_crosstab psql_pipeline rules psql stats_ext
ok 158       + rules                                     119 ms
ok 159       + psql                                      187 ms
ok 160       + psql_crosstab                              10 ms
ok 161       + psql_pipeline                              15 ms
ok 162       + amutils                                     9 ms
ok 163       + stats_ext                                 648 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           334 ms
ok 167       - write_parallel                             39 ms
ok 168       - vacuum_parallel                            49 ms
ok 169       - maintain_every                             11 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               132 ms
ok 171       + subscription                               30 ms
# parallel group (18 tests):  portals_p2 combocid advisory_lock xmlmap tsdicts functional_deps equivclass guc dependency select_views stats_rewrite window bitmapops tsearch cluster indirect_toast foreign_data foreign_key
ok 172       + select_views                               57 ms
ok 173       + portals_p2                                 13 ms
ok 174       + foreign_key                               433 ms
ok 175       + cluster                                   139 ms
ok 176       + dependency                                 47 ms
ok 177       + guc                                        42 ms
ok 178       + bitmapops                                 127 ms
ok 179       + combocid                                   20 ms
ok 180       + tsearch                                   133 ms
ok 181       + tsdicts                                    27 ms
ok 182       + foreign_data                              247 ms
ok 183       + window                                    104 ms
ok 184       + xmlmap                                     23 ms
ok 185       + functional_deps                            34 ms
ok 186       + advisory_lock                              20 ms
ok 187       + indirect_toast                            156 ms
ok 188       + equivclass                                 36 ms
ok 189       + stats_rewrite                              59 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson sqljson_jsontable jsonb_jsonpath json sqljson_queryfuncs jsonb
ok 190       + json                                       40 ms
ok 191       + jsonb                                      97 ms
ok 192       + json_encoding                               7 ms
ok 193       + jsonpath                                   16 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             32 ms
ok 196       + sqljson                                    23 ms
ok 197       + sqljson_queryfuncs                         40 ms
ok 198       + sqljson_jsontable                          30 ms
# parallel group (18 tests):  prepare xml limit conversion plancache returning polymorphism rowtypes copy2 sequence truncate rangefuncs with largeobject temp domain plpgsql alter_table
ok 199       + plancache                                  60 ms
ok 200       + limit                                      51 ms
ok 201       + plpgsql                                   220 ms
ok 202       + copy2                                      96 ms
ok 203       + temp                                      132 ms
ok 204       + domain                                    137 ms
ok 205       + rangefuncs                                116 ms
ok 206       + prepare                                    21 ms
ok 207       + conversion                                 54 ms
ok 208       + truncate                                  113 ms
ok 209       + alter_table                               530 ms
ok 210       + sequence                                   99 ms
ok 211       + polymorphism                               84 ms
ok 212       + rowtypes                                   94 ms
ok 213       + returning                                  82 ms
ok 214       + largeobject                               121 ms
ok 215       + with                                      119 ms
ok 216       + xml                                        42 ms
# parallel group (18 tests):  numa compression_lz4 hash_part reloptions predicate explain partition_info memoize compression eager_aggregate partition_merge partition_split partition_join partition_prune partition_aggregate tuplesort indexing stats
ok 217       + partition_merge                           229 ms
ok 218       + partition_split                           255 ms
ok 219       + partition_join                            307 ms
ok 220       + partition_prune                           320 ms
ok 221       + reloptions                                 32 ms
ok 222       + hash_part                                  21 ms
ok 223       + indexing                                  419 ms
ok 224       + partition_aggregate                       332 ms
ok 225       + partition_info                             49 ms
ok 226       + tuplesort                                 392 ms
ok 227       + explain                                    42 ms
ok 228       + compression                                78 ms
ok 229       + compression_lz4                             9 ms
ok 230       + memoize                                    76 ms
ok 231       + stats                                     494 ms
ok 232       + predicate                                  41 ms
ok 233       + numa                                        5 ms
ok 234       + eager_aggregate                           133 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   79 ms
ok 236       + event_trigger                              92 ms
ok 237       - event_trigger_login                        17 ms
ok 238       - fast_default                               55 ms
ok 239       - tablespace                                158 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
