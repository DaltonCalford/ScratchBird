#coding:utf-8

"""
ID:          gtcs.dsql-domain-22
FBTEST:      functional.gtcs.dsql_domain_22
TITLE:       Verify result of ALTER DOMAIN with changing NOT NULL flag and CHECK constraints
  when a table exists with field based on this domain
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/DSQL_DOMAIN_22.script

    Comment in GTCS:
      test for error conditions when using the alter domain statement on domains
      that are already in use in table definitions,
      with domain defaults and check constraints.

    Test creates domain with DEFAULT value and CHECK constraint.
    Initially domain definition:
    1) allows insertion of NULLs;
    2) have DEFAULT value which meets CHECK requirements.

    Then we create table and insert one record with DEFAULT value (it must pass) and second record with NULL.

    After this we try to change domain definition by adding NOT NULL clause - and it must
    fail because of existing record with null. Finally, we replace CHECK constraint so that
    its new expression will opposite to previous one, and try again to insert record with DEFAULT value.
    It must fail because of new domain CHECK violation.

    This is performed separately for each datatype (smallint, int, ...).

    ::: NB-1 :::
    Test uses datatypes that did appear only in FB 4.0: INT128, DECFLOAT and
    TIME[STAMP] WITH TIME ZONE. For this reason only FB 4.0+ can be tested.

    ::: NB-2 :::
    Domain CHECK constraint *can* be changed so that existing data will not satisfy new expression.
    Only NOT NULL is verified against data that were inserted in the table.
NOTES:
    [19.04.2022] pzotov
    Manipulations with domain 'dom22_08' were changed: removed usage of EXP() to get value that is minimal
    distinguish from zero (used before: exp(-745.NNNNN)). Reason: result is hardware-dependent (Intel vs AMD).

    [23.06.2025] pzotov
    Fixed wrong value of charset that was used to connect: "utf-8". This caused crash of isql in recent 6.x.
    https://github.com/FirebirdSQL/firebird/commit/5b41342b169e0d79d63b8d2fdbc033061323fa1b
    Thanks to Vlad for solved problem.

    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 6.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *
from pathlib import Path

substitutions = [ 
                  ('After line.*', ''),
                  ('X_BLOB_20.*', ''),
                  ('X_BLOB_21.*', ''),
                  ('X_BLOB_22.*', ''),
                  ('DM_FVALID.*', ''),
                  ('DM_FDEFAULT.*', ''),
                  ('0.0000000000000000', '0.000000000000000'),
                  ('DATE_FROM_DOMAIN_DEFAULT .*', 'DATE_FROM_DOMAIN_DEFAULT'),
                  ('"TEST"."X_DATE", value .*', 'TEST.X_DATE'),
                ]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = python_act('db', substitutions = substitutions)

db = db_factory()

act = python_act('db', substitutions=substitutions)

test_expected_stderr = """
    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_SML of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_SML", value "0"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_SML", value "0"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_INT of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_INT", value "500"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_INT", value "500"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_DATE of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_DATE", value "2021-04-20"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_DATE", value "2021-04-20"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_CHAR of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_CHAR", value "Wisła              "

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_CHAR", value "Wisła              "

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_VCHR of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_VCHR", value "Norrström"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_VCHR", value "Norrström"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_NUM of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_NUM", value "-327.68"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_NUM", value "-327.68"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_DEC of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range

    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_DP of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_DP", value "0.0000000000000000"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_DP", value "0.0000000000000000"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_BIG of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_BIG", value "9223372036854775807"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_BIG", value "9223372036854775807"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_NC of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_NC", value "Y"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_NC", value "Y"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_BIN of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_BIN", value "Ÿ"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_BIN", value "Ÿ"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_VB of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_VB", value "Ÿ"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_VB", value "Ÿ"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_BOO of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_BOO", value "FALSE"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_BOO", value "FALSE"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_DF16 of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_DF16", value "-9.999999999999999E+384"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_DF16", value "-9.999999999999999E+384"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_DF34 of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 42000
    expression evaluation not supported
    -Argument for LOG10 must be positive

    Statement failed, SQLSTATE = 42000
    expression evaluation not supported
    -Argument for LOG10 must be positive

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_I128 of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_I128", value "170141183460469231731687303715884105727"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_I128", value "170141183460469231731687303715884105727"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_TMTZ of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_TMTZ", value "11:11:11.1110 Indian/Cocos"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_TMTZ", value "11:11:11.1110 Indian/Cocos"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_DTS of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_DTS", value "01-JAN-0001 0:00:01.0010"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_DTS", value "01-JAN-0001 0:00:01.0010"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_TSTZ of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_TSTZ", value "21-DEC-2013 11:11:11.1110 Indian/Cocos"

    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."X_TSTZ", value "21-DEC-2013 11:11:11.1110 Indian/Cocos"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_BLOB_20 of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 22018
    conversion error from string "BLOB"

    Statement failed, SQLSTATE = 22018
    conversion error from string "BLOB"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_BLOB_21 of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 22018
    conversion error from string "BLOB"

    Statement failed, SQLSTATE = 22018
    conversion error from string "BLOB"

    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X_BLOB_22 of table TEST NOT NULL because there are NULLs present

    Statement failed, SQLSTATE = 22018
    conversion error from string "BLOB"

    Statement failed, SQLSTATE = 22018
    conversion error from string "BLOB"

