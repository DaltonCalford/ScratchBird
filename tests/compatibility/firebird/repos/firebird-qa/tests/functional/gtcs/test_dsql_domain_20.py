#coding:utf-8

"""
ID:          gtcs.dsql-domain-20
FBTEST:      functional.gtcs.dsql_domain_20
TITLE:       Verify result of ALTER DOMAIN SET/DROP DEFAULT when a table exists with field based on this domain
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/DSQL_DOMAIN_20.script

  Comment in GTCS
    This script will test using the alter domain statement on domains that are already in use in table definitions.
    Related bugs: have to exit db for changes made to domains to affect data being entered into tables.

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

    create domain dom20_01 as smallint default -32768;
    create domain dom20_02 as int default -2147483648;
    create domain dom20_03 as bigint default -9223372036854775807;
    --create domain dom20_03 as bigint default -9223372036854775808; -- currently raises error 'numeric overflow', see CORE-6291
    create domain dom20_04 as date default '31.12.9999';
    create domain dom20_05 as time default '23:59:59.999';
    create domain dom20_06 as time with time zone default '11:11:11.111 Indian/Cocos';
    create domain dom20_07 as timestamp default '01.01.0001 00:00:01.001';
    create domain dom20_08 as timestamp with time zone default '21.12.2013 11:11:11.111 Indian/Cocos';
    create domain dom20_09 as char(1) character set utf8 default '€';
    create domain dom20_10 as varchar(1) character set utf8 default '¢';
    -- https://en.wikipedia.org/wiki/ISO/IEC_8859-1,
	-- see table "Languages with incomplete coverage",
	-- column "Typical workaround" for Hungarian 'Ő':
	create domain dom20_11 as nchar(1) default 'Ö' ;
    create domain dom20_12 as numeric(2,2) default -327.68;
	create domain dom20_13 as decimal(20,2) default -999999999999999999;

	-- Online evaluation of expressions: https://www.wolframalpha.com

	-- https://en.wikipedia.org/wiki/Single-precision_floating-point_format
	-- (largest number less than one):  1 - power(2,-24)
	create domain dom20_14 as float default 0.999999940395355224609375;

	-- https://en.wikipedia.org/wiki/Double-precision_floating-point_format
	-- Max Double: power(2,1023) * ( 1+(1-power(2,-52) )
	create domain dom20_15 as double precision default 1.7976931348623157e308;

	create domain dom20_16 as blob default 'Ø';

    create domain dom20_17 as boolean default false;
    create domain dom20_18 as decfloat(16) default -9.999999999999999E+384;
    create domain dom20_19 as decfloat default -9.999999999999999999999999999999999E6144;
    commit;

    recreate table test(
         f01 dom20_01
        ,f02 dom20_02
        ,f03 dom20_03
        ,f04 dom20_04
        ,f05 dom20_05
        ,f06 dom20_06
        ,f07 dom20_07
        ,f08 dom20_08
        ,f09 dom20_09
        ,f10 dom20_10
        ,f11 dom20_11
        ,f12 dom20_12
        ,f13 dom20_13
        ,f14 dom20_14
        ,f15 dom20_15
		,f16_blob_id dom20_16
        ,f17 dom20_17
        ,f18 dom20_18
        ,f19 dom20_19
	);
	commit;

	insert into test default values;
	select 'point-1' as msg, t.* from test t;
	rollback;

	alter domain dom20_01 drop default;
	alter domain dom20_02 drop default;
	alter domain dom20_03 drop default;
	alter domain dom20_04 drop default;
	alter domain dom20_05 drop default;
	alter domain dom20_06 drop default;
	alter domain dom20_07 drop default;
	alter domain dom20_08 drop default;
	alter domain dom20_09 drop default;
	alter domain dom20_10 drop default;
	alter domain dom20_11 drop default;
	alter domain dom20_12 drop default;
	alter domain dom20_13 drop default;
	alter domain dom20_14 drop default;
	alter domain dom20_15 drop default;
	alter domain dom20_16 drop default;
	alter domain dom20_17 drop default;
	alter domain dom20_18 drop default;
	alter domain dom20_19 drop default;
	commit;

	insert into test default values;
	select 'point-2' as msg, t.* from test t;
	rollback;

	alter domain dom20_01 set default 0x7FFF; -- 32767
	alter domain dom20_02 set default 0x7FFFFFFF; -- 2147483647
	alter domain dom20_03 set default 0x7FFFFFFFFFFFFFFF; -- 9223372036854775807
	alter domain dom20_04 set default '01.01.0001';
	alter domain dom20_05 set default '23:59:59.999';
	alter domain dom20_06 set default '22:22:22.222 Pacific/Fiji';
	alter domain dom20_07 set default '15.12.1234 12:34:56.789';
	alter domain dom20_08 set default '22.12.2222 22:22:22.222 Pacific/Fiji';
	alter domain dom20_09 set default '¥';
	alter domain dom20_10 set default '£';
	alter domain dom20_11 set default 'Ç';
	alter domain dom20_12 set default 327.67;
	alter domain dom20_13 set default 0xDE0B6B3A763FFFF; -- 999999999999999999
	alter domain dom20_14 set default 1.0000001192;
	alter domain dom20_15 set default 1.4012984643e-45;
	alter domain dom20_16 set default 'Ö';
	alter domain dom20_17 set default true;
	alter domain dom20_18 set default 9.999999999999999E+384;
	alter domain dom20_19 set default 9.999999999999999999999999999999999E+6144;

	insert into test default values;
	select 'point-3' as msg, t.* from test t;
	rollback;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('F16_BLOB_ID.*', '')])

expected_stdout = """
	MSG                             point-1
	F01                             -32768
	F02                             -2147483648
	F03                             -9223372036854775807
	F04                             9999-12-31
	F05                             23:59:59.9990
	F06                             11:11:11.1110 Indian/Cocos
	F07                             0001-01-01 00:00:01.0010
	F08                             2013-12-21 11:11:11.1110 Indian/Cocos
	F09                             €
	F10                             ¢
	F11                             Ö
	F12                             -327.68
	F13                                                    -999999999999999999.00
	F14                             0.99999994
	F15                             1.797693134862316e+308
	F16_BLOB_ID                     80:0
	Ø
	F17                             <false>
	F18                             -9.999999999999999E+384
	F19                             -9.999999999999999999999999999999999E+6144



	MSG                             point-2
	F01                             <null>
	F02                             <null>
	F03                             <null>
	F04                             <null>
	F05                             <null>
	F06                             <null>
	F07                             <null>
	F08                             <null>
	F09                             <null>
	F10                             <null>
	F11                             <null>
	F12                             <null>
	F13                             <null>
	F14                             <null>
	F15                             <null>
	F16_BLOB_ID                     <null>
	F17                             <null>
	F18                             <null>
	F19                             <null>



	MSG                             point-3
	F01                             32767
	F02                             2147483647
	F03                             9223372036854775807
	F04                             0001-01-01
	F05                             23:59:59.9990
	F06                             22:22:22.2220 Pacific/Fiji
	F07                             1234-12-15 12:34:56.7890
	F08                             2222-12-22 22:22:22.2220 Pacific/Fiji
	F09                             ¥
	F10                             £
	F11                             Ç
	F12                             327.67
	F13                                                     999999999999999999.00
	F14                             1.0000001
	F15                             1.401298464300000e-45
	F16_BLOB_ID                     80:0
	Ö
	F17                             <true>
	F18                              9.999999999999999E+384
	F19                              9.999999999999999999999999999999999E+6144
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
