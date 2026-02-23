#coding:utf-8

"""
ID:          issue-5417
ISSUE:       5417
TITLE:       ALTER SEQUENCE RESTART WITH does not change the initial value
DESCRIPTION:
NOTES:
[18.08.2020]
  FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
  statement 'CREATE SEQUENCE <G>' will create generator with current value LESS FOR 1 then it was before.
  Thus, 'create sequence g;' followed by 'show sequence;' will output "current value: -1" (!!) rather than 0.
  See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d

  This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt
  Because of this, it was decided create separate section for FB 4.x.
JIRA:        CORE-5133
FBTEST:      bugs.core_5133
"""

import pytest
from firebird.qa import *

db_1 = db_factory()

test_script = """
    create or alter view v_gen as select 1 id from rdb$database;
    commit;

    recreate sequence s1 start with 100 increment 2;
    create or alter view v_gen as
    select
        trim(rdb$generator_name) gen_name
        ,gen_id(s1, 0) as generator_current_value
        ,rdb$initial_value gen_init
        ,rdb$generator_increment gen_incr
    from rdb$generators
    where rdb$system_flag<>1;
    commit;


    set list on;
    select * from v_gen;

    alter sequence s1 restart with -1 increment -3;
    commit;

    select * from v_gen;
"""

act = isql_act('db_1', test_script)

# version: 3.0

expected_stdout_1 = """
    GEN_NAME                        S1
    GENERATOR_CURRENT_VALUE         100
    GEN_INIT                        100
    GEN_INCR                        2
    GEN_NAME                        S1
    GENERATOR_CURRENT_VALUE         -1
    GEN_INIT                        -1
    GEN_INCR                        -3
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

expected_stdout_2 = """
    GEN_NAME                        S1
    GENERATOR_CURRENT_VALUE         98
    GEN_INIT                        100
    GEN_INCR                        2
    GEN_NAME                        S1
    GENERATOR_CURRENT_VALUE         2
    GEN_INIT                        100
    GEN_INCR                        -3
"""

@pytest.mark.version('>=4.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

