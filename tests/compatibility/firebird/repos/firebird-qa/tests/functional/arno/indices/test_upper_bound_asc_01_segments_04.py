#coding:utf-8

"""
ID:          index.upper-bound-asc-1-segment-04
TITLE:       ASC single segment index upper bound
DESCRIPTION: Check if all 5 values are fetched with "lower than or equal" operator.
FBTEST:      functional.arno.indices.upper_bound_asc_01_segments_04
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE TABLE test (
      ID VARCHAR(15)
    );

    INSERT INTO test (ID) VALUES (NULL);
    INSERT INTO test (ID) VALUES ('A');
    INSERT INTO test (ID) VALUES ('AA');
    INSERT INTO test (ID) VALUES ('AAA');
    INSERT INTO test (ID) VALUES ('AAAA');
    INSERT INTO test (ID) VALUES ('AAAAB');
    INSERT INTO test (ID) VALUES ('AAAB');
    INSERT INTO test (ID) VALUES ('AAB');
    INSERT INTO test (ID) VALUES ('AB');
    INSERT INTO test (ID) VALUES ('B');
    INSERT INTO test (ID) VALUES ('BA');
    INSERT INTO test (ID) VALUES ('BAA');
    INSERT INTO test (ID) VALUES ('BAAA');
    INSERT INTO test (ID) VALUES ('BAAAA');
    INSERT INTO test (ID) VALUES ('BAAAB');
    COMMIT;

    CREATE ASC INDEX test_idx ON test (ID);
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set plan on;
    select id from test t where t.id < 'AAAB';
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
