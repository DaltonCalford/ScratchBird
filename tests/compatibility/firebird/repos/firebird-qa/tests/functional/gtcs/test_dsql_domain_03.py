#coding:utf-8

"""
ID:          gtcs.dsql-domain-03
FBTEST:      functional.gtcs.dsql_domain_03
TITLE:       Test the level 0 syntax for SQL "CREATE DOMAIN" statement using datatype and NOT NULL constraint
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/DSQL_DOMAIN_03.script

  NB: avoid usage of ISQL command 'SHOW DOMAIN' because of unstable output.
  We display info about domains using common VIEW based on RDB$FIELDS table.
  Columns with rdb$validation_source and rdb$default_source contain BLOB data thus we have to skip from showing their blob ID - see substitution.
NOTES:
  Added domains with datatype that did appear only in FB 4.0: DECFLOAT and
  TIME[STAMP] WITH TIME ZONE. For this reason only FB 4.0+ can be tested.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create view v_test as
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
        ,ff.rdb$validation_source as dm_fvalid_blob_id
        ,ff.rdb$default_source as dm_fdefault_blob_id
    from rdb$fields ff
    where
        ff.rdb$system_flag is distinct from 1
        and ff.rdb$field_name starting with upper( 'dom0' )
    ;
    commit;

    set bail on;
    create domain dom03_01 as smallint not null;
    create domain dom03_02 as integer not null;
    create domain dom03_03 as bigint not null;
    create domain dom03_04 as date not null;
    create domain dom03_05 as char(20) not null;
    create domain dom03_06 as varchar(25) not null;
    create domain dom03_07 as decimal(6,2) not null;
    create domain dom03_08 as numeric(6,2) not null;
    create domain dom03_09 as float not null;
    create domain dom03_10 as real not null;
    create domain dom03_11 as double precision not null;
    create domain dom03_12 as long float not null;
    create domain dom03_13 as blob not null;
    create domain dom03_14 as blob sub_type text not null;
    create domain dom03_15 as blob sub_type binary not null;
    create domain dom03_16 as boolean not null;
    create domain dom03_17 as time not null;
    create domain dom03_18 as time with time zone not null;
    create domain dom03_19 as timestamp not null;
    create domain dom03_20 as timestamp with time zone not null;
    create domain dom03_21 as nchar(20) not null;
    create domain dom03_22 as binary(20) not null; -- this datatype is alias for char(N) character set octets
    create domain dom03_23 as varbinary(20) not null;
    create domain dom03_24 as decfloat not null;
    create domain dom03_25 as decfloat(16) not null;
    create domain dom03_26 as decfloat(34) not null;
    commit;
    set list on;
    set count on;
    select * from v_test order by dm_name;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('DM_FDEFAULT_BLOB_ID.*', ''),
                                                 ('DM_FVALID_BLOB_ID.*', '')])

expected_stdout = """
    DM_NAME                         DOM03_01
    DM_TYPE                         7
    DM_SUBTYPE                      0
    DM_FLEN                         2
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_02
    DM_TYPE                         8
    DM_SUBTYPE                      0
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_03
    DM_TYPE                         16
    DM_SUBTYPE                      0
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_04
    DM_TYPE                         12
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_05
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         20
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      20
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_06
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         25
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      25
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_07
    DM_TYPE                         8
    DM_SUBTYPE                      2
    DM_FLEN                         4
    DM_FSCALE                       -2
    DM_FPREC                        6
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_08
    DM_TYPE                         8
    DM_SUBTYPE                      1
    DM_FLEN                         4
    DM_FSCALE                       -2
    DM_FPREC                        6
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_09
    DM_TYPE                         10
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_10
    DM_TYPE                         10
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_11
    DM_TYPE                         27
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_12
    DM_TYPE                         27
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_13
    DM_TYPE                         261
    DM_SUBTYPE                      0
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_14
    DM_TYPE                         261
    DM_SUBTYPE                      1
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_15
    DM_TYPE                         261
    DM_SUBTYPE                      0
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_16
    DM_TYPE                         23
    DM_SUBTYPE                      <null>
    DM_FLEN                         1
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_17
    DM_TYPE                         13
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_18
    DM_TYPE                         28
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_19
    DM_TYPE                         35
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_20
    DM_TYPE                         29
    DM_SUBTYPE                      <null>
    DM_FLEN                         12
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_21
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         20
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        21
    DM_FCOLL                        0
    DM_FCHRLEN                      20
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_22
    DM_TYPE                         14
    DM_SUBTYPE                      1
    DM_FLEN                         20
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        1
    DM_FCOLL                        0
    DM_FCHRLEN                      20
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_23
    DM_TYPE                         37
    DM_SUBTYPE                      1
    DM_FLEN                         20
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        1
    DM_FCOLL                        0
    DM_FCHRLEN                      20
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_24
    DM_TYPE                         25
    DM_SUBTYPE                      <null>
    DM_FLEN                         16
    DM_FSCALE                       0
    DM_FPREC                        34
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_25
    DM_TYPE                         24
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        16
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    DM_NAME                         DOM03_26
    DM_TYPE                         25
    DM_SUBTYPE                      <null>
    DM_FLEN                         16
    DM_FSCALE                       0
    DM_FPREC                        34
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        1
    DM_FVALID_BLOB_ID               <null>
    DM_FDEFAULT_BLOB_ID             <null>
    Records affected: 26
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
