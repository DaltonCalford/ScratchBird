#coding:utf-8

"""
ID:          dml.update-or-insert-03
FBTEST:      functional.dml.update_or_insert.03
TITLE:       UPDATE OR INSERT
DESCRIPTION: MATCHING clause
NOTES:
    [08.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.930; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory(init="CREATE TABLE TMPTEST_NOKEY ( id INTEGER , name VARCHAR(20));")

test_script = """
    SET LIST ON;
    UPDATE OR INSERT INTO TMPTEST_NOKEY(id, name) VALUES (1,'ivan' )
    MATCHING (id);

    select name from TMPTEST_NOKEY where id =1;

    UPDATE OR INSERT INTO TMPTEST_NOKEY(id, name) VALUES (1,'bob' )
    MATCHING (id);

    select name from TMPTEST_NOKEY where id =1;
    UPDATE OR INSERT INTO TMPTEST_NOKEY(id, name) VALUES (1,'ivan' );
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    TEST_TABLE_NAME = 'TMPTEST_NOKEY' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TMPTEST_NOKEY"'
    expected_stdout = f"""
        NAME                            ivan
        NAME                            bob
        Statement failed, SQLSTATE = 22000
        Dynamic SQL Error
        -Primary key required on table {TEST_TABLE_NAME}
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
