#coding:utf-8

"""
ID:          issue-4530
ISSUE:       4530
TITLE:       ISQL -x does not output the START WITH clause of generators/sequences
DESCRIPTION:
JIRA:        CORE-4205
FBTEST:      bugs.core_4205
NOTES:
    [29.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate sequence tmp_gen_42051 start with 9223372036854775807 increment by -2147483647;
    recreate sequence tmp_gen_42052 start with -9223372036854775808 increment by 2147483647;
    recreate sequence tmp_gen_42053 start with 9223372036854775807 increment by  2147483647;
    recreate sequence tmp_gen_42054 start with -9223372036854775808 increment by -2147483647;
"""

db = db_factory(sql_dialect=3, init=init_script)

act = python_act('db', substitutions=[('^((?!(SQLSTATE|CREATE GENERATOR)).)*$', '')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'PUBLIC.'
    expected_stdout = f"""
        CREATE GENERATOR {SQL_SCHEMA_PREFIX}TMP_GEN_42051 START WITH 9223372036854775807 INCREMENT -2147483647;
        CREATE GENERATOR {SQL_SCHEMA_PREFIX}TMP_GEN_42052 START WITH -9223372036854775808 INCREMENT 2147483647;
        CREATE GENERATOR {SQL_SCHEMA_PREFIX}TMP_GEN_42053 START WITH 9223372036854775807 INCREMENT 2147483647;
        CREATE GENERATOR {SQL_SCHEMA_PREFIX}TMP_GEN_42054 START WITH -9223372036854775808 INCREMENT -2147483647;
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-x'], combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
