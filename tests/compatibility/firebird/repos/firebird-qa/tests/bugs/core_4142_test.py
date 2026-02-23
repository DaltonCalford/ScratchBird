#coding:utf-8

"""
ID:          issue-4469
ISSUE:       4469
TITLE:       Regression: Server crashes while preparing a query with DISTINCT and ORDER BY
DESCRIPTION:
JIRA:        CORE-4142
FBTEST:      bugs.core_4142
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table trel(id smallint, name char(31) character set unicode_fss collate unicode_fss);
    commit;
    insert into trel(id, name) select row_number()over(), 'RDB$NAME_' || row_number()over()
    from rdb$types
    rows 9;
    commit;
    alter table trel add constraint trel_name_unq unique (name);
    create index trel_id on trel(id);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    select distinct id + 0 id, name
    from trel
    order by name;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
    ID NAME
    == ==========
     1 RDB$NAME_1
     2 RDB$NAME_2
     3 RDB$NAME_3
     4 RDB$NAME_4
     5 RDB$NAME_5
     6 RDB$NAME_6
     7 RDB$NAME_7
     8 RDB$NAME_8
     9 RDB$NAME_9
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

