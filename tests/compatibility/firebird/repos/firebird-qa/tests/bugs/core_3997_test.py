#coding:utf-8

"""
ID:          issue-4329
ISSUE:       4329
TITLE:       Join using RDB$DB_KEY produce NATURAL plan
DESCRIPTION:
JIRA:        CORE-3997
FBTEST:      bugs.core_3997
NOTES:
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t (f integer);
    recreate table t_key (k char(8) character set octets);
    commit;

    set term ^;
    execute block as
      declare i int = 10000;
    begin
     While (i>0) do
      begin
       insert into t values (:i);
       i = i-1;
      end
    end^
    set term ;^
    commit;

    insert into t_key select rdb$db_key from t where f=1;
    commit;

    set planonly;
    select f
    from t join t_key on
    t.rdb$db_key=t_key.k;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN JOIN (T_KEY NATURAL, T INDEX ())
"""

expected_stdout_6x = """
    PLAN JOIN ("PUBLIC"."T_KEY" NATURAL, "PUBLIC"."T" INDEX ())
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
