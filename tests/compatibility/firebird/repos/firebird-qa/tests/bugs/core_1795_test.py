#coding:utf-8

"""
ID:          issue-2221
ISSUE:       2221
TITLE:       Server crashes on SQL script
DESCRIPTION:
JIRA:        CORE-1795
FBTEST:      bugs.core_1795
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table colors (
      colorid integer not null,
      colorname varchar(20)
    );

    create table flowers (
      flowerid integer not null,
      flowername varchar(30),
      colorid integer
    );

    commit;

    insert into colors (colorid, colorname) values (0, 'not defined');
    insert into colors (colorid, colorname) values (1, 'red');
    insert into colors (colorid, colorname) values (2, 'white');
    insert into colors (colorid, colorname) values (3, 'blue');
    insert into colors (colorid, colorname) values (4, 'yellow');
    insert into colors (colorid, colorname) values (5, 'black');
    insert into colors (colorid, colorname) values (6, 'purple');

    insert into flowers (flowerid, flowername, colorid) values (1, 'red rose', 1);
    insert into flowers (flowerid, flowername, colorid) values (2, 'white rose', 2);
    insert into flowers (flowerid, flowername, colorid) values (3, 'blue rose', 3);
    insert into flowers (flowerid, flowername, colorid) values (4, 'yellow rose', 4);
    insert into flowers (flowerid, flowername, colorid) values (5, 'black rose', 5);
    insert into flowers (flowerid, flowername, colorid) values (6, 'red tulip', 1);
    insert into flowers (flowerid, flowername, colorid) values (7, 'white tulip', 2);
    insert into flowers (flowerid, flowername, colorid) values (8, 'yellow tulip', 4);
    insert into flowers (flowerid, flowername, colorid) values (9, 'blue gerbera', 3);
    insert into flowers (flowerid, flowername, colorid) values (10, 'purple gerbera', 6);

    commit;

    -- normally these indexes are created by the primary/foreign keys,
    -- but we don't want to rely on them for this test
    create unique asc index pk_colors on colors (colorid);
    create unique asc index pk_flowers on flowers (flowerid);
    create asc index fk_flowers_colors on flowers (colorid);
    create asc index i_colors_colorname on colors (colorname);
    commit;

    set list on;
    -- disable output of PLAN, it differ in 3.0 vs rest: set plan on;
    select
      f.colorid,
      c.colorname,
      count(*)
    from
      colors c
      left join flowers f on (f.colorid = c.colorid)
    group by
      f.colorid, c.colorname
    having
      c.colorname starting with 'b';
"""

act = isql_act('db', test_script)

expected_stdout = """
    COLORID                         3
    COLORNAME                       blue
    COUNT                           2

    COLORID                         5
    COLORNAME                       black
    COUNT                           1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

