#coding:utf-8

"""
ID:          issue-2477
ISSUE:       2477
TITLE:       update or insert with gen_id() with wrong generator value
DESCRIPTION:
JIRA:        CORE-2041
FBTEST:      bugs.core_2041
"""

import pytest
from firebird.qa import *

init_script = """CREATE GENERATOR LOGID;

set generator LOGID to 0;

CREATE TABLE LOGBOOK
  ( ID integer not null,
    ENTRY varchar(64),
    PRIMARY KEY (ID));

set term ^ ;

create procedure logrow(txt varchar(64))
as
  declare variable lid integer;
begin
  lid = gen_id(LOGID, 1);
  update or insert into logbook (id, entry)
      values (:lid, :txt);
end^

set term ; ^
"""

db = db_factory(init=init_script)

test_script = """update or insert into logbook (id, entry) values (gen_id(LOGID, 1), 'Testing 1');
update or insert into logbook (id, entry) values (gen_id(LOGID, 1), 'Testing 2');
update or insert into logbook (id, entry) values (gen_id(LOGID, 1), 'Testing 3');
update or insert into logbook (id, entry) values (gen_id(LOGID, 1), 'Testing 4');
select * from logbook;

execute procedure logrow('Test 1');
execute procedure logrow('Test 2');
execute procedure logrow('Test 3');
execute procedure logrow('Test 4');
select * from logbook;
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID ENTRY
============ ================================================================
           1 Testing 1
           2 Testing 2
           3 Testing 3
           4 Testing 4


          ID ENTRY
============ ================================================================
           1 Testing 1
           2 Testing 2
           3 Testing 3
           4 Testing 4
           5 Test 1
           6 Test 2
           7 Test 3
           8 Test 4

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

