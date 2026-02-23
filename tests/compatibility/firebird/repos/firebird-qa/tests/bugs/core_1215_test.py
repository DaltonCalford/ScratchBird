#coding:utf-8

"""
ID:          issue-1639
ISSUE:       1639
TITLE:       Wrong SELECT query results using index to evaluate >= condition
DESCRIPTION:
JIRA:        CORE-1215
FBTEST:      bugs.core_1215
NOTES:
    [24.06.2025] pzotov
    Separated execution plans for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema name and quotes to enclosing object names.
    Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.858; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    CREATE TABLE T (ID INT);
    COMMIT;

    set term ^;

    EXECUTE BLOCK AS
    DECLARE I INT = 0;
    BEGIN
      WHILE (I < 50000) DO
      BEGIN
        INSERT INTO T VALUES (1);
        I = I + 1;
      END
    END^

    set term ;^
    commit;

    CREATE INDEX IDX_T ON T (ID);
    COMMIT;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set plan on;
    SELECT COUNT(*) FROM T ;
    SELECT COUNT(*) FROM T WHERE ID >= 1 ;
    SELECT COUNT(*) FROM T WHERE ID = 1 ;
    SELECT COUNT(*) FROM T WHERE ID <= 1 ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN (T NATURAL)
    COUNT 50000
    PLAN (T INDEX (IDX_T))
    COUNT 50000
    PLAN (T INDEX (IDX_T))
    COUNT 50000
    PLAN (T INDEX (IDX_T))
    COUNT 50000
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."T" NATURAL)
    COUNT 50000
    PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."IDX_T"))
    COUNT 50000
    PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."IDX_T"))
    COUNT 50000
    PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."IDX_T"))
    COUNT 50000
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

