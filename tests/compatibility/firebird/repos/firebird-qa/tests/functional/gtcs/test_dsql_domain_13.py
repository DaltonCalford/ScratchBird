#coding:utf-8

"""
ID:          gtcs.dsql-domain-13
FBTEST:      functional.gtcs.dsql_domain_13
TITLE:       Verify result of INSERT DEFAULT for domain-based fields which have their own default values
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/DSQL_DOMAIN_13.script

  Comment in GTCS
    This script will test level 1 syntax checking for create domain
    statement using datatype and default clauses. The domains are then
    used to create a table where column defaults are also specified.
    Data is then inserted into the table allowing the missing fields
    to be supplied by the column defaults (where specified) and the
    domain defaults (where no column default exists).

  ::: NOTE :::
  Added domains with datatype that did appear only in FB 4.0: DECFLOAT and
  TIME[STAMP] WITH TIME ZONE. For this reason only FB 4.0+ can be tested.

  Fields without default values have names 'F1xx': f101, f102, ...
  Fields with their own default values are 'F2xx': f201, f202, ...

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

    create domain dom13_01 as smallint default -32768;
    create domain dom13_02 as int default -2147483648;
    create domain dom13_03 as bigint default -9223372036854775807;
    --create domain dom13_03 as bigint default -9223372036854775808; -- currently raises error 'numeric overflow', see CORE-6291
    create domain dom13_04 as date default '31.12.9999';
    create domain dom13_05 as time default '23:59:59.999';
    create domain dom13_06 as time with time zone default '11:11:11.111 Indian/Cocos';
    create domain dom13_07 as timestamp default '01.01.0001 00:00:01.001';
    create domain dom13_08 as timestamp with time zone default '21.12.2013 11:11:11.111 Indian/Cocos';
    create domain dom13_09 as char(1) character set utf8 default '€';
    create domain dom13_10 as varchar(1) character set utf8 default '¢';
    -- https://en.wikipedia.org/wiki/ISO/IEC_8859-1,
	-- see table "Languages with incomplete coverage",
	-- column "Typical workaround" for Hungarian 'Ő':
	create domain dom13_11 as nchar(1) default 'Ö' ;
    create domain dom13_12 as numeric(2,2) default -327.68;
	create domain dom13_13 as decimal(20,2) default -999999999999999999;

	-- Online evaluation of expressions: https://www.wolframalpha.com

	-- https://en.wikipedia.org/wiki/Single-precision_floating-point_format
	-- (largest number less than one):  1 - power(2,-24)
	create domain dom13_14 as float default 0.999999940395355224609375;

	-- https://en.wikipedia.org/wiki/Double-precision_floating-point_format
	-- Max Double: power(2,1023) * ( 1+(1-power(2,-52) )
	create domain dom13_15 as double precision default 1.7976931348623157e308;

	create domain dom13_16 as blob default 'Ø';

    create domain dom13_17 as boolean default false;
    create domain dom13_18 as decfloat(16) default -9.999999999999999E+384;
    create domain dom13_19 as decfloat default -9.999999999999999999999999999999999E6144;
    commit;

    recreate table test(
         f101 dom13_01
        ,f102 dom13_02
        ,f103 dom13_03
        ,f104 dom13_04
        ,f105 dom13_05
        ,f106 dom13_06
        ,f107 dom13_07
        ,f108 dom13_08
        ,f109 dom13_09
        ,f110 dom13_10
        ,f111 dom13_11
        ,f112 dom13_12
        ,f113 dom13_13
        ,f114 dom13_14
        ,f115 dom13_15
		,f116 dom13_16
        ,f117 dom13_17
        ,f118 dom13_18
        ,f119 dom13_19
		-------------------------------------

        ,f201 dom13_01 default 32767
        ,f202 dom13_02 default 2147483647
        ,f203 dom13_03 default 9223372036854775807
        ,f204 dom13_04 default '01.01.0001'
        ,f205 dom13_05 default '23:59:59.999'
        ,f206 dom13_06 default '22:22:22.222 Pacific/Fiji'
        ,f207 dom13_07 default '15.12.1234 12:34:56.789'
        ,f208 dom13_08 default '22.12.2222 22:22:22.222 Pacific/Fiji'
        ,f209 dom13_09 default '¥'
        ,f210 dom13_10 default '£'
        ,f211 dom13_11 default 'Ç'
        ,f212 dom13_12 default 327.67
        ,f213 dom13_13 default 999999999999999999
        ,f214 dom13_14 default 1.0000001192
        ,f215 dom13_15 default 1.4012984643e-45
        ,f216_blob_id dom13_16 default 'Ö'
        ,f217 dom13_17 default true
        ,f218 dom13_18 default 9.999999999999999E+384
        ,f219 dom13_19 default 9.999999999999999999999999999999999E+6144

   );
   commit;

   insert into test default values;
   set count on;
   select * from test;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), ('F116_BLOB_ID.*', ''), ('F216_BLOB_ID.*', '')])

expected_stdout = """
	F101                            -32768
	F102                            -2147483648
	F103                            -9223372036854775807
	F104                            9999-12-31
	F105                            23:59:59.9990
	F106                            11:11:11.1110 Indian/Cocos
	F107                            0001-01-01 00:00:01.0010
	F108                            2013-12-21 11:11:11.1110 Indian/Cocos
	F109                            €
	F110                            ¢
	F111                            Ö
	F112                            -327.68
	F113                                                   -999999999999999999.00
	F114                            0.99999994
	F115                            1.797693134862316e+308
	F116                            80:0
	Ø
	F117                            <false>
	F118                            -9.999999999999999E+384
	F119                            -9.999999999999999999999999999999999E+6144
	F201                            32767
	F202                            2147483647
	F203                            9223372036854775807
	F204                            0001-01-01
	F205                            23:59:59.9990
	F206                            22:22:22.2220 Pacific/Fiji
	F207                            1234-12-15 12:34:56.7890
	F208                            2222-12-22 22:22:22.2220 Pacific/Fiji
	F209                            ¥
	F210                            £
	F211                            Ç
	F212                            327.67
	F213                                                    999999999999999999.00
	F214                            1.0000001
	F215                            1.401298464300000e-45
	F216_BLOB_ID                    80:1
	Ö
	F217                            <true>
	F218                             9.999999999999999E+384
	F219                             9.999999999999999999999999999999999E+6144

	Records affected: 1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
