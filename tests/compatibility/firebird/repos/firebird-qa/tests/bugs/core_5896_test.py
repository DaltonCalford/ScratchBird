#coding:utf-8

"""
ID:          issue-6154
ISSUE:       6154
TITLE:       NOT NULL constraint is not synchronized after rename column
DESCRIPTION:
NOTES:
[26.08.2018]
  Added check of rdb$relation_fields.rdb$null_flag after suggestion by Adriano, 26.08.2018 19:12.
JIRA:        CORE-5896
FBTEST:      bugs.core_5896
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set count on;
    set list on;


    create or alter view v_chk as
        select
             --cc.rdb$constraint_name constr_name
             rc.rdb$relation_name rel_name
            ,cc.rdb$trigger_name trg_name
            ,rf.rdb$null_flag null_flag
          from rdb$check_constraints cc
               join rdb$relation_constraints rc on cc.rdb$constraint_name = rc.rdb$constraint_name
               left join rdb$relation_fields rf
                  on rc.rdb$relation_name = rf.rdb$relation_name
                 and cc.rdb$trigger_name = rf.rdb$field_name
         where rc.rdb$constraint_type = upper('not null')
    ;
    commit;

    recreate table test (
        old_name bigint not null
    );
    commit;

    select * from v_chk;
    commit;

    alter table test alter old_name to new_name;
    commit;

    select * from v_chk;

    -- Output BEFORE fix was:
    -------------------------
    -- REL_NAME                        TEST
    -- TRG_NAME                        OLD_NAME
    -- NULL_FLAG                       <null>
    commit;

"""

act = isql_act('db', test_script)

expected_stdout = """
    REL_NAME                        TEST
    TRG_NAME                        OLD_NAME
    NULL_FLAG                       1
    Records affected: 1

    REL_NAME                        TEST
    TRG_NAME                        NEW_NAME
    NULL_FLAG                       1
    Records affected: 1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
