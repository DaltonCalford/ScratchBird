#coding:utf-8

"""
ID:          issue-2040
ISSUE:       2040
TITLE:       Some aggregate functions does NOT support NULL-constant in 3-d dialect
DESCRIPTION:
JIRA:        CORE-1619
FBTEST:      bugs.core_1619
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=3)

test_script = """
    set list on;
    select
        avg(null)              as avg_for_null
       ,sum(null)              as sum_for_null
       ,var_samp(null)         as var_samp_for_null
       ,var_pop(null)          as var_pop_for_null
       ,stddev_samp(null)      as stddev_samp_for_null
       ,stddev_pop(null)       as stddev_pop_for_null
       ,covar_samp(null, null) as covar_samp_for_nulls
       ,covar_pop(null, null)  as covar_pop_for_nulls
       ,corr(null, null)       as corr_for_nulls
    from rdb$relations;
"""

act = isql_act('db', test_script)

expected_stdout = """
    AVG_FOR_NULL                    <null>
    SUM_FOR_NULL                    <null>
    VAR_SAMP_FOR_NULL               <null>
    VAR_POP_FOR_NULL                <null>
    STDDEV_SAMP_FOR_NULL            <null>
    STDDEV_POP_FOR_NULL             <null>
    COVAR_SAMP_FOR_NULLS            <null>
    COVAR_POP_FOR_NULLS             <null>
    CORR_FOR_NULLS                  <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

