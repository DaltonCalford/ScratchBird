#coding:utf-8

"""
ID:          issue-4280
ISSUE:       4280
TITLE:       Wrong results when the column with collation using option (NUMERIC-SORT=1) is in where clause
DESCRIPTION:
  Changing in 3.0, comment in tracker 05/Oct/12 04:52: NUMERIC-SORT UNIQUE indexes will
  not be usable for ORDER, only for lookups.
JIRA:        CORE-3947
FBTEST:      bugs.core_3947
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    -- See also: sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1093394&msg=15987488
    create collation coll_ns for utf8 from unicode 'NUMERIC-SORT=1';
    commit;
    create domain dm_ns as varchar(10) character set utf8 collate coll_ns;
    commit;

    recreate table t(
       s1 dm_ns
      ,s2 dm_ns
      ,s3 dm_ns
    ); commit;

    create index t_s1_non_unq on t(s1);
    create UNIQUE index t_s2_unq_asc on t(s2);
    create UNIQUE DESCENDING index t_s3_unq_des on t(s3);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set plan on;
    select * from t order by s1;
    select * from t order by s2;
    select * from t order by s3 desc;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN (T ORDER T_S1_NON_UNQ)
    PLAN SORT (T NATURAL)
    PLAN SORT (T NATURAL)
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."T" ORDER "PUBLIC"."T_S1_NON_UNQ")
    PLAN SORT ("PUBLIC"."T" NATURAL)
    PLAN SORT ("PUBLIC"."T" NATURAL)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
