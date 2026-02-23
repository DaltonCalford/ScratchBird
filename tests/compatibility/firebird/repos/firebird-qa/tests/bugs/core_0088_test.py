#coding:utf-8

"""
ID:          issue-413
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/413
TITLE:       Join on diffrent datatypes
DESCRIPTION:
JIRA:        CORE-88
FBTEST:      bugs.core_0088
NOTES:
    [23.03.2025] pzotov
    Removed PLAN output because it differs on 6.x vs previous versions since commit fc12c0ef39
    ("Unnest IN/ANY/EXISTS subqueries and optimize them using semi-join algorithm (#8061)").
    This test must check only returned data.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test1 (
        id integer not null primary key,
        snum char(10) unique using index test1_snum_unq
    );

    recreate table test2 (
        id integer not null primary key,
        inum numeric(15,2) unique using index test2_inum_unq
    );
    commit;

    insert into test1 (id, snum) values (1, '01');
    insert into test1 (id, snum) values (2, '02');
    insert into test1 (id, snum) values (3, '03');
    insert into test1 (id, snum) values (5, '05');
    commit;

    insert into test2 (id, inum) values (1, 1);
    insert into test2 (id, inum) values (2, 2);
    insert into test2 (id, inum) values (3, 3);
    insert into test2 (id, inum) values (4, 4);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set count on;
    select * from test2 where inum not in (select snum from test1) order by id;
    select * from test2 where inum in (select snum from test1) order by id;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    expected_stdout = """
        ID                              4
        INUM                            4.00
        Records affected: 1

        ID                              1
        INUM                            1.00
        ID                              2
        INUM                            2.00
        ID                              3
        INUM                            3.00
        Records affected: 3
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
