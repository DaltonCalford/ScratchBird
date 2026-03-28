Last updated: 2026-03-28

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
# using temp instance on port 58928 with PID 2837618
ok 1         - test_setup                                105 ms
# parallel group (20 tests):  oid varchar text char name int2 int4 boolean txid int8 money pg_lsn float4 regproc float8 bit enum uuid numeric rangetypes
ok 2         + boolean                                    15 ms
ok 3         + char                                       10 ms
ok 4         + name                                       10 ms
ok 5         + varchar                                     7 ms
ok 6         + text                                        7 ms
ok 7         + int2                                       10 ms
ok 8         + int4                                       11 ms
ok 9         + int8                                       17 ms
ok 10        + oid                                         7 ms
ok 11        + float4                                     17 ms
ok 12        + float8                                     25 ms
ok 13        + bit                                        27 ms
ok 14        + numeric                                   101 ms
ok 15        + txid                                       14 ms
ok 16        + uuid                                       45 ms
ok 17        + enum                                       29 ms
ok 18        + money                                      16 ms
ok 19        + rangetypes                                152 ms
ok 20        + pg_lsn                                     16 ms
ok 21        + regproc                                    19 ms
# parallel group (20 tests):  md5 lseg line point path circle time numerology macaddr timetz macaddr8 inet date interval strings box polygon multirangetypes timestamp timestamptz
ok 22        + strings                                    60 ms
ok 23        + md5                                         6 ms
ok 24        + numerology                                 18 ms
ok 25        + point                                      11 ms
ok 26        + lseg                                        6 ms
ok 27        + line                                       10 ms
ok 28        + box                                        79 ms
ok 29        + path                                       11 ms
ok 30        + polygon                                    89 ms
ok 31        + circle                                     12 ms
ok 32        + date                                       31 ms
ok 33        + time                                       16 ms
ok 34        + timetz                                     18 ms
ok 35        + timestamp                                 335 ms
ok 36        + timestamptz                               339 ms
ok 37        + interval                                   34 ms
ok 38        + inet                                       23 ms
ok 39        + macaddr                                    16 ms
ok 40        + macaddr8                                   17 ms
ok 41        + multirangetypes                            91 ms
# parallel group (19 tests):  comments euc_kr misc_sanity unicode pg_ndistinct pg_dependencies tstypes oid8 expressions xid encoding horology mvcc geometry type_sanity stats_import opr_sanity database regex
ok 42        + geometry                                   43 ms
ok 43        + horology                                   37 ms
ok 44        + tstypes                                    21 ms
ok 45        + regex                                     187 ms
ok 46        + type_sanity                                44 ms
ok 47        + opr_sanity                                119 ms
ok 48        + misc_sanity                                 9 ms
ok 49        + comments                                    7 ms
ok 50        + expressions                                26 ms
ok 51        + unicode                                    15 ms
ok 52        + xid                                        28 ms
ok 53        + mvcc                                       38 ms
ok 54        + database                                  137 ms
ok 55        + stats_import                               96 ms
ok 56        + pg_ndistinct                               15 ms
ok 57        + pg_dependencies                            17 ms
ok 58        + oid8                                       20 ms
ok 59        + encoding                                   31 ms
ok 60        + euc_kr                                      7 ms
# parallel group (6 tests):  copyencoding copyselect copydml insert_conflict copy insert
ok 61        + copy                                       51 ms
ok 62        + copyselect                                 14 ms
ok 63        + copydml                                    15 ms
ok 64        + copyencoding                                8 ms
ok 65        + insert                                    112 ms
ok 66        + insert_conflict                            39 ms
# parallel group (7 tests):  create_function_c create_operator create_type create_misc create_schema create_procedure create_table
ok 67        + create_function_c                           6 ms
ok 68        + create_misc                                17 ms
ok 69        + create_operator                             8 ms
ok 70        + create_procedure                           28 ms
ok 71        + create_table                              122 ms
ok 72        + create_type                                14 ms
ok 73        + create_schema                              19 ms
# parallel group (5 tests):  index_including index_including_gist create_view create_index_spgist create_index
ok 74        + create_index                              294 ms
ok 75        + create_index_spgist                       173 ms
ok 76        + create_view                               128 ms
ok 77        + index_including                            65 ms
ok 78        + index_including_gist                       95 ms
# parallel group (16 tests):  create_cast roleattributes errors hash_func create_aggregate drop_if_exists select typed_table create_function_sql create_am infinite_recurse vacuum constraints updatable_views inherit triggers
ok 79        + create_aggregate                           24 ms
ok 80        + create_function_sql                        53 ms
ok 81        + create_cast                                10 ms
ok 82        + constraints                               233 ms
ok 83        + triggers                                  346 ms
ok 84        + select                                     25 ms
ok 85        + inherit                                   332 ms
ok 86        + typed_table                                45 ms
ok 87        + vacuum                                    163 ms
ok 88        + drop_if_exists                             22 ms
ok 89        + updatable_views                           235 ms
ok 90        + roleattributes                             13 ms
ok 91        + create_am                                  63 ms
ok 92        + hash_func                                  16 ms
ok 93        + errors                                     14 ms
ok 94        + infinite_recurse                          100 ms
ok 95        - sanity_check                               50 ms
# parallel group (20 tests):  select_distinct_on select_having delete select_implicit case random prepared_xacts select_into namespace portals select_distinct transactions union update arrays subselect hash_index aggregates join btree_index
ok 96        + select_into                                42 ms
ok 97        + select_distinct                            69 ms
ok 98        + select_distinct_on                         19 ms
ok 99        + select_implicit                            22 ms
ok 100       + select_having                              19 ms
ok 101       + subselect                                 138 ms
ok 102       + union                                      79 ms
ok 103       + case                                       30 ms
ok 104       + join                                      288 ms
ok 105       + aggregates                                251 ms
ok 106       + transactions                               72 ms
ok 107       + random                                     33 ms
ok 108       + portals                                    61 ms
ok 109       + arrays                                    131 ms
ok 110       + btree_index                               501 ms
ok 111       + hash_index                                244 ms
ok 112       + update                                    125 ms
ok 113       + delete                                     19 ms
ok 114       + namespace                                  40 ms
ok 115       + prepared_xacts                             36 ms
# parallel group (20 tests):  init_privs drop_operator security_label tablesample lock object_address password collate replica_identity groupingsets matview identity rowsecurity spgist generated_stored gin gist join_hash brin privileges
ok 116       + brin                                      567 ms
ok 117       + gin                                       355 ms
ok 118       + gist                                      367 ms
ok 119       + spgist                                    315 ms
ok 120       + privileges                                715 ms
ok 121       + init_privs                                  9 ms
ok 122       + security_label                             22 ms
ok 123       + collate                                   101 ms
ok 124       + matview                                   203 ms
ok 125       + lock                                       57 ms
ok 126       + replica_identity                          114 ms
ok 127       + rowsecurity                               267 ms
ok 128       + object_address                             60 ms
ok 129       + tablesample                                38 ms
ok 130       + groupingsets                              167 ms
ok 131       + drop_operator                              13 ms
ok 132       + password                                   78 ms
ok 133       + identity                                  222 ms
ok 134       + generated_stored                          321 ms
ok 135       + join_hash                                 563 ms
# parallel group (2 tests):  brin_bloom brin_multi
ok 136       + brin_bloom                                 46 ms
ok 137       + brin_multi                                105 ms
# parallel group (20 tests):  async nls dbsize alter_operator tid tidscan collate.utf8 sysviews tsrf create_role misc_functions alter_generic misc tidrangescan incremental_sort merge create_table_like collate.icu.utf8 generated_virtual without_overlaps
ok 138       + create_table_like                         175 ms
ok 139       + alter_generic                              56 ms
ok 140       + alter_operator                             23 ms
ok 141       + misc                                       57 ms
ok 142       + async                                       7 ms
ok 143       + dbsize                                      8 ms
ok 144       + merge                                     127 ms
ok 145       + misc_functions                             51 ms
ok 146       + nls                                         6 ms
ok 147       + sysviews                                   35 ms
ok 148       + tsrf                                       38 ms
ok 149       + tid                                        22 ms
ok 150       + tidscan                                    26 ms
ok 151       + tidrangescan                               64 ms
ok 152       + collate.utf8                               25 ms
ok 153       + collate.icu.utf8                          182 ms
ok 154       + incremental_sort                           77 ms
ok 155       + create_role                                38 ms
ok 156       + without_overlaps                          235 ms
ok 157       + generated_virtual                         221 ms
# parallel group (8 tests):  collate.windows.win1252 collate.linux.utf8 amutils psql_pipeline psql_crosstab rules psql stats_ext
ok 158       + rules                                     114 ms
ok 159       + psql                                      269 ms
ok 160       + psql_crosstab                              13 ms
ok 161       + psql_pipeline                              10 ms
ok 162       + amutils                                    10 ms
ok 163       + stats_ext                                 576 ms
ok 164       + collate.linux.utf8                          5 ms
ok 165       + collate.windows.win1252                     5 ms
ok 166       - select_parallel                           331 ms
ok 167       - write_parallel                             41 ms
ok 168       - vacuum_parallel                            46 ms
ok 169       - maintain_every                             12 ms
# parallel group (2 tests):  subscription publication
ok 170       + publication                               172 ms
ok 171       + subscription                               32 ms
# parallel group (18 tests):  portals_p2 combocid advisory_lock xmlmap tsdicts functional_deps equivclass dependency guc select_views stats_rewrite window tsearch cluster bitmapops indirect_toast foreign_data foreign_key
ok 172       + select_views                               65 ms
ok 173       + portals_p2                                 11 ms
ok 174       + foreign_key                               512 ms
ok 175       + cluster                                   138 ms
ok 176       + dependency                                 47 ms
ok 177       + guc                                        47 ms
ok 178       + bitmapops                                 144 ms
ok 179       + combocid                                   22 ms
ok 180       + tsearch                                   136 ms
ok 181       + tsdicts                                    36 ms
ok 182       + foreign_data                              264 ms
ok 183       + window                                    111 ms
ok 184       + xmlmap                                     24 ms
ok 185       + functional_deps                            36 ms
ok 186       + advisory_lock                              23 ms
ok 187       + indirect_toast                            159 ms
ok 188       + equivclass                                 43 ms
ok 189       + stats_rewrite                              68 ms
# parallel group (9 tests):  jsonpath_encoding json_encoding jsonpath sqljson_jsontable sqljson json sqljson_queryfuncs jsonb_jsonpath jsonb
ok 190       + json                                       46 ms
ok 191       + jsonb                                     106 ms
ok 192       + json_encoding                               8 ms
ok 193       + jsonpath                                   15 ms
ok 194       + jsonpath_encoding                           6 ms
ok 195       + jsonb_jsonpath                             59 ms
ok 196       + sqljson                                    34 ms
ok 197       + sqljson_queryfuncs                         53 ms
ok 198       + sqljson_jsontable                          32 ms
# parallel group (18 tests):  prepare plancache limit conversion xml returning polymorphism copy2 rowtypes sequence rangefuncs with truncate temp largeobject domain plpgsql alter_table
ok 199       + plancache                                  46 ms
ok 200       + limit                                      54 ms
ok 201       + plpgsql                                   255 ms
ok 202       + copy2                                      98 ms
ok 203       + temp                                      133 ms
ok 204       + domain                                    160 ms
ok 205       + rangefuncs                                114 ms
ok 206       + prepare                                    14 ms
ok 207       + conversion                                 58 ms
ok 208       + truncate                                  131 ms
ok 209       + alter_table                               509 ms
ok 210       + sequence                                  107 ms
ok 211       + polymorphism                               95 ms
ok 212       + rowtypes                                  101 ms
ok 213       + returning                                  70 ms
ok 214       + largeobject                               136 ms
ok 215       + with                                      121 ms
ok 216       + xml                                        59 ms
# parallel group (18 tests):  compression_lz4 numa hash_part reloptions predicate partition_info explain compression memoize eager_aggregate partition_merge partition_split partition_aggregate partition_join partition_prune tuplesort indexing stats
ok 217       + partition_merge                           240 ms
ok 218       + partition_split                           264 ms
ok 219       + partition_join                            338 ms
ok 220       + partition_prune                           379 ms
ok 221       + reloptions                                 28 ms
ok 222       + hash_part                                  21 ms
ok 223       + indexing                                  438 ms
ok 224       + partition_aggregate                       301 ms
ok 225       + partition_info                             46 ms
ok 226       + tuplesort                                 420 ms
ok 227       + explain                                    52 ms
ok 228       + compression                                70 ms
ok 229       + compression_lz4                             7 ms
ok 230       + memoize                                    90 ms
ok 231       + stats                                     493 ms
ok 232       + predicate                                  39 ms
ok 233       + numa                                        8 ms
ok 234       + eager_aggregate                           132 ms
# parallel group (2 tests):  oidjoins event_trigger
ok 235       + oidjoins                                   76 ms
ok 236       + event_trigger                              88 ms
ok 237       - event_trigger_login                        17 ms
ok 238       - fast_default                               65 ms
ok 239       - tablespace                                191 ms
1..239
# All 239 tests passed.
make: Leaving directory '<outside-tree-path>'
```

## Notes
- This smoke verifies in-tree snapshot presence and optional full make check when PG_UPSTREAM_BUILD_DIR is set.
- Full required check/installcheck/check-world suites remain in PG-EMU-041/042 closure work.
