#coding:utf-8

"""
ID:          index.lower-bound-asc-2-segments
TITLE:       ASC 2-segment index lower bound
DESCRIPTION:
  Check if all 5 values are fetched with "equals" operator over first segment and
  "greater than or equal" operator on second segment. 2 values are bound to the upper
  segments and 1 value is bound to the lower segment.
FBTEST:      functional.arno.indices.lower_bound_asc_02_segments_01
"""

import pytest
from firebird.qa import *

init_script = """
    create table test (
      f1 integer,
      f2 integer
    );

    insert into test (f1, f2) values (1, 1);
    insert into test (f1, f2) values (1, 2);
    insert into test (f1, f2) values (1, 3);
    insert into test (f1, f2) values (1, 4);
    insert into test (f1, f2) values (1, 5);
    insert into test (f1, f2) values (1, 6);
    insert into test (f1, f2) values (1, 7);
    insert into test (f1, f2) values (1, 8);
    insert into test (f1, f2) values (1, 9);
    insert into test (f1, f2) values (1, 10);
    insert into test (f1, f2) values (2, 1);
    insert into test (f1, f2) values (2, 2);
    insert into test (f1, f2) values (2, 3);
    insert into test (f1, f2) values (2, 4);
    insert into test (f1, f2) values (2, 5);
    insert into test (f1, f2) values (2, 6);
    insert into test (f1, f2) values (2, 7);
    insert into test (f1, f2) values (2, 8);
    insert into test (f1, f2) values (2, 9);
    insert into test (f1, f2) values (2, 10);
    commit;

    create asc index test_idx on test (f1, f2);
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set plan on;
    select t.f1, t.f2 from test t where t.f1 = 2 and t.f2 >= 6;
"""


substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_TEST_NAME = 'T' if act.is_version('<6') else '"T"'
    INDEX_TEST_NAME = 'TEST_IDX' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST_IDX"'
    expected_stdout = f"""
        PLAN ({TABLE_TEST_NAME} INDEX ({INDEX_TEST_NAME}))
        F1 2
        F2 6
        F1 2
        F2 7
        F1 2
        F2 8
        F1 2
        F2 9
        F1 2
        F2 10
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
