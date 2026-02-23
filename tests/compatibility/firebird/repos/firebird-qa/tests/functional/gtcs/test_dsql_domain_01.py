#coding:utf-8

"""
ID:          gtcs.dsql-domain-01
FBTEST:      functional.gtcs.dsql_domain_01
TITLE:       Test the level 0 syntax for SQL create domain defining only the datatype
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/DSQL_DOMAIN_01.script

  NB: avoid usage of ISQL command 'SHOW DOMAIN' because of unstable output.
  We display info about domains using common VIEW based on RDB$FIELDS table.
"""

import pytest
from firebird.qa import db_factory, isql_act, Action

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
        ,ff.rdb$validation_source as dm_fvalid
        ,ff.rdb$default_source as dm_fdefault
    from rdb$fields ff
    where
        ff.rdb$system_flag is distinct from 1
        and ff.rdb$field_name starting with upper( 'dom0' )
    ;
    commit;

    set bail on;
    create domain dom01a_1 as smallint;
    create domain dom01a_2 as numeric(3,1);
    create domain dom01b_1 as integer;
    create domain dom01b_2 as int;
    create domain dom01b_3 as numeric;
    create domain dom01b_4 as numeric(6,2);
    create domain dom01c as date;
    create domain dom01d_1 as char(20);
    create domain dom01d_2 as character(99);
    create domain dom01e_1 as varchar(25);
    create domain dom01e_2 as character varying(100);
    create domain dom01e_3 as char varying(2);
    create domain dom01f_1 as decimal(6,2);
    create domain dom01g_1 as float;
    create domain dom01g_2 as long float;
    create domain dom01g_3 as real;
    create domain dom01h as double precision;
    create domain dom01i_1 as blob;
    create domain dom01i_2 as blob(60,1);
    commit;
    set bail off;

    set list on;
    set count on;
    select * from v_test order by dm_name;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|SQL error code).)*$', ''),
                                                 (' = ', ' '), ('[ \t]+', ' ')])

expected_stdout = """
    DM_NAME                         DOM01A_1
    DM_TYPE                         7
    DM_SUBTYPE                      0
    DM_FLEN                         2
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01A_2
    DM_TYPE                         7
    DM_SUBTYPE                      1
    DM_FLEN                         2
    DM_FSCALE                       -1
    DM_FPREC                        3
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01B_1
    DM_TYPE                         8
    DM_SUBTYPE                      0
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01B_2
    DM_TYPE                         8
    DM_SUBTYPE                      0
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        0
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01B_3
    DM_TYPE                         8
    DM_SUBTYPE                      1
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        9
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01B_4
    DM_TYPE                         8
    DM_SUBTYPE                      1
    DM_FLEN                         4
    DM_FSCALE                       -2
    DM_FPREC                        6
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01C
    DM_TYPE                         12
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01D_1
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         20
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      20
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01D_2
    DM_TYPE                         14
    DM_SUBTYPE                      0
    DM_FLEN                         99
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      99
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01E_1
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         25
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      25
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01E_2
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         100
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      100
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01E_3
    DM_TYPE                         37
    DM_SUBTYPE                      0
    DM_FLEN                         2
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      2
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01F_1
    DM_TYPE                         8
    DM_SUBTYPE                      2
    DM_FLEN                         4
    DM_FSCALE                       -2
    DM_FPREC                        6
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01G_1
    DM_TYPE                         10
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01G_2
    DM_TYPE                         27
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01G_3
    DM_TYPE                         10
    DM_SUBTYPE                      <null>
    DM_FLEN                         4
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01H
    DM_TYPE                         27
    DM_SUBTYPE                      <null>
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01I_1
    DM_TYPE                         261
    DM_SUBTYPE                      0
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        <null>
    DM_FCOLL                        <null>
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    DM_NAME                         DOM01I_2
    DM_TYPE                         261
    DM_SUBTYPE                      1
    DM_FLEN                         8
    DM_FSCALE                       0
    DM_FPREC                        <null>
    DM_FCSET                        0
    DM_FCOLL                        0
    DM_FCHRLEN                      <null>
    DM_FNULL                        <null>
    DM_FVALID                       <null>
    DM_FDEFAULT                     <null>
    Records affected: 19
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
