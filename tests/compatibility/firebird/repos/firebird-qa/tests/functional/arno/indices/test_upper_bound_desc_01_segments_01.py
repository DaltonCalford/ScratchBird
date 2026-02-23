#coding:utf-8

"""
ID:          index.upper-bound-desc-1-segment-01
TITLE:       DESC single segment index upper bound
DESCRIPTION: Check if all 15 values are fetched with "greater than or equal" operator.
FBTEST:      functional.arno.indices.upper_bound_desc_01_segments_01
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE TABLE test (
      ID INTEGER
    );

    SET TERM ^ ;
    CREATE PROCEDURE PR_FillTable_66
    AS
    DECLARE VARIABLE FillID INTEGER;
    BEGIN
      FillID = 2147483647;
      WHILE (FillID > 0) DO
      BEGIN
        INSERT INTO test (ID) VALUES (:FillID);
        FillID = FillID / 2;
      END
      INSERT INTO test (ID) VALUES (NULL);
      INSERT INTO test (ID) VALUES (0);
      INSERT INTO test (ID) VALUES (NULL);
      FillID = -2147483648;
      WHILE (FillID < 0) DO
      BEGIN
        INSERT INTO test (ID) VALUES (:FillID);
        FillID = FillID / 2;
      END
    END
    ^
    SET TERM ;^
    COMMIT;

    EXECUTE PROCEDURE PR_FillTable_66;
    COMMIT;

    CREATE DESC INDEX TEST_IDX ON test (ID);
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set plan on;
    select t.id from test t where t.id >= 131071;
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
        ID 2147483647
        ID 1073741823
        ID 536870911
        ID 268435455
        ID 134217727
        ID 67108863
        ID 33554431
        ID 16777215
        ID 8388607
        ID 4194303
        ID 2097151
        ID 1048575
        ID 524287
        ID 262143
        ID 131071
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
