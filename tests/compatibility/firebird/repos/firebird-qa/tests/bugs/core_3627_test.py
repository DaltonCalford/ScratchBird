#coding:utf-8

"""
ID:          issue-3979
ISSUE:       3979
TITLE:       Server crashes with access violation when inserting row into table with unique index
DESCRIPTION:
JIRA:        CORE-3627
FBTEST:      bugs.core_3627
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Confirmed crash on WI-V2.5.1.26351:
    -- Statement failed, SQLSTATE = 08006
    -- Unable to complete network request to host "localhost".
    -- -Error reading data from the connection.

    recreate table testtable
    (
      "ID" integer not null,
      "CLASSID" integer,
      primary key ("ID")
    );
    insert into TestTable values(1, 1);
    insert into TestTable values(2, 2);
    commit;

    alter table testtable add ksgfk integer;
    create unique index classidksgidx on testtable (classid, ksgfk);

    insert into testtable values(3,1,null);
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 23000
    attempt to store duplicate value (visible to active transactions) in unique index "CLASSIDKSGIDX"
    -Problematic key value is ("CLASSID" = 1, "KSGFK" = NULL)
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 23000
    attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."CLASSIDKSGIDX"
    -Problematic key value is ("CLASSID" = 1, "KSGFK" = NULL)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
