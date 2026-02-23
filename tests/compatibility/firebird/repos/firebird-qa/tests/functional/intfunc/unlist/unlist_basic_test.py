#coding:utf-8

"""
ID:          issue-8418
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8418
TITLE:       UNLIST function. Basic examples.
DESCRIPTION: Provided by red-soft. Original file name: "unlist.test_eamples.py"
NOTES:
    [09.04.2025] pzotov
    Checked on 6.0.0.722
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- Following examples must PASS:

    select u.* from unlist('1,2,3,4,5') as u(example_01);
    select u.* from unlist('6,7,8,9,0' returning int) as u(example_02);
    select u.* from unlist('11:22:33:44:55',':') as u(example_03);
    select u.* from unlist('100:200:300:400:500',':' returning int) as u(example_04);
    select u.* from unlist('1:2:3:4:5',':' returning float) as u(example_05);
    select u.* from unlist('123:456:789',':' returning varchar(10)) as u(example_06);
    select u.* from unlist('monday,tuesday,wednesday,thursday,friday,saturday,sunday') as u(example_07);
    select u.* from unlist('monday_tuesday_wednesday_thursday_friday_saturday_sunday', '_' returning varchar(9)) as u(example_08);

    select u.* from unlist('12:00 gmt,11:00 gmt' returning time) as u(example_09);
    select u.* from unlist('31.12.2024 23:59:59.9999,31.12.2025 23:59:59.9999' returning timestamp) as u(example_10);
    select u.* from unlist('31.12.2024 23:59:59.9999,31.12.2025 23:59:59.9999') as u(example_11);
    select u.* from unlist('true,false' returning boolean) as u(example_12);
    select u.* from unlist('7,6,8,9,0' returning int) as u(example_13) order by u.example_13;

    select u.* from unlist('a*,as a') as u(example_14);
    select u.* from unlist('b,as ab') as u(example_15);
    select u.* from unlist('ab,as ab') as u(example_16);

    select u.* from unlist('order by a* as a,1,2,3') as u(example_17) order by 1;
    select u.* from unlist('order by ab as ab,1,2,3') as u(example_18) order by 1;
    select u.* from unlist('group by a* as a,1,2,3') as u(example_19) group by 1;
    select u.* from unlist('order by ab as ab,1,2,3') as u(example_20) group by 1;

    select u.* from unlist('1,0,2,0') as u(example_21) where u.example_21 = 0;

    select w.* from(select u.* from unlist('*,*,as a') as u(example_22)) w;

    select w.* from(select u.* from unlist('*,a*,as a') as u(example_23)) w;
    select w.* from(select u.* from unlist('*,ab,as ab') as u(example_24)) w;

    select c.* from(select u.* from unlist('c*,*,as a,as c') as u(example_25)) as c;
    select c.* from(select u.* from unlist('c*,a*,as a,as c') as u(example_26)) as c;
    select c.* from(select u.* from unlist('c*,ab,as ab,as c') as u(example_27)) as c;

    select c.* from(select u.* from unlist('cd,*,as a,as cd') as u) as c(example_28);
    select c.* from(select u.* from unlist('cd,a*,as a,as cd') as u) as c(example_29);
    select c.* from(select u.* from unlist('cd,ab,as ab,as cd') as u) as c(example_30);

    recreate view v1 (example_31) as select u.* from unlist('view,2,3') as u(example_31);
    select u.* from v1 as u;

    ----------------------------------------------------------------------------------------

    -- Following examples must FAIL with "SQLSTATE = 42S22 / ... / column unknown UNLIST":

    select * from unlist('1,0,2,0') as a where unlist = 0;
    select unlist from unlist('unlist,a,s,a') as u(example_1 );
    select unlist from unlist('unlist,a,s,a' returning varchar(10)) as u(example_1 );
    select a.unlist from unlist('a.unlist,a,s,a') as u(example_1 );
    select a.unlist from unlist('a.unlist,a,s,a' returning varchar(10)) as u(example_1 );
"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' '), ('(-)?At line \\d+, column \\d+', '') ])

expected_stdout = """
    EXAMPLE_01 1
    EXAMPLE_01 2
    EXAMPLE_01 3
    EXAMPLE_01 4
    EXAMPLE_01 5
    EXAMPLE_02 6
    EXAMPLE_02 7
    EXAMPLE_02 8
    EXAMPLE_02 9
    EXAMPLE_02 0
    EXAMPLE_03 11
    EXAMPLE_03 22
    EXAMPLE_03 33
    EXAMPLE_03 44
    EXAMPLE_03 55
    EXAMPLE_04 100
    EXAMPLE_04 200
    EXAMPLE_04 300
    EXAMPLE_04 400
    EXAMPLE_04 500
    EXAMPLE_05 1
    EXAMPLE_05 2
    EXAMPLE_05 3
    EXAMPLE_05 4
    EXAMPLE_05 5
    EXAMPLE_06 123
    EXAMPLE_06 456
    EXAMPLE_06 789
    EXAMPLE_07 monday
    EXAMPLE_07 tuesday
    EXAMPLE_07 wednesday
    EXAMPLE_07 thursday
    EXAMPLE_07 friday
    EXAMPLE_07 saturday
    EXAMPLE_07 sunday
    EXAMPLE_08 monday
    EXAMPLE_08 tuesday
    EXAMPLE_08 wednesday
    EXAMPLE_08 thursday
    EXAMPLE_08 friday
    EXAMPLE_08 saturday
    EXAMPLE_08 sunday
    EXAMPLE_09 15:00:00.0000
    EXAMPLE_09 14:00:00.0000
    EXAMPLE_10 2024-12-31 23:59:59.9999
    EXAMPLE_10 2025-12-31 23:59:59.9999
    EXAMPLE_11 31.12.2024 23:59:59.9999
    EXAMPLE_11 31.12.2025 23:59:59.9999
    EXAMPLE_12 <true>
    EXAMPLE_12 <false>
    EXAMPLE_13 0
    EXAMPLE_13 6
    EXAMPLE_13 7
    EXAMPLE_13 8
    EXAMPLE_13 9
    EXAMPLE_14 a*
    EXAMPLE_14 as a
    EXAMPLE_15 b
    EXAMPLE_15 as ab
    EXAMPLE_16 ab
    EXAMPLE_16 as ab
    EXAMPLE_17 1
    EXAMPLE_17 2
    EXAMPLE_17 3
    EXAMPLE_17 order by a* as a
    EXAMPLE_18 1
    EXAMPLE_18 2
    EXAMPLE_18 3
    EXAMPLE_18 order by ab as ab
    EXAMPLE_19 1
    EXAMPLE_19 2
    EXAMPLE_19 3
    EXAMPLE_19 group by a* as a
    EXAMPLE_20 1
    EXAMPLE_20 2
    EXAMPLE_20 3
    EXAMPLE_20 order by ab as ab
    EXAMPLE_21 0
    EXAMPLE_21 0
    EXAMPLE_22 *
    EXAMPLE_22 *
    EXAMPLE_22 as a
    EXAMPLE_23 *
    EXAMPLE_23 a*
    EXAMPLE_23 as a
    EXAMPLE_24 *
    EXAMPLE_24 ab
    EXAMPLE_24 as ab
    EXAMPLE_25 c*
    EXAMPLE_25 *
    EXAMPLE_25 as a
    EXAMPLE_25 as c
    EXAMPLE_26 c*
    EXAMPLE_26 a*
    EXAMPLE_26 as a
    EXAMPLE_26 as c
    EXAMPLE_27 c*
    EXAMPLE_27 ab
    EXAMPLE_27 as ab
    EXAMPLE_27 as c
    EXAMPLE_28 cd
    EXAMPLE_28 *
    EXAMPLE_28 as a
    EXAMPLE_28 as cd
    EXAMPLE_29 cd
    EXAMPLE_29 a*
    EXAMPLE_29 as a
    EXAMPLE_29 as cd
    EXAMPLE_30 cd
    EXAMPLE_30 ab
    EXAMPLE_30 as ab
    EXAMPLE_30 as cd
    EXAMPLE_31 view
    EXAMPLE_31 2
    EXAMPLE_31 3
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"UNLIST"
    -At line 5, column 48
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"UNLIST"
    -At line 1, column 8
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"UNLIST"
    -At line 1, column 8
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"A"."UNLIST"
    -At line 1, column 8
    Statement failed, SQLSTATE = 42S22
    Dynamic SQL Error
    -SQL error code = -206
    -Column unknown
    -"A"."UNLIST"
    -At line 1, column 8
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
