#coding:utf-8

"""
ID:          issue-4419
ISSUE:       4419
TITLE:       Incorrect full join result with ROW_NUMBER() Function in CTE
DESCRIPTION:
  We have to avoid usage of current RDB$DB_KEY values because they can change.
  Instead, we have to create tables XTYPES and XFORMATS with data from RDB$-tables
  that were in any empty adtabase created on WI-T3.0.0.30566 Firebird 3.0 Alpha 1.
JIRA:        CORE-4091
FBTEST:      bugs.core_4091
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table rtypes( dbkey char(16) character set octets, xtype smallint );
    recreate table rformats( dbkey char(16) character set octets, xformat smallint );
    commit;

    insert into rtypes(dbkey, xtype) values('0B00000001000000',   14);
    insert into rtypes(dbkey, xtype) values('0B00000002000000',    7);
    insert into rtypes(dbkey, xtype) values('0B00000003000000',    8);
    insert into rtypes(dbkey, xtype) values('0B00000004000000',    9);
    insert into rtypes(dbkey, xtype) values('0B00000005000000',   10);
    insert into rtypes(dbkey, xtype) values('0B00000006000000',   27);
    insert into rtypes(dbkey, xtype) values('0B00000007000000',   35);
    insert into rtypes(dbkey, xtype) values('0B00000008000000',   37);
    insert into rtypes(dbkey, xtype) values('0B00000009000000',  261);
    insert into rtypes(dbkey, xtype) values('0B0000000A000000',   40);
    insert into rtypes(dbkey, xtype) values('0B0000000B000000',   45);
    insert into rtypes(dbkey, xtype) values('0B0000000C000000',   12);
    insert into rtypes(dbkey, xtype) values('0B0000000D000000',   13);
    insert into rtypes(dbkey, xtype) values('0B0000000E000000',   16);
    insert into rtypes(dbkey, xtype) values('0B0000000F000000',   23);
    insert into rtypes(dbkey, xtype) values('0B00000010000000',    0);
    insert into rtypes(dbkey, xtype) values('0B00000011000000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000012000000',    2);
    insert into rtypes(dbkey, xtype) values('0B00000013000000',    3);
    insert into rtypes(dbkey, xtype) values('0B00000014000000',    4);
    insert into rtypes(dbkey, xtype) values('0B00000015000000',    5);
    insert into rtypes(dbkey, xtype) values('0B00000016000000',    6);
    insert into rtypes(dbkey, xtype) values('0B00000017000000',    7);
    insert into rtypes(dbkey, xtype) values('0B00000018000000',    8);
    insert into rtypes(dbkey, xtype) values('0B00000019000000',    9);
    insert into rtypes(dbkey, xtype) values('0B0000001A000000',    0);
    insert into rtypes(dbkey, xtype) values('0B0000001B000000',    1);
    insert into rtypes(dbkey, xtype) values('0B0000001C000000',    0);
    insert into rtypes(dbkey, xtype) values('0B0000001D000000',    1);
    insert into rtypes(dbkey, xtype) values('0B0000001E000000',    2);
    insert into rtypes(dbkey, xtype) values('0B0000001F000000',    3);
    insert into rtypes(dbkey, xtype) values('0B00000020000000',    4);
    insert into rtypes(dbkey, xtype) values('0B00000021000000',    5);
    insert into rtypes(dbkey, xtype) values('0B00000022000000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000023000000',    2);
    insert into rtypes(dbkey, xtype) values('0B00000024000000',    3);
    insert into rtypes(dbkey, xtype) values('0B00000025000000',    4);
    insert into rtypes(dbkey, xtype) values('0B00000026000000',    5);
    insert into rtypes(dbkey, xtype) values('0B00000027000000',    6);
    insert into rtypes(dbkey, xtype) values('0B00000028000000', 8192);
    insert into rtypes(dbkey, xtype) values('0B00000029000000', 8193);
    insert into rtypes(dbkey, xtype) values('0B0000002A000000', 8194);
    insert into rtypes(dbkey, xtype) values('0B0000002B000000', 8195);
    insert into rtypes(dbkey, xtype) values('0B0000002C000000', 8196);
    insert into rtypes(dbkey, xtype) values('0B0000002D000000',    0);
    insert into rtypes(dbkey, xtype) values('0B0000002E000000',    1);
    insert into rtypes(dbkey, xtype) values('0B0000002F000000',    2);
    insert into rtypes(dbkey, xtype) values('0B00000030000000',    3);
    insert into rtypes(dbkey, xtype) values('0B00000031000000',    4);
    insert into rtypes(dbkey, xtype) values('0B000000F0000000',    5);
    insert into rtypes(dbkey, xtype) values('0B000000F1000000',    6);
    insert into rtypes(dbkey, xtype) values('0B000000F2000000',    7);
    insert into rtypes(dbkey, xtype) values('0B000000F3000000',    8);
    insert into rtypes(dbkey, xtype) values('0B000000F4000000',    9);
    insert into rtypes(dbkey, xtype) values('0B000000F5000000',   10);
    insert into rtypes(dbkey, xtype) values('0B000000F6000000',   11);
    insert into rtypes(dbkey, xtype) values('0B000000F7000000',   12);
    insert into rtypes(dbkey, xtype) values('0B000000F8000000',   13);
    insert into rtypes(dbkey, xtype) values('0B000000F9000000',   14);
    insert into rtypes(dbkey, xtype) values('0B000000FA000000',   15);
    insert into rtypes(dbkey, xtype) values('0B000000FB000000',   16);
    insert into rtypes(dbkey, xtype) values('0B000000FC000000',   17);
    insert into rtypes(dbkey, xtype) values('0B000000FD000000',   18);
    insert into rtypes(dbkey, xtype) values('0B000000FE000000',   19);
    insert into rtypes(dbkey, xtype) values('0B000000FF000000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000000010000',    2);
    insert into rtypes(dbkey, xtype) values('0B00000001010000',    3);
    insert into rtypes(dbkey, xtype) values('0B00000002010000',    0);
    insert into rtypes(dbkey, xtype) values('0B00000003010000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000004010000',    2);
    insert into rtypes(dbkey, xtype) values('0B00000005010000',    3);
    insert into rtypes(dbkey, xtype) values('0B00000006010000',    4);
    insert into rtypes(dbkey, xtype) values('0B00000007010000',    5);
    insert into rtypes(dbkey, xtype) values('0B00000008010000',    0);
    insert into rtypes(dbkey, xtype) values('0B00000009010000',    1);
    insert into rtypes(dbkey, xtype) values('0B0000000A010000',    2);
    insert into rtypes(dbkey, xtype) values('0B0000000B010000',    3);
    insert into rtypes(dbkey, xtype) values('0B0000000C010000',    4);
    insert into rtypes(dbkey, xtype) values('0B0000000D010000',    5);
    insert into rtypes(dbkey, xtype) values('0B0000000E010000',    0);
    insert into rtypes(dbkey, xtype) values('0B0000000F010000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000010010000',    2);
    insert into rtypes(dbkey, xtype) values('0B00000011010000',    0);
    insert into rtypes(dbkey, xtype) values('0B00000012010000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000013010000',    0);
    insert into rtypes(dbkey, xtype) values('0B00000014010000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000015010000',    2);
    insert into rtypes(dbkey, xtype) values('0B00000016010000',    0);
    insert into rtypes(dbkey, xtype) values('0B00000017010000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000018010000',    2);
    insert into rtypes(dbkey, xtype) values('0B00000019010000',    3);
    insert into rtypes(dbkey, xtype) values('0B0000001A010000',    0);
    insert into rtypes(dbkey, xtype) values('0B0000001B010000',    1);
    insert into rtypes(dbkey, xtype) values('0B0000001C010000',    2);
    insert into rtypes(dbkey, xtype) values('0B0000001D010000',    3);
    insert into rtypes(dbkey, xtype) values('0B0000001E010000',    0);
    insert into rtypes(dbkey, xtype) values('0B0000001F010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000DF010000',    2);
    insert into rtypes(dbkey, xtype) values('0B000000E0010000',    0);
    insert into rtypes(dbkey, xtype) values('0B000000E1010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000E2010000',    2);
    insert into rtypes(dbkey, xtype) values('0B000000E3010000',    3);
    insert into rtypes(dbkey, xtype) values('0B000000E4010000',    4);
    insert into rtypes(dbkey, xtype) values('0B000000E5010000',    0);
    insert into rtypes(dbkey, xtype) values('0B000000E6010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000E7010000',    0);
    insert into rtypes(dbkey, xtype) values('0B000000E8010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000E9010000',    0);
    insert into rtypes(dbkey, xtype) values('0B000000EA010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000EB010000',    0);
    insert into rtypes(dbkey, xtype) values('0B000000EC010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000ED010000',    0);
    insert into rtypes(dbkey, xtype) values('0B000000EE010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000EF010000',    0);
    insert into rtypes(dbkey, xtype) values('0B000000F0010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000F1010000',    2);
    insert into rtypes(dbkey, xtype) values('0B000000F2010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000F3010000',    2);
    insert into rtypes(dbkey, xtype) values('0B000000F4010000',    3);
    insert into rtypes(dbkey, xtype) values('0B000000F5010000',    4);
    insert into rtypes(dbkey, xtype) values('0B000000F6010000',    5);
    insert into rtypes(dbkey, xtype) values('0B000000F7010000',    6);
    insert into rtypes(dbkey, xtype) values('0B000000F8010000',    7);
    insert into rtypes(dbkey, xtype) values('0B000000F9010000',    8);
    insert into rtypes(dbkey, xtype) values('0B000000FA010000',    9);
    insert into rtypes(dbkey, xtype) values('0B000000FB010000',   10);
    insert into rtypes(dbkey, xtype) values('0B000000FC010000',    0);
    insert into rtypes(dbkey, xtype) values('0B000000FD010000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000FE010000',    0);
    insert into rtypes(dbkey, xtype) values('0B000000FF010000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000000020000',    0);
    insert into rtypes(dbkey, xtype) values('0B00000001020000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000002020000',    0);
    insert into rtypes(dbkey, xtype) values('0B00000003020000',    1);
    insert into rtypes(dbkey, xtype) values('0B00000004020000',    2);
    insert into rtypes(dbkey, xtype) values('0B00000005020000',    3);
    insert into rtypes(dbkey, xtype) values('0B00000006020000',    4);
    insert into rtypes(dbkey, xtype) values('0B00000007020000',    5);
    insert into rtypes(dbkey, xtype) values('0B00000008020000',    6);
    insert into rtypes(dbkey, xtype) values('0B00000009020000',   10);
    insert into rtypes(dbkey, xtype) values('0B0000000A020000',   11);
    insert into rtypes(dbkey, xtype) values('0B0000000B020000',   12);
    insert into rtypes(dbkey, xtype) values('0B0000000C020000',   21);
    insert into rtypes(dbkey, xtype) values('0B0000000D020000',   22);
    insert into rtypes(dbkey, xtype) values('0B0000000E020000',   23);
    insert into rtypes(dbkey, xtype) values('0B000000CE020000',   34);
    insert into rtypes(dbkey, xtype) values('0B000000CF020000',   35);
    insert into rtypes(dbkey, xtype) values('0B000000D0020000',   36);
    insert into rtypes(dbkey, xtype) values('0B000000D1020000',   37);
    insert into rtypes(dbkey, xtype) values('0B000000D2020000',   38);
    insert into rtypes(dbkey, xtype) values('0B000000D3020000',   39);
    insert into rtypes(dbkey, xtype) values('0B000000D4020000',   40);
    insert into rtypes(dbkey, xtype) values('0B000000D5020000',   45);
    insert into rtypes(dbkey, xtype) values('0B000000D6020000',   46);
    insert into rtypes(dbkey, xtype) values('0B000000D7020000',   13);
    insert into rtypes(dbkey, xtype) values('0B000000D8020000',   47);
    insert into rtypes(dbkey, xtype) values('0B000000D9020000',   14);
    insert into rtypes(dbkey, xtype) values('0B000000DA020000',   50);
    insert into rtypes(dbkey, xtype) values('0B000000DB020000',    9);
    insert into rtypes(dbkey, xtype) values('0B000000DC020000',   15);
    insert into rtypes(dbkey, xtype) values('0B000000DD020000',   16);
    insert into rtypes(dbkey, xtype) values('0B000000DE020000',   17);
    insert into rtypes(dbkey, xtype) values('0B000000DF020000',   18);
    insert into rtypes(dbkey, xtype) values('0B000000E0020000',   48);
    insert into rtypes(dbkey, xtype) values('0B000000E1020000',   49);
    insert into rtypes(dbkey, xtype) values('0B000000E2020000',   51);
    insert into rtypes(dbkey, xtype) values('0B000000E3020000',   52);
    insert into rtypes(dbkey, xtype) values('0B000000E4020000',   53);
    insert into rtypes(dbkey, xtype) values('0B000000E5020000',   54);
    insert into rtypes(dbkey, xtype) values('0B000000E6020000',   55);
    insert into rtypes(dbkey, xtype) values('0B000000E7020000',   19);
    insert into rtypes(dbkey, xtype) values('0B000000E8020000',   58);
    insert into rtypes(dbkey, xtype) values('0B000000E9020000',   59);
    insert into rtypes(dbkey, xtype) values('0B000000EA020000',   60);
    insert into rtypes(dbkey, xtype) values('0B000000EB020000',   44);
    insert into rtypes(dbkey, xtype) values('0B000000EC020000',   56);
    insert into rtypes(dbkey, xtype) values('0B000000ED020000',   57);
    insert into rtypes(dbkey, xtype) values('0B000000EE020000',   63);
    insert into rtypes(dbkey, xtype) values('0B000000EF020000',   64);
    insert into rtypes(dbkey, xtype) values('0B000000F0020000',   65);
    insert into rtypes(dbkey, xtype) values('0B000000F1020000',   66);
    insert into rtypes(dbkey, xtype) values('0B000000F2020000',   67);
    insert into rtypes(dbkey, xtype) values('0B000000F3020000',   68);
    insert into rtypes(dbkey, xtype) values('0B000000F4020000',   69);
    insert into rtypes(dbkey, xtype) values('0B000000F5020000',    1);
    insert into rtypes(dbkey, xtype) values('0B000000F6020000',    2);
    insert into rtypes(dbkey, xtype) values('0B000000F7020000',    2);
    insert into rtypes(dbkey, xtype) values('0B000000F8020000',    3);
    insert into rtypes(dbkey, xtype) values('0B000000F9020000',    3);
    insert into rtypes(dbkey, xtype) values('0B000000FA020000',    4);
    insert into rtypes(dbkey, xtype) values('0B000000FB020000',    5);
    insert into rtypes(dbkey, xtype) values('0B000000FC020000',    6);
    insert into rtypes(dbkey, xtype) values('0B000000BD030000',   10);
    insert into rtypes(dbkey, xtype) values('0B000000BE030000',   11);
    insert into rtypes(dbkey, xtype) values('0B000000BF030000',   12);
    insert into rtypes(dbkey, xtype) values('0B000000C0030000',   21);
    insert into rtypes(dbkey, xtype) values('0B000000C1030000',   21);
    insert into rtypes(dbkey, xtype) values('0B000000C2030000',   21);
    insert into rtypes(dbkey, xtype) values('0B000000C3030000',   22);
    insert into rtypes(dbkey, xtype) values('0B000000C4030000',   22);
    insert into rtypes(dbkey, xtype) values('0B000000C5030000',   22);
    insert into rtypes(dbkey, xtype) values('0B000000C6030000',   23);
    insert into rtypes(dbkey, xtype) values('0B000000C7030000',   23);
    insert into rtypes(dbkey, xtype) values('0B000000C8030000',   23);
    insert into rtypes(dbkey, xtype) values('0B000000C9030000',   34);
    insert into rtypes(dbkey, xtype) values('0B000000CA030000',   34);
    insert into rtypes(dbkey, xtype) values('0B000000CB030000',   34);
    insert into rtypes(dbkey, xtype) values('0B000000CC030000',   35);
    insert into rtypes(dbkey, xtype) values('0B000000CD030000',   35);
    insert into rtypes(dbkey, xtype) values('0B000000CE030000',   36);
    insert into rtypes(dbkey, xtype) values('0B000000CF030000',   36);
    insert into rtypes(dbkey, xtype) values('0B000000D0030000',   37);
    insert into rtypes(dbkey, xtype) values('0B000000D1030000',   37);
    insert into rtypes(dbkey, xtype) values('0B000000D2030000',   38);
    insert into rtypes(dbkey, xtype) values('0B000000D3030000',   38);
    insert into rtypes(dbkey, xtype) values('0B000000D4030000',   39);
    insert into rtypes(dbkey, xtype) values('0B000000D5030000',   39);
    insert into rtypes(dbkey, xtype) values('0B000000D6030000',   39);
    insert into rtypes(dbkey, xtype) values('0B000000D7030000',   40);
    insert into rtypes(dbkey, xtype) values('0B000000D8030000',   40);
    insert into rtypes(dbkey, xtype) values('0B000000D9030000',   40);
    insert into rtypes(dbkey, xtype) values('0B000000DA030000',   45);
    insert into rtypes(dbkey, xtype) values('0B000000DB030000',   46);
    insert into rtypes(dbkey, xtype) values('0B000000DC030000',   13);
    insert into rtypes(dbkey, xtype) values('0B000000DD030000',   47);
    insert into rtypes(dbkey, xtype) values('0B000000DE030000',   14);
    insert into rtypes(dbkey, xtype) values('0B000000DF030000',    9);
    insert into rtypes(dbkey, xtype) values('0B000000E0030000',   15);
    insert into rtypes(dbkey, xtype) values('0B000000E1030000',   16);
    insert into rtypes(dbkey, xtype) values('0B000000E2030000',   17);
    insert into rtypes(dbkey, xtype) values('0B000000E3030000',   18);
    insert into rtypes(dbkey, xtype) values('0B000000E4030000',   48);
    insert into rtypes(dbkey, xtype) values('0B000000E5030000',   49);
    insert into rtypes(dbkey, xtype) values('0B000000E6030000',   51);
    insert into rtypes(dbkey, xtype) values('0B000000E7030000',   52);
    insert into rtypes(dbkey, xtype) values('0B000000E8030000',   53);
    insert into rtypes(dbkey, xtype) values('0B000000E9030000',   54);
    insert into rtypes(dbkey, xtype) values('0B000000EA030000',   55);
    insert into rtypes(dbkey, xtype) values('0B000000AC040000',   58);
    insert into rtypes(dbkey, xtype) values('0B000000AD040000',   59);
    insert into rtypes(dbkey, xtype) values('0B000000AE040000',   60);
    insert into rtypes(dbkey, xtype) values('0B000000AF040000',   65);
    insert into rtypes(dbkey, xtype) values('0B000000B0040000',   44);
    insert into rtypes(dbkey, xtype) values('0B000000B1040000',   44);
    insert into rtypes(dbkey, xtype) values('0B000000B2040000',   44);
    insert into rtypes(dbkey, xtype) values('0B000000B3040000',   56);
    insert into rtypes(dbkey, xtype) values('0B000000B4040000',   56);
    insert into rtypes(dbkey, xtype) values('0B000000B5040000',   56);
    insert into rtypes(dbkey, xtype) values('0B000000B6040000',   57);
    insert into rtypes(dbkey, xtype) values('0B000000B7040000',   57);
    insert into rtypes(dbkey, xtype) values('0B000000B8040000',   57);
    commit;

    insert into rformats(dbkey, xformat) values('08000000F0000000',   1);
    insert into rformats(dbkey, xformat) values('08000000F1000000',   1);
    commit;

    set count on;
    with A as (
        select t.xtype, row_number()over(order by t.dbkey) as RN from rtypes t
    ),
    B as (
        select f.xformat, row_number()over(order by f.dbkey) as RN from rformats f
    )
    select a.xtype as "rtypes.type", a.rn as "rtypes.rn", b.rn as "rformats.rn", b.xformat as "rformats.format"
    from a
    full outer join b on a.rn = b.rn
    where b.xformat is not null
    order by 1,2,3,4
    ;
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

expected_stdout = """
    rtypes.type             rtypes.rn           rformats.rn rformats.format
    =========== ===================== ===================== ===============
    0                     2                     2               1
    2                     1                     1               1
    Records affected: 2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

