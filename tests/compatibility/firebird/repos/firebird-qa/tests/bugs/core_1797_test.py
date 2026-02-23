#coding:utf-8

"""
ID:          issue-2223
ISSUE:       2223
TITLE:       OLD/NEW.RDB$DB_KEY returns incorrect result in triggers
DESCRIPTION:
JIRA:        CORE-1797
FBTEST:      bugs.core_1797
"""

import pytest
from firebird.qa import *

init_script = """create table t (n integer) ;
create table x (c char(8) character set octets);
set term ^ ;
create trigger t_ai for t after insert
as
begin
  insert into x values (new.rdb$db_key);
end ^
set term ; ^
"""

db = db_factory(init=init_script)

test_script = """insert into t values (1) returning rdb$db_key;

select rdb$db_key from t;
select * from x;

insert into t values (2) returning rdb$db_key;

select rdb$db_key from t;
select * from x;
"""

act = isql_act('db', test_script)

expected_stdout = """
DB_KEY
================
8000000001000000


DB_KEY
================
8000000001000000


C
================
8000000001000000


DB_KEY
================
8000000002000000


DB_KEY
================
8000000001000000
8000000002000000


C
================
8000000001000000
8000000002000000

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

