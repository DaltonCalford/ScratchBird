#coding:utf-8

"""
ID:          issue-5081
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5081
TITLE:       Command `SHOW TABLE` fails when the table contains field with unicode collation in its DDL
DESCRIPTION:
JIRA:        CORE-4782
FBTEST:      bugs.core_4782
NOTES:
    [30.06.2025] pzotov
    1. Regression was fixed in http://sourceforge.net/p/firebird/code/61521 2015-05-11 15:48:35 +0000
       Confirmed bug on 3.0.0.31828, got:
          Statement failed, SQLSTATE = 22001
          arithmetic exception, numeric overflow, or string truncation
          -string right truncation
          -expected length 7, actual 9
    2. Separated expected output for FB major versions prior/since 6.x.
       No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
       Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

TABLE_DDL = 'create table "ĄČĘĢÆĖŠŚÖÜØ£"("ąčęėįšųūž" varchar(100) character set utf8 collate test_coll)'
test_script = f"""
    -- NB: it was connection charset = UTF8 that causes error, title of ticket should be changed.
    create collation test_coll for utf8 from unicode;
    commit;
    {TABLE_DDL};
    commit;
    show table "ĄČĘĢÆĖŠŚÖÜØ£";
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout_5x = """
    ąčęėįšųūž              VARCHAR(100) CHARACTER SET UTF8 Nullable
    COLLATE TEST_COLL
"""

expected_stdout_6x = """
    Table: PUBLIC."ĄČĘĢÆĖŠŚÖÜØ£"
    "ąčęėįšųūž"            VARCHAR(100) CHARACTER SET SYSTEM.UTF8 COLLATE PUBLIC.TEST_COLL Nullable
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
