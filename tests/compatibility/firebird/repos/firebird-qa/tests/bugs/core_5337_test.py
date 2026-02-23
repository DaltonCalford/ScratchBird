#coding:utf-8

"""
ID:          issue-5613
ISSUE:       5613
TITLE:       Regression: The subquery in the insert list expressions ignore the changes made earlier in the same executable block.
DESCRIPTION:
JIRA:        CORE-5337
FBTEST:      bugs.core_5337
NOTES:
    [27.06.2023] pzotov
    Regression detected with 'UPDATE OR INSERT' statement (it was missed in original version of this test), added appropriate EXECUTE BLOCK.
    Checked on 5.0.0.1088 (intermediate build after https://github.com/FirebirdSQL/firebird/commit/421a73ae4bd28dd935d1f668f43f9cc89bcf7fdd )
    Thanks to dimitr.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test (
        id int primary key using index test_id
       ,val int not null
    );

    set term ^;
    execute block as
    begin
      insert into test (id, val) values (1, 111);
      insert into test (id, val) values (2, 2 * (select val from test where id = 1));
    end
    ^
    select * from test ^
    rollback ^
    ---------------------
    execute block as
    begin
      insert into test (id, val) values (3, 333);
    end
    ^
    execute block as
    begin
      update or insert into test(  id
                                  ,val
                                )
                          values(  4
                                  ,3 * (select val from test where id = 3)
                                )
                      matching(id);
    end
    ^
    select * from test ^
    rollback ^
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    VAL                             111
    ID                              2
    VAL                             222

    ID                              3
    VAL                             333
    ID                              4
    VAL                             999
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

