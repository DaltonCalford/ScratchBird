#coding:utf-8

"""
ID:          index.upper-bound-asc-2-segments
TITLE:       ASC 2-segment index upper bound
DESCRIPTION:
  Check if all 5 values are fetched with "equals" operator over first segment and
  "lower than or equal" operator on second segment. 2 values are bound to the upper
  segments and 1 value is bound to the lower segments.
FBTEST:      functional.arno.indices.upper_bound_asc_02_segments_01
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE TABLE Table_2_10 (
      F1 INTEGER,
      F2 INTEGER
    );

    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 1);
    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 2);
    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 3);
    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 4);
    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 5);
    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 6);
    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 7);
    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 8);
    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 9);
    INSERT INTO Table_2_10 (F1, F2) VALUES (1, 10);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 1);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 2);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 3);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 4);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 5);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 6);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 7);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 8);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 9);
    INSERT INTO Table_2_10 (F1, F2) VALUES (2, 10);
    commit;

    CREATE ASC INDEX I_Table_2_10_ASC ON Table_2_10 (F1, F2);
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set plan on;
    select
      t.f1,
      t.f2
    from table_2_10 t
    where t.f1 = 2 and t.f2 <= 5;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_TEST_NAME = 'T' if act.is_version('<6') else '"T"'
    INDEX_TEST_NAME = 'I_TABLE_2_10_ASC' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"I_TABLE_2_10_ASC"'
    expected_stdout = f"""
        PLAN ({TABLE_TEST_NAME} INDEX ({INDEX_TEST_NAME}))
        F1                              2
        F2                              1
        F1                              2
        F2                              2
        F1                              2
        F2                              3
        F1                              2
        F2                              4
        F1                              2
        F2                              5
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
