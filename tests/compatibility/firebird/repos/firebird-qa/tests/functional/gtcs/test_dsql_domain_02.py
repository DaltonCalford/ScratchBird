#coding:utf-8

"""
ID:          gtcs.dsql-domain-02
FBTEST:      functional.gtcs.dsql_domain_02
TITLE:       Test the level 0 syntax for SQL "CREATE DOMAIN" statement using datatype and DEFAULT clauses
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/DSQL_DOMAIN_02.script

  NB: avoid usage of ISQL command 'SHOW DOMAIN' because of unstable output.
  We display info about domains using common VIEW based on RDB$FIELDS table.
  Columns with rdb$validation_source and rdb$default_source contain BLOB data thus we have
  to skip from showing their blob ID - see substitution.
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
    create domain dom02a1 as smallint default 0;
    create domain dom02b1 as integer default 0;
    create domain dom02d1 as char(30) default 0;
    create domain dom02e1 as varchar(4) default 0;
    create domain dom02f1 as decimal(10,1) default 0;
    create domain dom02g1 as float default 0;
    create domain dom02h1 as double precision default 0;
    create domain dom02d2 as char(30) default 'def';
    create domain dom02e2 as varchar(4) default 'def';
    create domain dom02c3_1 as date default '27-JAN-1992';
    create domain dom02c3_2 as date default 'today';
    create domain dom02c3_3 as date default '01/27/92';
    create domain dom02d3 as char(30) default '28-OCT-1990';
    create domain dom02e3 as varchar(8) default '09/01/82';
    create domain dom02j as smallint default null;
    create domain dom02k as integer default null;
    create domain dom02l as date default null;
    create domain dom02m as char(2) default null;
    create domain dom02n as varchar(15) default null;
    create domain dom02o as decimal(4,1) default null;
    create domain dom02p as float default null;
    create domain dom02q as double precision default null;
    create domain dom02r as blob default null;
    create domain dom02v as char(15) default user;
    create domain dom02w as varchar(60) default user;
    commit;
    set bail off;

    set list on;
    set count on;
    select * from v_test order by dm_name;
--('[ 	]+', ' '),
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('DM_FDEFAULT_BLOB_ID.*', ''),
                                                 ('DM_FVALID_BLOB_ID.*', '')])

expected_stdout = """
    DM_NAME                         DOM02A1
    DM_TYPE                         7
    DM_SUBTYPE                      0
    DM_FLEN                         2
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default 0
    DM_NAME                         DOM02B1
    DM_TYPE                         8
    DM_SUBTYPE                      0
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default 0
    DM_NAME                         DOM02C3_1
    DM_TYPE                         12
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default '27-JAN-1992'
    DM_NAME                         DOM02C3_2
    DM_TYPE                         12
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default 'today'
    DM_NAME                         DOM02C3_3
    DM_TYPE                         12
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default '01/27/92'
    DM_NAME                         DOM02D1
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         30
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      30
    DM_FNULL                        <null>
    default 0
    DM_NAME                         DOM02D2
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         30
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      30
    DM_FNULL                        <null>
    default 'def'
    DM_NAME                         DOM02D3
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         30
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      30
    DM_FNULL                        <null>
    default '28-OCT-1990'
    DM_NAME                         DOM02E1
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      4
    DM_FNULL                        <null>
    default 0
    DM_NAME                         DOM02E2
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      4
    DM_FNULL                        <null>
    default 'def'
    DM_NAME                         DOM02E3
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      8
    DM_FNULL                        <null>
    default '09/01/82'
    DM_NAME                         DOM02F1
    DM_TYPE                         16
    DM_SUBTYPE                      2
    DM_FLEN                         8
    DM_FSCALE                       -1
    DM_FPREC                        10
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default 0
    DM_NAME                         DOM02G1
    DM_TYPE                         10
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default 0
    DM_NAME                         DOM02H1
    DM_TYPE                         27
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default 0
    DM_NAME                         DOM02J
    DM_TYPE                         7
    DM_SUBTYPE                      0
    DM_FLEN                         2
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default null
    DM_NAME                         DOM02K
    DM_TYPE                         8
    DM_SUBTYPE                      0
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default null
    DM_NAME                         DOM02L
    DM_TYPE                         12
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default null
    DM_NAME                         DOM02M
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         2
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      2
    DM_FNULL                        <null>
    default null
    DM_NAME                         DOM02N
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         15
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      15
    DM_FNULL                        <null>
    default null
    DM_NAME                         DOM02O
    DM_TYPE                         8
    DM_SUBTYPE                      2
    DM_FLEN                         4
    DM_FSCALE                       -1
    DM_FPREC                        4
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default null
    DM_NAME                         DOM02P
    DM_TYPE                         10
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default null
    DM_NAME                         DOM02Q
    DM_TYPE                         27
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default null
    DM_NAME                         DOM02R
    DM_TYPE                         261
    DM_SUBTYPE                      0
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    default null
    DM_NAME                         DOM02V
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         15
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      15
    DM_FNULL                        <null>
    default user
    DM_NAME                         DOM02W
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         60
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      60
    DM_FNULL                        <null>
    default user
    Records affected: 25
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
