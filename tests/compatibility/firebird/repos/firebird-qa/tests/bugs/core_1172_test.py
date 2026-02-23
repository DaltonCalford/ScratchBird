#coding:utf-8

"""
ID:          issue-679
ISSUE:       679
TITLE:       Symbols ignored for ES_ES_CI_AI collation
DESCRIPTION:
JIRA:        CORE-1172
FBTEST:      bugs.core_1172
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE TABLE_A (
    FIELD_A VARCHAR(10) CHARACTER SET iso8859_1 COLLATE ES_ES_CI_AI
);

ALTER TABLE TABLE_A ADD CONSTRAINT UNQ1_TABLE_A UNIQUE (FIELD_A);

INSERT INTO TABLE_A (FIELD_A) VALUES ('A');

commit;

create collation es_es_ci_ai2 for iso8859_1 from es_es_ci_ai 'SPECIALS-FIRST=1';

CREATE TABLE TABLE_B (
    FIELD_A VARCHAR(10) CHARACTER SET iso8859_1 COLLATE ES_ES_CI_AI2
);

ALTER TABLE TABLE_B ADD CONSTRAINT UNQ1_TABLE_B UNIQUE (FIELD_A);

INSERT INTO TABLE_B (FIELD_A) VALUES ('A');

commit;
"""

db = db_factory(init=init_script)

test_script = """INSERT INTO TABLE_A (FIELD_A) VALUES ('A.');
INSERT INTO TABLE_A (FIELD_A) VALUES ('A-');
INSERT INTO TABLE_A (FIELD_A) VALUES ('-A');
INSERT INTO TABLE_A (FIELD_A) VALUES ('(A)');
INSERT INTO TABLE_A (FIELD_A) VALUES ('(A)a');
INSERT INTO TABLE_A (FIELD_A) VALUES ('Aa');

INSERT INTO TABLE_B (FIELD_A) VALUES ('A.');
INSERT INTO TABLE_B (FIELD_A) VALUES ('A-');
INSERT INTO TABLE_B (FIELD_A) VALUES ('-A');
INSERT INTO TABLE_B (FIELD_A) VALUES ('(A)');
INSERT INTO TABLE_B (FIELD_A) VALUES ('(A)a');
INSERT INTO TABLE_B (FIELD_A) VALUES ('Aa');
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
