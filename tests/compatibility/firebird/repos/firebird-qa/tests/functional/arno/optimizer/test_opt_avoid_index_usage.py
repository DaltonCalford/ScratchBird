#coding:utf-8

"""
ID:          optimizer.avoid-index-usage
ISSUE:       3431
TITLE:       AVOID index usage in WHERE <indexed_varchar_field> = <integer_value>
DESCRIPTION:
  Samples here are from #3431.
  Confirmed usage 'PLAN INDEX ...' in FB 2.0.0.12724
JIRA:        CORE-3051
FBTEST:      functional.arno.optimizer.opt_avoid_index_usage
NOTES:
    [07.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.914; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t(x varchar(10), y varchar(10));
    create index t_x_asc on t(x);
    create descending index t_y_desc on t(y);
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    set planonly;
    select * from t where x = 0;
    select * from t where y = 0;
    select * from t where x > 0;
    select * from t where y < 0;
    select * from t where x between 0 and 1;
    select * from t where y between 0 and 1;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    expected_stdout_5x = """
        PLAN (T NATURAL)
        PLAN (T NATURAL)
        PLAN (T NATURAL)
        PLAN (T NATURAL)
        PLAN (T NATURAL)
        PLAN (T NATURAL)
    """
    expected_stdout_6x = """
        PLAN ("PUBLIC"."T" NATURAL)
        PLAN ("PUBLIC"."T" NATURAL)
        PLAN ("PUBLIC"."T" NATURAL)
        PLAN ("PUBLIC"."T" NATURAL)
        PLAN ("PUBLIC"."T" NATURAL)
        PLAN ("PUBLIC"."T" NATURAL)
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