"""

test_expected_stdout = """
    DM_NAME                         DOM22_01
    DM_TYPE                         7
    DM_SUBTYPE                      0
    DM_FLEN                         2
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:1e5
    check (value >= 0 and value < 100)
    DM_FDEFAULT                     2:1e7
    default 0
    DM_NAME                         DOM22_02
    DM_TYPE                         8
    DM_SUBTYPE                      0
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:1e9
    check (value >= 500)
    DM_FDEFAULT                     2:1eb
    default 500
    DM_NAME                         DOM22_03
    DM_TYPE                         12
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:1ed
    check (value >= 'today')
    DM_FDEFAULT                     2:1ef
    default 'TODAY'
    DM_NAME                         DOM22_04
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         20
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      20
    DM_FNULL                        <null>
    DM_FVALID                       2:1f1
    check ( value in ('Волга','Дніпро','Wisła','Dunărea','Rhône') )
    DM_FDEFAULT                     2:1f3
    default 'Wisła'
    DM_NAME                         DOM22_05
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         25
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      25
    DM_FNULL                        <null>
    DM_FVALID                       2:1f5
    check ( value NOT in ('Волга','Дніпро','Wisła','Dunărea','Rhône') )
    DM_FDEFAULT                     2:1f7
    default 'Norrström'
    DM_NAME                         DOM22_06
    DM_TYPE                         7
    DM_SUBTYPE                      1
    DM_FLEN                         2
    DM_FSCALE                       -2
    DM_FPREC                        2
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:1f9
    check (value < 0)
    DM_FDEFAULT                     2:1fb
    default -327.68
    DM_NAME                         DOM22_07
    DM_TYPE                         8
    DM_SUBTYPE                      2
    DM_FLEN                         4
    DM_FSCALE                       -2
    DM_FPREC                        6
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:1fd
    check (value < 0)
    DM_FDEFAULT                     2:1ff
    default -999.99
    DM_NAME                         DOM22_08
    DM_TYPE                         27
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:201
    check (value is null or value = 0)
    DM_FDEFAULT                     2:203
    default 0
    DM_NAME                         DOM22_09
    DM_TYPE                         16
    DM_SUBTYPE                      0
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:205
    check (value > 0)
    DM_FDEFAULT                     2:207
    default 9223372036854775807
    DM_NAME                         DOM22_10
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         1
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        21
    DM_FCOLL                        0
    DM_FCHRLEN                      1
    DM_FNULL                        <null>
    DM_FVALID                       2:209
    check( value in ('Y', 'y') )
    DM_FDEFAULT                     2:20b
    default 'Y'
    DM_NAME                         DOM22_11
    DM_TYPE                         14
    DM_SUBTYPE                      1
    DM_FLEN                         2
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        1
    DM_FCOLL                        0
    DM_FCHRLEN                      2
    DM_FNULL                        <null>
    DM_FVALID                       2:20d
    check( value in ('Ÿ', 'ÿ') )
    DM_FDEFAULT                     2:20f
    default 'Ÿ'
    DM_NAME                         DOM22_12
    DM_TYPE                         37
    DM_SUBTYPE                      1
    DM_FLEN                         2
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        1
    DM_FCOLL                        0
    DM_FCHRLEN                      2
    DM_FNULL                        <null>
    DM_FVALID                       2:211
    check( value in ('Ÿ', 'ÿ') )
    DM_FDEFAULT                     2:213
    default 'Ÿ'
    DM_NAME                         DOM22_13
    DM_TYPE                         23
    DM_SUBTYPE                      <null>
    DM_FLEN                         1
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:215
    check ( value is not true )
    DM_FDEFAULT                     2:217
    default false
    DM_NAME                         DOM22_14
    DM_TYPE                         24
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        16
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:219
    check( log10(abs(value)) >= 384 )
    DM_FDEFAULT                     2:21b
    default -9.999999999999999E+384
    DM_NAME                         DOM22_15
    DM_TYPE                         25
    DM_SUBTYPE                      <null>
    DM_FLEN                         16
    DM_FSCALE                       0
    DM_FPREC                        34
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:21d
    check( log10(abs(value)) >= 6144 )
    DM_FDEFAULT                     2:21f
    default -9.999999999999999999999999999999999E6144
    DM_NAME                         DOM22_16
    DM_TYPE                         26
    DM_SUBTYPE                      0
    DM_FLEN                         16
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:221
    check( value in(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727) )
    DM_FDEFAULT                     2:223
    default 170141183460469231731687303715884105727
    DM_NAME                         DOM22_17
    DM_TYPE                         28
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:225
    check ( extract(hour from value) <=12 )
    DM_FDEFAULT                     2:227
    default '11:11:11.111 Indian/Cocos'
    DM_NAME                         DOM22_18
    DM_TYPE                         35
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:229
    check ( extract(minute from value) = 0 )
    DM_FDEFAULT                     2:22b
    default '01.01.0001 00:00:01.001'
    DM_NAME                         DOM22_19
    DM_TYPE                         29
    DM_SUBTYPE                      <null>
    DM_FLEN                         12
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:22d
    check ( extract(minute from value) <=30 )
    DM_FDEFAULT                     2:22f
    default '21.12.2013 11:11:11.111 Indian/Cocos'
    DM_NAME                         DOM22_20
    DM_TYPE                         261
    DM_SUBTYPE                      0
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:231
    check( value in ('Ÿ', 'ÿ') )
    DM_FDEFAULT                     2:233
    default 'Ÿ'
    DM_NAME                         DOM22_21
    DM_TYPE                         261
    DM_SUBTYPE                      1
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        4
    DM_FCOLL                        126
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:235
    check (value is null or value NOT in (select river from rivers))
    DM_FDEFAULT                     2:237
    default 'Ätran'
    DM_NAME                         DOM22_22
    DM_TYPE                         261
    DM_SUBTYPE                      0
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       2:239
    check (value > 0x01)
    DM_FDEFAULT                     2:23b
    default 0x10


    X_SML                           0
    X_INT                           500
    DATE_FROM_DOMAIN_DEFAULT        2021-04-20
    X_CHAR                          Wisła
    X_VCHR                          Norrström
    X_NUM                           -327.68
    X_DEC                           -999.99
    X_DP                            0.0000000000000000
    X_BIG                           9223372036854775807
    X_NC                            Y
    X_BIN                           C5B8
    X_VB                            C5B8
    X_BOO                           <false>
    X_DF16                          -9.999999999999999E+384
    X_DF34                          -9.999999999999999999999999999999999E+6144
    X_I128                                170141183460469231731687303715884105727
    X_TMTZ                          11:11:11.1110 Indian/Cocos
    X_DTS                           0001-01-01 00:00:01.0010
    X_TSTZ                          2013-12-21 11:11:11.1110 Indian/Cocos
    X_BLOB_20                       95:0
    Ÿ
    X_BLOB_21                       96:0
    Ätran
    X_BLOB_22                       97:0
    16
