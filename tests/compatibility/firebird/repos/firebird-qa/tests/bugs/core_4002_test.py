#coding:utf-8

"""
ID:          issue-4334
ISSUE:       4334
TITLE:       Error message "index unexpectedly deleted" in database trigger on commit transaction
DESCRIPTION:
JIRA:        CORE-4002
FBTEST:      bugs.core_4002
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter trigger trg_commit active on transaction commit position 999 as
    begin
    end;

    create or alter procedure test_gg as begin end;
    commit;

    recreate global temporary table g2(id int);
    commit;

    recreate global temporary table g1 (
        id int primary key,
        tt varchar(100) not null,
        bb blob sub_type 1 segment size 80 not null
    ) on commit delete rows;
    commit;

    recreate global temporary table g2 (
        id int primary key ,
        id_g1 int not null references g1,
        tt varchar(100) not null,
        bb blob sub_type 1 segment size 80 not null
    ) on commit delete rows;
    commit;

    set term ^ ;

    create or alter procedure test_gg
    as
      declare i int = 0;
    begin
      in autonomous transaction do
      while (i<100) do
        begin
          insert into g1 (id, tt, bb) values (:i, rand(), rand());

          insert into g2 (id, id_g1, tt, bb) values (:i*1000+1, :i, rand(), rand());
          insert into g2 (id, id_g1, tt, bb) values (:i*1000+2, :i, rand(), rand());
          insert into g2 (id, id_g1, tt, bb) values (:i*1000+3, :i, rand(), rand());
          insert into g2 (id, id_g1, tt, bb) values (:i*1000+4, :i, rand(), rand());
          insert into g2 (id, id_g1, tt, bb) values (:i*1000+5, :i, rand(), rand());

          i=i+1;
        end
    end^

    commit^

    create or alter trigger trg_commit
    active on transaction commit position 999
    as
      declare variable id integer;
      declare variable tt varchar(100);
      declare variable bb blob sub_type 1;

      declare variable id_g1 integer;
      declare variable tt_2 varchar(100);
      declare variable bb_2 blob sub_type 1;

      declare i int=0;
    begin
      for select id, tt, bb
          from g1
          order by id
          into :id, :tt, :bb
      do
        begin
          for select id_g1, tt, bb
              from g2
              where id_g1=:id
              order by id
              into :id_g1, :tt_2, :bb_2
          do
            begin
              i=i+1;
            end
        end
    end^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    execute procedure test_gg;
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
