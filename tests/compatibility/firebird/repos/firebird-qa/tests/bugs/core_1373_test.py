#coding:utf-8

"""
ID:          issue-1791
ISSUE:       1791
TITLE:       Incorrect result of recursive CTE query when recursive member's SELECT list contains expression using self-referenced fields
DESCRIPTION:
JIRA:        CORE-1373
FBTEST:      bugs.core_1373
"""

import pytest
from firebird.qa import *

init_script = """RECREATE TABLE Phases
(Id INT NOT NULL PRIMARY KEY, ParentPhaseId INT);

CREATE GENERATOR GenPhases;
COMMIT;

INSERT INTO Phases VALUES(491, NULL);
INSERT INTO Phases VALUES(494, 491);
INSERT INTO Phases VALUES(495, 491);
INSERT INTO Phases VALUES(497, 494);
INSERT INTO Phases VALUES(498, NULL);

-- below i want to renumber Phases table and keep parent-child relation
SET GENERATOR GenPhases to 0;
COMMIT;
"""

db = db_factory(init=init_script)

test_script = """WITH RECURSIVE
  Tree (OldPhaseId, OldParentPhaseId, NewPhaseId, NewParentPhaseId) AS
  (
    SELECT P.Id, P.ParentPhaseId, GEN_ID(GenPhases, 1), CAST(NULL AS INTEGER)
      FROM Phases P
     WHERE P.ParentPhaseId IS NULL

    UNION ALL

    SELECT P.Id, P.ParentPhaseId, GEN_ID(GenPhases, 1), T.NewPhaseId
      FROM Phases P, Tree T
     WHERE P.ParentPhaseId = T.OldPhaseId
  )
SELECT * FROM Tree;

"""

act = isql_act('db', test_script)

expected_stdout = """
  OLDPHASEID OLDPARENTPHASEID            NEWPHASEID NEWPARENTPHASEID
============ ================ ===================== ================
         491           <null>                     1           <null>
         494              491                     2                1
         497              494                     3                2
         495              491                     4                1
         498           <null>                     5           <null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