"""


test_script_1 = """
    set list on;
    set blob all;
    -- set names utf8;
    -- connect '%(dsn)s' user '%(user_name)s' password '%(user_password)s';


    create collation nm_coll for utf8 from unicode case insensitive accent insensitive;
    commit;

    create domain dom20u as varchar(20) character set utf8 collate nm_coll;
    commit;


    create table rivers(
      id int
      ,river dom20u
    );
    insert into rivers(id, river) values(1, 'Волга');
    insert into rivers(id, river) values(2, 'Дніпро');
    insert into rivers(id, river) values(3, 'Wisła');
    insert into rivers(id, river) values(4, 'Dunărea');
    insert into rivers(id, river) values(5, 'Rhône');
    commit;

    create domain dom22_01 as smallint         default 0           check (value >= 0 and value < 100);
    create domain dom22_02 as integer          default 500         check (value >= 500);

    -- NB: concrete value of date will be suppressed, see subst.:
    create domain dom22_03 as date             default 'TODAY'     check (value >= 'today');

    -- CHECK-expression of this domain will be changed to
    -- "check (value in (select river from rivers))" - see below:
    create domain dom22_04 as char(20)         default 'Wisła'     check ( value in ('Волга','Дніпро','Wisła','Dunărea','Rhône') );

    -- CHECK-expression of this domain will be changed to
    -- "check (value NOT in (select river from rivers))" - see below:
    create domain dom22_05 as varchar(25)      default 'Norrström' check ( value NOT in ('Волга','Дніпро','Wisła','Dunărea','Rhône') );

    create domain dom22_06 as numeric(2,2)     default -327.68      check (value < 0);
    create domain dom22_07 as decimal(6,2)     default -999.99      check (value < 0);

    -- exp(-745.1332192) is max. double precision value that will be NOT distinguish from zero:
    -- create domain dom22_08 as double precision default 0 check (value is null or value is not distinct from exp(-745.1332192));
    
    -- 19.04.2022
    create domain dom22_08 as double precision default 0 check (value is null or value = 0);

    -----------------------------

    -- Additional datataypes (they either not present in original test or did appear since FB 3.x):
    create domain dom22_09 as bigint         default 9223372036854775807 check (value > 0);
    create domain dom22_10 as nchar(1)       default 'Y' check( value in ('Y', 'y') ); -- alias for ISO8859_1
    create domain dom22_11 as binary(2)      default 'Ÿ' check( value in ('Ÿ', 'ÿ') ); -- this datatype is alias for char(N) character set octets
    create domain dom22_12 as varbinary(2)   default 'Ÿ' check( value in ('Ÿ', 'ÿ') );
    create domain dom22_13 as boolean        default false check ( value is not true );

    create domain dom22_14 as decfloat(16)   default -9.999999999999999E+384 check( log10(abs(value)) >= 384 );
    create domain dom22_15 as decfloat       default -9.999999999999999999999999999999999E6144 check( log10(abs(value)) >= 6144 );
    create domain dom22_16 as int128         default 170141183460469231731687303715884105727 check( value in(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727) );

    create domain dom22_17 as time with time zone      default '11:11:11.111 Indian/Cocos'            check ( extract(hour from value) <=12 );
    create domain dom22_18 as timestamp                default '01.01.0001 00:00:01.001'              check ( extract(minute from value) = 0 );
    create domain dom22_19 as timestamp with time zone default '21.12.2013 11:11:11.111 Indian/Cocos' check ( extract(minute from value) <=30 );

    create domain dom22_20 as blob                                  default 'Ÿ' check( value in ('Ÿ', 'ÿ') );
    create domain dom22_21 as blob sub_type text character set utf8 default 'Ätran' check (value is null or value NOT in (select river from rivers)) collate nm_coll;
    create domain dom22_22 as blob sub_type binary                  default 0x10 check (value > 0x01);

    create or alter view v_test as
    select
        ff.rdb$field_name as dm_name
        ,ff.rdb$field_type as dm_type
        ,ff.rdb$field_sub_type as dm_subtype
        ,ff.rdb$field_length as dm_flen
        ,ff.rdb$field_scale as dm_fscale
        ,ff.rdb$field_precision as dm_fprec
        ,ff.rdb$character_set_id as dm_fcset
        ,ff.rdb$collation_id as dm_fcoll
        ,ff.rdb$character_length dm_fchrlen
        ,ff.rdb$null_flag as dm_fnull
        ,ff.rdb$validation_source as dm_fvalid
        ,ff.rdb$default_source as dm_fdefault
    from rdb$fields ff
    where
        ff.rdb$system_flag is distinct from 1
        and ff.rdb$field_name starting with upper( 'dom22_' )
    order by dm_name
    ;
    commit;

    select * from v_test;
    -- ############################################################

    set bail off;

    ------------------------------------------------

    recreate table test(x_sml dom22_01); -- smallint         default 0           check (value >= 0 and value < 100);

    insert into test default values returning x_sml;
    insert into test values(null);
    commit;

    alter domain dom22_01 set not null; -- must fail with SQLSTATE = 22006 / -Cannot make field ... NOT NULL because there are NULLs
    alter domain dom22_01 drop constraint add constraint check(value < 0);
    commit;

    insert into test default values returning x_sml; -- must fail with SQLSTATE = 23000 / validation error ... value "0"
    update test set x_sml = default where x_sml is null; -- must fail with SQLSTATE = 23000 / validation error ... value "0"
    commit;

    ------------------------------------------------

    recreate table test(x_int dom22_02); -- integer          default 500         check (value >= 500);

    insert into test default values returning x_int;
    insert into test values(null);
    commit;

    alter domain dom22_02 set not null; -- must fail with SQLSTATE = 22006 / -Cannot make field ... NOT NULL because there are NULLs
    alter domain dom22_02 drop constraint add constraint check(value < 0);
    commit;

    insert into test default values returning x_int; -- must fail with SQLSTATE = 23000 / validation error ... value "500"
    update test set x_int = default where x_int is null; -- must fail with SQLSTATE = 23000 / validation error ... value "500"
    commit;

    ------------------------------------------------

    recreate table test(x_date dom22_03); -- date             default 'TODAY'     check (value >= 'today');

    -- must pass and issue current date, values will be suppressed, see 'susbtitutions':
    insert into test default values returning x_date as DATE_FROM_DOMAIN_DEFAULT;
    insert into test values(null);
    commit;

    alter domain dom22_03 set not null; -- must fail with SQLSTATE = 22006 / -Cannot make field ... NOT NULL because there are NULLs
    alter domain dom22_03 drop constraint add constraint check(value < 'today');
    commit;

    insert into test default values returning x_date; -- must fail with SQLSTATE = 23000 / validation error ... value <current_date>
    update test set x_date = default where x_date is null; -- must fail with SQLSTATE = 23000 / validation error ... value <current_date>
    commit;

    ------------------------------------------------

    recreate table test(x_char dom22_04); -- char(20)         default 'Wisła'     check (value in (select river from rivers));

    insert into test default values returning x_char;
    insert into test values(null);
    commit;

    alter domain dom22_04 set not null; -- must fail with SQLSTATE = 22006 / -Cannot make field ... NOT NULL because there are NULLs
    alter domain dom22_04 drop constraint add constraint check(value NOT in (select river from rivers));
    commit;

    insert into test default values returning x_char; -- must fail with SQLSTATE = 23000 / validation error ... value "Wisła              "
    update test set x_char = default where x_char is null; -- must fail with SQLSTATE = 23000 / validation error ... value "Wisła              "
    commit;

    ------------------------------------------------

    recreate table test(x_vchr dom22_05); -- varchar(25)      default 'Norrström' check (value NOT in (select river from rivers));

    insert into test default values returning x_vchr;
    insert into test values(null);
    commit;

    alter domain dom22_05 set not null; -- must fail with SQLSTATE = 22006 / -Cannot make field ... NOT NULL because there are NULLs
    alter domain dom22_05 drop constraint add constraint check(value in (select river from rivers));
    commit;

    insert into test default values returning x_vchr; -- must fail with SQLSTATE = 23000 / validation error ... value "Norrström"
    update test set x_vchr = default where x_vchr is null; -- must fail with SQLSTATE = 23000 / validation error ... value "Norrström"
    commit;

    ------------------------------------------------

    recreate table test(x_num dom22_06); -- numeric(2,2)     default -327.68      check (value < 0);

    insert into test default values returning x_num;
    insert into test values(null);
    commit;

    alter domain dom22_06 set not null; -- must fail with SQLSTATE = 22006 / -Cannot make field ... NOT NULL because there are NULLs
    alter domain dom22_06 drop constraint add constraint check(value >0);
    commit;

    insert into test default values returning x_num; -- must fail with SQLSTATE = 23000 / validation error ... value "-327.68"
    update test set x_num = default where x_num is null; -- must fail with SQLSTATE = 23000 / validation error ... value "-327.68"
    commit;

    ------------------------------------------------

    recreate table test(x_dec dom22_07); -- decimal(6,2)     default -999.99      check (value < 0);

    insert into test default values returning x_dec;
    insert into test values(null);
    commit;

    alter domain dom22_07 set not null;

    -- numeric(2,2) can hold values from the scope -327.68 to +327.68.
    -- cast of -999.99 to numeric(2,2) (being valid for decimal(2,2)) must fail with 'numeric value is out of range':
    alter domain dom22_07 drop constraint add constraint check(cast(value as numeric(2,2)) < 0 );
    commit;

    insert into test default values returning x_dec; -- must fail with SQLSTATE = 23000 / numeric value is out of range
    update test set x_dec = default where x_dec is null returning x_dec; -- must fail with SQLSTATE = 23000 / numeric value is out of range
    commit;

    ------------------------------------------------

    recreate table test(x_dp dom22_08); -- default 0 check (value is null or value = 0)

    insert into test default values returning x_dp;
    insert into test values(null);
    commit;

    alter domain dom22_08 set not null;
    -- alter domain dom22_08 drop constraint add constraint check(value is not distinct from exp(-745.1332191) ); -- minimal DP value that can be distinguished from zero
    alter domain dom22_08 drop constraint add constraint check(value = 1);
    commit;

    insert into test default values returning x_dp; -- must fail with SQLSTATE = 23000 / validation error ... value "0.0000000000000000"
    update test set x_dp = default where x_dp is null returning x_dp; -- must fail with SQLSTATE = 23000 / validation error ... value "0.0000000000000000"
    commit;

    ------------------------------------------------

    recreate table test(x_big dom22_09); -- default 9223372036854775807 check (value > 0);

    insert into test default values returning x_big;
    insert into test values(null);
    commit;

    alter domain dom22_09 set not null;
    alter domain dom22_09 drop constraint add constraint check(value between bin_shr(-9223372036854775808,63) and bin_shr(9223372036854775807,63)); -- -1...0
    commit;

    insert into test default values returning x_big; -- must fail with SQLSTATE = 23000 / validation error ... value "9223372036854775807"
    update test set x_big = default where x_big is null returning x_big; -- must fail with SQLSTATE = 23000 / validation error ... value "9223372036854775807"
    commit;

    ------------------------------------------------

    recreate table test(x_nc dom22_10); -- nchar(1) default 'Y' check( value in ('Y', 'y') );
    insert into test default values returning x_nc;
    insert into test values(null);
    commit;

    alter domain dom22_10 set not null;
    alter domain dom22_10 drop constraint add constraint check( value similar to 'U');
    commit;

    insert into test default values returning x_nc;
    update test set x_nc = default where x_nc is null returning x_nc;
    commit;

    ------------------------------------------------

    recreate table test(x_bin dom22_11); -- binary(2) default 'Ÿ' check( value in ('Ÿ', 'ÿ') )
    insert into test default values returning x_bin;
    insert into test values(null);
    commit;

    alter domain dom22_11 set not null;
    alter domain dom22_11 drop constraint add constraint check( value is not distinct from 'ł' or value is not distinct from 'ă' or value is not distinct from 'ô' );
    commit;

    insert into test default values returning x_bin; -- must fail with SQLSTATE = 23000 / validation error ... value "Ÿ"
    update test set x_bin = default where x_bin is null returning x_bin; -- must fail with SQLSTATE = 23000 / validation error ... value "Ÿ"
    commit;

    ------------------------------------------------

    recreate table test(x_vb dom22_12); -- varbinary(2) default 'Ÿ' check( value in ('Ÿ', 'ÿ') )
    insert into test default values returning x_vb;
    insert into test values(null);
    commit;

    alter domain dom22_12 set not null;
    alter domain dom22_12 drop constraint add constraint check( value = any(select 'ł' from rdb$database union all select 'ă' from rdb$database) );
    commit;

    insert into test default values returning x_vb; -- must fail with SQLSTATE = 23000 / validation error ... value "Ÿ"
    update test set x_vb = default where x_vb is null returning x_vb; -- must fail with SQLSTATE = 23000 / validation error ... value "Ÿ"
    commit;

    ------------------------------------------------

    recreate table test(x_boo dom22_13); -- boolean        default false check ( value is not true );
    insert into test default values returning x_boo;
    insert into test values(null);
    commit;

    alter domain dom22_13 set not null;
    alter domain dom22_13 drop constraint add constraint check( value NOT in (false) );
    commit;

    insert into test default values returning x_boo;
    update test set x_boo = default where x_boo is null returning x_boo;
    commit;

    ------------------------------------------------

    recreate table test(x_df16 dom22_14); -- decfloat(16)   default -9.999999999999999E+384 check( log10(abs(value)) >= 384 );

    insert into test default values returning x_df16;
    insert into test values(null);
    commit;

    alter domain dom22_14 set not null;
    alter domain dom22_14 drop constraint add constraint check( value = 0 );
    commit;

    insert into test default values returning x_df16;
    update test set x_df16 = default where x_df16 is null returning x_df16;
    commit;

    ------------------------------------------------

    recreate table test(x_df34 dom22_15); -- default -9.999999999999999999999999999999999E6144 check( log10(abs(value)) >= 6144 );

    insert into test default values returning x_df34;
    insert into test values(null);
    commit;

    alter domain dom22_15 set not null;
    alter domain dom22_15 set default 0;
    alter domain dom22_15 drop constraint add constraint check( log10(abs(value)) < 0 );
    commit;

    insert into test default values returning x_df34; -- must fail with SQLSTATE = 42000 / -Argument for LOG10 must be positive
    update test set x_df34 = default where x_df34 is null returning x_df34; -- must fail with SQLSTATE = 42000 / -Argument for LOG10 must be positive
    commit;

    ------------------------------------------------

    recreate table test(x_i128 dom22_16); -- int128  default 170141183460469231731687303715884105727 check( value in(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727) );

    insert into test default values returning x_i128;
    insert into test values(null);
    commit;

    alter domain dom22_16 set not null;
    alter domain dom22_16 drop constraint add constraint check(value between bin_shr(-170141183460469231731687303715884105727,127) and bin_shr(170141183460469231731687303715884105727,127)); -- -1...0
    commit;

    insert into test default values returning x_i128; -- must fail with SQLSTATE = 23000 / validation error ... value "170141183460469231731687303715884105727"
    update test set x_i128 = default where x_i128 is null returning x_i128; -- must fail with SQLSTATE = 23000 / validation error ... value "170141183460469231731687303715884105727"
    commit;

    ------------------------------------------------

    recreate table test(x_tmtz dom22_17); -- time with time zone default '11:11:11.111 Indian/Cocos' check ( extract(hour from value) <=12 );

    insert into test default values returning x_tmtz;
    insert into test values(null);
    commit;

    alter domain dom22_17 set not null;
    alter domain dom22_17 drop constraint add constraint check( extract(minute from value) < 10 );
    commit;

    insert into test default values returning x_tmtz; -- must fail with SQLSTATE = 23000 / validation error ... value "11:11:11.1110 Indian/Cocos"
    update test set x_tmtz = default where x_tmtz is null returning x_tmtz; -- must fail with SQLSTATE = 23000 / validation error ... value "11:11:11.1110 Indian/Cocos"
    commit;

    ------------------------------------------------

    recreate table test(x_dts dom22_18); -- timestamp                default '01.01.0001 00:00:01.001'              check ( extract(minute from value) = 0 );

    insert into test default values returning x_dts;
    insert into test values(null);
    commit;

    alter domain dom22_18 set not null;
    alter domain dom22_18 drop constraint add constraint check( extract(hour from value) > 7 );
    commit;

    insert into test default values returning x_dts; -- must fail with SQLSTATE = 23000 / validation error ... value "01-JAN-0001 0:00:01.0010"
    update test set x_dts = default where x_dts is null returning x_dts; -- must fail with SQLSTATE = 23000 / validation error ... value "01-JAN-0001 0:00:01.0010"
    commit;

    ------------------------------------------------

    recreate table test(x_tstz dom22_19); -- timestamp with time zone default '21.12.2013 11:11:11.111 Indian/Cocos' check ( extract(minute from value) <=30 );
    insert into test default values returning x_tstz;
    insert into test values(null);
    commit;

    alter domain dom22_19 set not null;
    alter domain dom22_19 drop constraint add constraint check( value = '21.12.2013 11:11:11.111 Indian/Comoro' );
    commit;

    insert into test default values returning x_tstz; -- must fail with SQLSTATE = 23000 / validation error ... value "01-JAN-0001 0:00:01.0010"
    update test set x_tstz = default where x_tstz is null returning x_tstz; -- must fail with SQLSTATE = 23000 / validation error ... value "01-JAN-0001 0:00:01.0010"
    commit;

    ------------------------------------------------

    recreate table test(x_blob_20 dom22_20); -- default 'Ÿ' check( value in ('Ÿ', 'ÿ') );

    insert into test default values returning x_blob_20;
    insert into test values(null);
    commit;

    alter domain dom22_20 set not null;
    alter domain dom22_20 drop constraint add constraint check( value in ('ă', 'ô') );
    commit;

    insert into test default values returning x_blob_20; -- must fail with SQLSTATE = 22018 / conversion error ... value "BLOB"
    update test set x_blob_20 = default where x_blob_20 is null returning x_blob_20; -- must fail with SQLSTATE = 22018 / conversion error ... value "BLOB"
    commit;

    ------------------------------------------------

    recreate table test(x_blob_21 dom22_21); -- blob sub_type text character set utf8 default 'Ätran' check (value is null or value NOT in (select river from rivers)) collate nm_coll;

    insert into test default values returning x_blob_21;
    insert into test values(null);
    commit;

    alter domain dom22_21 set not null;
    alter domain dom22_21 drop constraint add constraint check( value in (select river from rivers) );
    commit;

    insert into test default values returning x_blob_21; -- must fail with SQLSTATE = 22018 / conversion error ... value "BLOB"
    update test set x_blob_21 = default where x_blob_21 is null returning x_blob_21; -- must fail with SQLSTATE = 22018 / conversion error ... value "BLOB"
    commit;

    ------------------------------------------------

    recreate table test(x_blob_22 dom22_22); -- blob sub_type binary default 0x10 check (value > 0x01);

    insert into test default values returning x_blob_22;
    insert into test values(null);
    commit;

    alter domain dom22_22 set not null;
    alter domain dom22_22 drop constraint add constraint check( value < 0x01 );
    commit;

    insert into test default values returning x_blob_22; -- must fail with SQLSTATE = 22018 / conversion error ... value "BLOB"
    update test set x_blob_22 = default where x_blob_22 is null returning x_blob_22; -- must fail with SQLSTATE = 22018 / conversion error ... value "BLOB"
    commit;
"""

tmp_file = temp_file('functional_gtcs_dsql_domain_22.sql')

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_file: Path):
    tmp_file.write_bytes(test_script_1.encode('utf-8'))
    act.expected_stdout = test_expected_stdout
    act.expected_stderr = test_expected_stderr

    act.isql(switches=['-q'], input_file=tmp_file, charset='utf8')
    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)

