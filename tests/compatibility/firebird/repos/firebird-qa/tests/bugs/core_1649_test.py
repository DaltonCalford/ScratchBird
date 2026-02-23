#coding:utf-8

"""
ID:          issue-2073
ISSUE:       2073
TITLE:       AV when recursive query used MERGE JOIN in execution plan
DESCRIPTION:
JIRA:        CORE-1649
FBTEST:      bugs.core_1649
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE XXX (
    ID INTEGER,
    L INTEGER,
    R INTEGER,
    TYP INTEGER,
    XID INTEGER);

CREATE TABLE con (
    typ integer,
    xid integer
);
COMMIT;
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (20, 7, 8, 0, 3);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (36, 18, 19, 1, 200017);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (18, 3, 4, 1, 200011);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (35, 16, 17, 1, 200014);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (34, 15, 20, 0, 5);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (32, 10, 11, 1, 200016);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (41, 25, 26, 1, 200020);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (37, 22, 39, 0, 6);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (39, 23, 24, 1, 200018);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (43, 27, 28, 1, 200021);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (46, 29, 30, 1, 200023);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (52, 35, 36, 1, 200026);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (51, 33, 34, 1, 200025);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (50, 31, 32, 1, 200024);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (54, 37, 38, 1, 200027);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (109, 63, 64, 1, 200048);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (93, 40, 41, 1, 200049);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (107, 59, 60, 1, 200052);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (95, 42, 43, 1, 200051);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (108, 61, 62, 1, 200028);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (97, 44, 45, 1, 200053);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (1, 1, 68, -1, NULL);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (106, 57, 58, 1, 200050);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (105, 55, 56, 1, 200033);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (100, 46, 47, 1, 200031);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (104, 53, 54, 1, 100003);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (101, 48, 65, 0, 7);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (103, 51, 52, 1, 100004);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (102, 49, 50, 1, 100002);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (110, 66, 67, 1, 200093);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (113, 12, 13, 1, 200094);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (3, 6, 21, 0, 1);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (22, 9, 14, 0, 4);
INSERT INTO XXX (ID, L, R, TYP, XID) VALUES (10, 2, 5, 0, 2);

INSERT INTO CON (TYP, XID) VALUES (0, 6);
INSERT INTO CON (TYP, XID) VALUES (1, 2);
INSERT INTO CON (TYP, XID) VALUES (1, 3);
INSERT INTO CON (TYP, XID) VALUES (1, 18);
INSERT INTO CON (TYP, XID) VALUES (1, 19);
INSERT INTO CON (TYP, XID) VALUES (1, 62);
INSERT INTO CON (TYP, XID) VALUES (1, 151);
INSERT INTO CON (TYP, XID) VALUES (1, 224);
INSERT INTO CON (TYP, XID) VALUES (1, 254);
INSERT INTO CON (TYP, XID) VALUES (1, 255);
INSERT INTO CON (TYP, XID) VALUES (1, 281);
INSERT INTO CON (TYP, XID) VALUES (1, 200053);

COMMIT;"""

db = db_factory(init=init_script)

test_script = """with recursive
downtree (lvl, id, l, r)
as
(-- base
  select -1, id, l, r
  from xxx
  where typ = -1

  -- children
  union all
  select parent.lvl + 1, child.id, child.l, child.r
  from xxx child
  natural join con

  join downtree parent on child.l between parent.l and parent.r and
  child.id <> parent.id
)
select * from downtree;
"""

act = isql_act('db', test_script)

expected_stdout = """
         LVL           ID            L            R
============ ============ ============ ============
          -1            1            1           68
           0           37           22           39
           0           97           44           45

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

