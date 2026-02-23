#coding:utf-8

"""
ID:          issue-1972
ISSUE:       1972
TITLE:       'select ...from...where...not in (select...from...)' no results
DESCRIPTION:
JIRA:        CORE-1555
FBTEST:      bugs.core_1555
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^ ;
    create or alter procedure tst1 returns (boxes integer) as
    begin
      boxes=1;
      suspend;
      boxes=2;
      suspend;
    end
    ^
    set term ;^
    commit;

    recreate table frrates1 (
      frrates1 integer not null,
      boxes integer,
      primary key(frrates1)
    );
    commit;

    create index idx_frrates1_boxes on frrates1 (boxes);
    commit;

    recreate table schedpkgs1 (
      schedpkgs1 integer not null,
      schedule integer,
      frrates1 integer,
      primary key (schedpkgs1)
    );
    commit;

    create index idx_schedpkgs1_schedule on schedpkgs1 (schedule);
    commit;


    insert into frrates1 (frrates1, boxes) values (11, 1);
    insert into frrates1 (frrates1, boxes) values (12, 2);
    commit;

    insert into schedpkgs1 (schedpkgs1, schedule, frrates1) values(21, 16651, 11);
    insert into schedpkgs1 (schedpkgs1, schedule, frrates1) values(22, 16651, null);
    commit;

    ---------------------------

    set list on;

    select fr.boxes
      from schedpkgs1 sp
      join frrates1 fr on fr.frrates1=sp.frrates1
      where sp.schedule = 16651;


    select boxes
      from tst1
      where boxes not in (select fr.boxes
                               from schedpkgs1 sp
                               join frrates1 fr on fr.frrates1=sp.frrates1
                               where sp.schedule = 16651);


    select boxes
      from tst1
      where boxes not in (select fr.boxes
                               from schedpkgs1 sp
                               join frrates1 fr on fr.frrates1=sp.frrates1
                               where sp.schedule = 16651 and fr.boxes>0);


    select f2.boxes
      from frrates1 f2
      where f2.boxes not in (select fr.boxes
                                  from schedpkgs1 sp
                                  join frrates1 fr on fr.frrates1=sp.frrates1
                                  where sp.schedule = 16651);

    select f2.boxes
      from frrates1 f2
      where f2.boxes not in (select fr.boxes
                                  from schedpkgs1 sp
                                  join frrates1 fr on fr.frrates1=sp.frrates1
                                  where sp.schedule = 16651 and fr.boxes>0);
"""

act = isql_act('db', test_script)

expected_stdout = """
    BOXES                           1
    BOXES                           2
    BOXES                           2
    BOXES                           2
    BOXES                           2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

