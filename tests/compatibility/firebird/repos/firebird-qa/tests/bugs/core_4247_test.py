#coding:utf-8

"""
ID:          issue-4571
ISSUE:       4571
TITLE:       Delete "where current of" cursor fails for tables with newly added fields
DESCRIPTION: Scenario has been taken from attachment to this ticket
JIRA:        CORE-4247
FBTEST:      bugs.core_4247
"""

import pytest
from firebird.qa import *

init_script = """
    create table test_table (id  integer not null, desc varchar(10));
    alter table test_table add constraint pk_test_table primary key (id);
    commit;

    insert into test_table (id, desc) values (1, 'a');
    insert into test_table (id, desc) values (2, 'b');
    insert into test_table (id, desc) values (3, 'c');
    insert into test_table (id, desc) values (4, 'd');
    insert into test_table (id, desc) values (5, 'e');
    insert into test_table (id, desc) values (6, 'f');
    insert into test_table (id, desc) values (7, 'g');
    insert into test_table (id, desc) values (8, 'h');
    insert into test_table (id, desc) values (9, 'i');
    insert into test_table (id, desc) values (10, 'k');
    commit;

    alter table test_table add seqno integer;
    commit;

    -- !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    -- without this update  - everything works
    update test_table set seqno=id where id>=5;
    commit;

    set term ^ ;
    create or alter procedure test_cursor
    as
    declare variable id integer;
    declare variable desc varchar(10);
    declare variable seqno integer;
    begin
        for
          select  id, desc, seqno from test_table
          order by seqno  -- if seqno values are unique - it works. With "order by id" works
          into
            :id, :desc, :seqno
          as cursor data_cursor
        do begin
          delete from test_table where current of data_cursor; -- this fails !!!
          -- with dummy suspend stored procedure works even it does not require to return any results
          --suspend;
        end
    end^
    set term ; ^

    commit;
"""

db = db_factory(init=init_script)

test_script = """
    execute procedure test_cursor;
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
