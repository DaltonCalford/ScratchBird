#coding:utf-8

"""
ID:          gtcs.dsql-domain-15
FBTEST:      functional.gtcs.dsql_domain_15
TITLE:       Verify result of INSERT DEFAULT for domain-based fields which are declared as NOT NULL and have their own default values
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/DSQL_DOMAIN_15.script

  Comment in GTCS
    This script will utilize the datatype, default and not null
    clauses in the create domain statement. A table is then
    created using the domain definitions with overriding column
    deafults, then data is added to the table with missing fields
    being supplied by the column or domain defaults.

  ::: NOTE :::
  Added domains with datatype that did appear only in FB 4.0: DECFLOAT and
  TIME[STAMP] WITH TIME ZONE. For this reason only FB 4.0+ can be tested.

  Currently following datatypes are NOT checked:
    blob sub_type text|binary
    long float;
    binary(20);
    varbinary(20);
"""

import pytest
from firebird.qa import *

db = db_factory(charset='utf8')

test_script = """
    set bail on;
    set list on;
    set blob all;
    -- set echo on;

    create domain dom15_01 as smallint default 0 not null;
    create domain dom15_02 as integer default 500 not null;
    create domain dom15_03 as date default 'today' not null;
    create domain dom15_04 as char(20) default 'DEFAULT' not null;
    create domain dom15_05 as varchar(25) default 'VARYING DEFAULT' not null;
    create domain dom15_06 as decimal(6,2) default 4.2 not null;
    create domain dom15_07 as float default 500.1 not null;
    create domain dom15_08 as double precision default 1000 not null;

    create domain dom15_10 as time default '23:59:59.999' not null;
    create domain dom15_11 as time with time zone default '11:11:11.111 Indian/Cocos' not null;
    create domain dom15_12 as timestamp default '01.01.0001 00:00:01.001' not null;
    create domain dom15_13 as timestamp with time zone default '21.12.2013 11:11:11.111 Indian/Cocos' not null;
    create domain dom15_14 as char(1) character set utf8 default '€' not null; -- euro sign, U+20AC
    create domain dom15_15 as varchar(1) character set utf8 default '¢' not null; -- cent sign, U+00A2
    create domain dom15_16 as nchar(1) default 'Æ'  not null; -- https://en.wikipedia.org/wiki/ISO/IEC_8859-1
    create domain dom15_17 as blob default 'Ø' not null;
    create domain dom15_18 as boolean default false not null;
    create domain dom15_19 as decfloat(16) default -9.999999999999999E+384 not null;
    create domain dom15_20 as decfloat default -9.999999999999999999999999999999999E6144 not null;

    commit;

    create table tab15a (
      fld01 dom15_01 default 5000
     ,fld02 dom15_02 default 50000000
     ,fld03 dom15_03 default '01/01/90'
     ,fld04 dom15_04 default 'FIXCHAR DEF'
     ,fld05 dom15_05 default 'VARCHAR DEF'
     ,fld06 dom15_06 default 3.1415926
     ,fld07 dom15_07 default 500.2
     ,fld08 dom15_08 default 2.718281828

     ,fld10 dom15_10 default '22:22:22.222'
     ,fld11 dom15_11 default '07:07:07.007 Pacific/Fiji'
     ,fld12 dom15_12 default '31.12.9999 23:59:59.999'
     ,fld13 dom15_13 default '01.01.1951 15:16:17.189 Pacific/Fiji'
     ,fld14 dom15_14 default '£' -- pound, U+00A3
     ,fld15 dom15_15 default '¥' -- yen euro, U+00A5
     ,fld16 dom15_16 default 'É'
     ,fld17 dom15_17 default 'ß' -- German eszett
     ,fld18 dom15_18 default true
     ,fld19 dom15_19 default 1.0E-383
     ,fld20 dom15_20 default 1.0E-6143
    );

    create view v_test as
    select
        fld01
        ,fld02
        ,fld03
        ,fld04
        ,fld05
        ,fld06
        ,fld07
        ,fld08
        ,fld10
        ,fld11
        ,fld12
        ,fld13
        ,fld14
        ,fld15
        ,fld16
        ,fld17 as fld17_blob_id
        ,fld18
        ,fld19
        ,fld20
    from tab15a;

    set term ^;
    create procedure sp_test returns (
      o_01 dom15_01
     ,o_02 dom15_02
     ,o_03 dom15_02
     ,o_04 dom15_02
     ,o_05 dom15_02
     ,o_06 dom15_02
     ,o_07 dom15_02
     ,o_08 dom15_02

     ,o_10 dom15_10
     ,o_11 dom15_11
     ,o_12 dom15_12
     ,o_13 dom15_13
     ,o_14 dom15_14
     ,o_15 dom15_15
     ,o_16 dom15_16
     ,o_17 dom15_17
     ,o_18 dom15_18
     ,o_19 dom15_19
     ,o_20 dom15_20
    ) as
    begin
      suspend;
    end
    ^
    set term ;^
    commit;

    insert into tab15a default values;

    select * from v_test;

    select
         o_01
        ,o_02
        ,o_03
        ,o_04
        ,o_05
        ,o_06
        ,o_07
        ,o_08
        ,o_10
        ,o_11
        ,o_12
        ,o_13
        ,o_14
        ,o_15
        ,o_16
        ,o_17 as o_17_blob_id
        ,o_18
        ,o_19
        ,o_20
    from sp_test;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('FLD17_BLOB_ID.*', ''),
                                                 ('O_17_BLOB_ID.*', '')])

expected_stdout = """
    FLD01                           5000
    FLD02                           50000000
    FLD03                           1990-01-01
    FLD04                           FIXCHAR DEF
    FLD05                           VARCHAR DEF
    FLD06                           3.14
    FLD07                           500.20001
    FLD08                           2.718281828000000
    FLD10                           22:22:22.2220
    FLD11                           07:07:07.0070 Pacific/Fiji
    FLD12                           9999-12-31 23:59:59.9990
    FLD13                           1951-01-01 15:16:17.1890 Pacific/Fiji
    FLD14                           £
    FLD15                           ¥
    FLD16                           É
    FLD17_BLOB_ID                   80:0
    ß
    FLD18                           <true>
    FLD19                                          1.0E-383
    FLD20                                                            1.0E-6143
    O_01                            0
    O_02                            500
    O_03                            500
    O_04                            500
    O_05                            500
    O_06                            500
    O_07                            500
    O_08                            500
    O_10                            23:59:59.9990
    O_11                            11:11:11.1110 Indian/Cocos
    O_12                            0001-01-01 00:00:01.0010
    O_13                            2013-12-21 11:11:11.1110 Indian/Cocos
    O_14                            €
    O_15                            ¢
    O_16                            Æ
    O_17_BLOB_ID                    0:3
    Ø
    O_18                            <false>
    O_19                            -9.999999999999999E+384
    O_20                            -9.999999999999999999999999999999999E+6144
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
