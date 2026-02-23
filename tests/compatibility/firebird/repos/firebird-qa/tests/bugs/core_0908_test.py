#coding:utf-8

"""
ID:          issue-1307
ISSUE:       1307
TITLE:       Garbage in plan output of complex statement
DESCRIPTION:
    This is unfortunate case. The fix for 2.1 went through several "adjustments" and we've
    get lost in changes. The result is that this was not properly fixed in 2.1 line (server
    doesn't crash, but don't returns the truncated plan as supposed either). Now when 2.1
    line is at 2.1.3 we can hope for proper fix in 2.1.4. It should work as intended in 2.5 line.
JIRA:        CORE-908
FBTEST:      bugs.core_0908
NOTES:
    [23.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create procedure big_plan
      returns (x integer)
    as
    begin
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;

      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;

      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;

      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;

      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;

      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;

      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;

      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;

      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;

      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
      select 1 from rdb$database into :x;
    /*  select 1 from rdb$relations into :x; */
      suspend;
    end ^
    set term ;^
"""

db = db_factory(init=init_script)

test_script = """
    set plan on;
    set list on;
    select * from big_plan ;
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' ')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    PLAN (BIG_PLAN NATURAL)
    X 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

