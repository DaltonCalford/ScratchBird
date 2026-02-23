#coding:utf-8

"""
ID:          issue-1552
ISSUE:       1552
TITLE:       Bad optimization -- <procedure> left join <subquery> (or <view>)
DESCRIPTION:
JIRA:        CORE-1130
FBTEST:      bugs.core_1130
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create procedure sp_test returns (r int) as
    begin
        r = 1;
        suspend;
    end
    ^
    set term ;^
    commit;

    set planonly;
    select p.*
    from sp_test p
    left join (select rdb$relation_id from rdb$relations ) r on p.r = r.rdb$relation_id;
"""

substitutions = [('RDB\\$INDEX_\\d+', 'RDB\\$INDEX')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    PLAN JOIN (P NATURAL, R RDB$RELATIONS INDEX (RDB$INDEX_1))
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
