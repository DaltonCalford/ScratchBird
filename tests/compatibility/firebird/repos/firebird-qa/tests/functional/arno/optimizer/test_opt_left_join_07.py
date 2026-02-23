#coding:utf-8

"""
ID:          optimizer.left-join-07
TITLE:       4 JOINed tables with 1 LEFT OUTER JOIN
DESCRIPTION:
  A INNER JOINed TableD to a LEFT JOINed TableC should be able to access the outer TableB
  of TableC. Also TableB is INNER JOINed to TableA. Three indexes can and should be used here.
FBTEST:      functional.arno.optimizer.opt_left_join_07
NOTES:
    [31.07.2023] pzotov
    Test was excluded from execution under FB 5.x: no more sense in it for this FB version.
    Discussed with dimitr, letter 30.07.2023.
    Checked finally on 4.0.3.2966, 3.0.11.33695 -- all fine.
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE Colors (
  ColorID INTEGER NOT NULL,
  ColorName VARCHAR(20)
);

CREATE TABLE Flowers (
  FlowerID INTEGER NOT NULL,
  FlowerName VARCHAR(30),
  ColorID INTEGER
);

COMMIT;

/* Value 0 represents -no value- */
INSERT INTO Colors (ColorID, ColorName) VALUES (0, 'Not defined');
INSERT INTO Colors (ColorID, ColorName) VALUES (1, 'Red');
INSERT INTO Colors (ColorID, ColorName) VALUES (2, 'Yellow');

/* insert some data with references */
INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (1, 'Rose', 1);
INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (2, 'Tulip', 2);
INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (3, 'Gerbera', 0);

COMMIT;

/* Normally these indexes are created by the primary/foreign keys,
   but we don't want to rely on them for this test */
CREATE UNIQUE ASC INDEX PK_Colors ON Colors (ColorID);
CREATE UNIQUE ASC INDEX PK_Flowers ON Flowers (FlowerID);
CREATE ASC INDEX FK_Flowers_Colors ON Flowers (ColorID);

COMMIT;
"""

db = db_factory(init=init_script)

test_script = """SET PLAN ON;
/* 4 joined tables with 1 LEFT JOIN */
SELECT
  f.ColorID,
  c1.ColorID,
  c2.ColorID,
  c3.ColorID
FROM
  Flowers f
  JOIN Colors c1 ON (c1.ColorID = f.ColorID)
  LEFT JOIN Colors c2 ON (c2.ColorID = c1.ColorID)
JOIN Colors c3 ON (c3.ColorID = c1.ColorID);"""

act = isql_act('db', test_script)

expected_stdout = """PLAN JOIN (JOIN (JOIN (F NATURAL, C1 INDEX (PK_COLORS)), C2 INDEX (PK_COLORS)), C3 INDEX (PK_COLORS))

     COLORID      COLORID      COLORID      COLORID
============ ============ ============ ============

           1            1            1            1
           2            2            2            2
0            0            0            0"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    if act.is_version('>=5'):
        pytest.skip("Test has no sense in FB 5.x, see notes.")
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
