#coding:utf-8

"""
ID:          issue-1774
ISSUE:       1774
TITLE:       TYPE OF COLUMN in PSQL
DESCRIPTION:
JIRA:        CORE-1356
FBTEST:      bugs.core_1356
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure sp_ins_person as begin end;
    commit;
    recreate table person (
      id integer,
      name varchar(40)
    );
    commit;

    set term ^;
    create or alter procedure sp_ins_person (
      id type of column person.id,
      name type of column person.name
    ) as
        declare variable new_id type of column person.id;
    begin
      insert into person (id, name) values (:id, :name) returning id into :new_id;
    end^
    set term ;^
    commit;
    --show procedure sp_ins_person;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
