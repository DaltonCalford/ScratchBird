#coding:utf-8

"""
ID:          ebdbadade5
ISSUE:       https://www.sqlite.org/src/tktview/ebdbadade5
TITLE:       LEFT JOIN incorrect when ON clause does not reference right table.
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table currency (
        cur char(3),
        primary key (cur)
    );

    create table exchange (
        cur1 char(3),
        cur2 char(3),
        rate real,
        primary key (cur1, cur2)
    );

    insert into currency (cur) values ('eur');
    insert into currency (cur) values ('gbp');
    insert into currency (cur) values ('usd');

    insert into exchange (cur1, cur2, rate) values ('eur', 'gbp', 0.85);
    insert into exchange (cur1, cur2, rate) values ('gbp', 'eur', 1/0.85);

    set count on;
    select c1.cur cur1, c2.cur cur2, coalesce(self.rate, x.rate) rate
    from currency c1
    cross join currency c2
    left join exchange x
        on x.cur1=c1.cur and x.cur2=c2.cur
    left join (select 1 rate from rdb$database) self
        on c1.cur=c2.cur;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CUR1 eur
    CUR2 eur
    RATE 1
    
    CUR1 eur
    CUR2 gbp
    RATE 0.85000002
    
    CUR1 eur
    CUR2 usd
    RATE <null>
    
    CUR1 gbp
    CUR2 eur
    RATE 1.17
    
    CUR1 gbp
    CUR2 gbp
    RATE 1
    
    CUR1 gbp
    CUR2 usd
    RATE <null>
    
    CUR1 usd
    CUR2 eur
    RATE <null>
    
    CUR1 usd
    CUR2 gbp
    RATE <null>
    
    CUR1 usd
    CUR2 usd
    RATE 1
    Records affected: 9
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
