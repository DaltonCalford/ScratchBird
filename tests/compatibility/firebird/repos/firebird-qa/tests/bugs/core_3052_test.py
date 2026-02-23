#coding:utf-8

"""
ID:          issue-3432
ISSUE:       3432
TITLE:       Wrong resultset
DESCRIPTION:
  Empty rowset when selecting from table with compound index on PXW_HUNDC-collated fields
JIRA:        CORE-3052
FBTEST:      bugs.core_3052
"""

import pytest
from firebird.qa import *

db = db_factory(charset='WIN1250')

test_script = """
    -- NB: do NOT downgrate minimal version to 2.5 - at least for 2.5.4.26857
    -- following queries return zero rows.

    create domain xvar10n as varchar(160) character set WIN1250 not null collate PXW_HUNDC;
    create domain xint as int;
    commit;

    create table tmp_test (
        m1 xvar10n
       ,m2 xvar10n
       ,val xint
    );
    commit;

    alter table tmp_test add constraint tmp_test_uk1 unique (m1, m2);
    commit;

    insert into tmp_test (m1, m2, val) values ('A', 'C1', 1);
    insert into tmp_test (m1, m2, val) values ('A', 'C2', 2);
    insert into tmp_test (m1, m2, val) values ('A', 'D2', 3);
    insert into tmp_test (m1, m2, val) values ('A', 'M3', 3);

    set list on;
    select *
    from tmp_test te
    where te.m1 = 'A' and te.m2 like 'C%';

    select *
    from tmp_test te
    where te.m1 = 'A' and te.m2 like 'D%';
"""

act = isql_act('db', test_script)

expected_stdout = """
    M1                              A
    M2                              C1
    VAL                             1

    M1                              A
    M2                              C2
    VAL                             2

    M1                              A
    M2                              D2
    VAL                             3
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

