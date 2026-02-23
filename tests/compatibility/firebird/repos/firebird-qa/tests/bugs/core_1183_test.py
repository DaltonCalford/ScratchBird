#coding:utf-8

"""
ID:          issue-1609
ISSUE:       1609
TITLE:       View cannot be created if its WHERE clause contains IN <subquery> with a procedure reference
DESCRIPTION:
JIRA:        CORE-1183
FBTEST:      bugs.core_1183
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure p
      returns (col int)
    as
    begin
      col = 1;
      suspend;
    end^
    set term ;^
    commit;

    create or alter view v
    as
      select
          rdb$description v_descr,
          rdb$relation_id v_rel_id,
          rdb$character_set_name v_cset_name
      from rdb$database
      where 1 in ( select col from p );
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select v_descr, sign(v_rel_id) as v_rel_id, v_cset_name
    from v;
"""

act = isql_act('db', test_script)

expected_stdout = """
    V_DESCR                         <null>
    V_REL_ID                        1
    V_CSET_NAME                     NONE
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

