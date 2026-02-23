#coding:utf-8

"""
ID:          7fc8e5ff25
ISSUE:       https://www.sqlite.org/src/tktview/7fc8e5ff25
TITLE:       INSERT into table with two triggers does not terminate
DESCRIPTION:
NOTES:
    [17.08.2025] pzotov
    Code must terminate with "SQLSTATE = 54001 / Too many concurrent executions ..." followed by
    call stack that contains "At trigger ... line: ... column ...".
    But size of buffer with error message is limited by 1K thus actual number of its lines depends
    on length of trigger and, moreover, presence of SQL schema name (in Fb 6.x).
    Because of that, it was decided to suppress output of such lines.

    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create sequence g;
    create table t0(c0 int, c1 int default 1, primary key (c0, c1));
    insert into t0(c0) select row_number()over()-1 from rdb$types rows 6;
    set term ^;
    create trigger tr1 before delete on t0 as
    begin
        delete from t0 where t0.c1 = 1;
    	insert into t0(c0) select gen_id(g,1)+5 from rdb$types rows 5;
    end
    ^
    create trigger tr0 before insert on t0 as
    begin
    	delete from t0;
    end
    ^
    set term ;^

    set count on;
    insert into t0(c1) select row_number()over()-1 from rdb$types rows 3;
"""

substitutions = [('[ \t]+', ' ')]
# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

substitutions.append( ('(-)?At.*', '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 54001
    Too many concurrent executions of the same request
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
