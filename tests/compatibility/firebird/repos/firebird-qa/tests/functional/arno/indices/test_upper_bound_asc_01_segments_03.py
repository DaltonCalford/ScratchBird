#coding:utf-8

"""
ID:          index.upper-bound-asc-1-segment-03
TITLE:       ASC single segment index upper bound
DESCRIPTION: Check if all 5 values are fetched with "lower than or equal" operator.
FBTEST:      functional.arno.indices.upper_bound_asc_01_segments_03
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE TABLE TEST (
      ID VARCHAR(15)
    );

    INSERT INTO TEST (ID) VALUES (NULL);
    INSERT INTO TEST (ID) VALUES ('A');
    INSERT INTO TEST (ID) VALUES ('AA');
    INSERT INTO TEST (ID) VALUES ('AAA');
    INSERT INTO TEST (ID) VALUES ('AAAA');
    INSERT INTO TEST (ID) VALUES ('AAAAB');
    INSERT INTO TEST (ID) VALUES ('AAAB');
    INSERT INTO TEST (ID) VALUES ('AAB');
    INSERT INTO TEST (ID) VALUES ('AB');
    INSERT INTO TEST (ID) VALUES ('B');
    INSERT INTO TEST (ID) VALUES ('BA');
    INSERT INTO TEST (ID) VALUES ('BAA');
    INSERT INTO TEST (ID) VALUES ('BAAA');
    INSERT INTO TEST (ID) VALUES ('BAAAA');
    INSERT INTO TEST (ID) VALUES ('BAAAB');
    COMMIT;

    CREATE ASC INDEX TEST_IDX ON TEST (ID);
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set plan on;
    select id from test t where t.id <= 'AAAAB';
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
        ID A
        ID AA
        ID AAA
        ID AAAA
        ID AAAAB
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
