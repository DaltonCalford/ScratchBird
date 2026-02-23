#coding:utf-8

"""
ID:          issue-6646
ISSUE:       6646
TITLE:       RETURNING clause in MERGE cannot reference column in aliased target table
  using qualified reference (alias.column) if DELETE action present
DESCRIPTION:
JIRA:        CORE-6408
FBTEST:      bugs.core_6408
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table dummy2 (
      id integer constraint pk_dummy2 primary key,
      val varchar(50)
    );
    commit;
    insert into dummy2 (id) values (1);
    commit;

    merge into dummy2 as d
      using (select 1, 'ab' from rdb$database) as src(id, val)
      on d.id = src.id
      when matched and d.id = 2 then delete
      when matched then update set d.val = src.val
      returning
           d.val ----  this was not allowed before fix ("Statement failed, SQLSTATE = 42S22 ... Column unknown")
          ,new.val
          ,old.val
          ,src.val
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    VAL                             ab
    CONSTANT                        ab
    VAL                             <null>
    VAL                             ab
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
