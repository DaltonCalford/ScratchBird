#coding:utf-8

"""
ID:          issue-2741
ISSUE:       2741
TITLE:       select * from (select cast(.... returns null
DESCRIPTION:
JIRA:        CORE-2317
FBTEST:      bugs.core_2317
"""

import pytest
from firebird.qa import *

init_script = """
	CREATE TABLE PROC(
	  PROC Char(10) NOT NULL,
	  TPRO Char(10) NOT NULL,
	  ACTO Varchar(200) NOT NULL,
	  CONSTRAINT PROC_PK PRIMARY KEY (PROC)
	);

	CREATE TABLE TPRO(
	  TPRO Char(10) NOT NULL,
	  CONSTRAINT TPRO_PK PRIMARY KEY (TPRO)
	);

	CREATE VIEW PROC1 (PROC, ACTO)
	AS SELECT "PROC"."PROC","PROC"."ACTO" FROM "PROC" left outer join "TPRO" on "PROC"."TPRO"="TPRO"."TPRO";

	insert into proc values ('1R1oK3qxdM', '1', '2');
	insert into tpro values ('1');
	commit;
"""

db = db_factory(init=init_script)

test_script = """
	set list on;
	-- returns NULL
	select * from (select cast("ACTO" as character(100)) as "D_COL1" from "PROC1" where "PROC"='1R1oK3qxdM') AA;
	-- returns the correct value
	select * from (select cast("ACTO" as character(100)) as "D_COL1" from "PROC" where "PROC"='1R1oK3qxdM') AA;
	select * from (select "ACTO" as "D_COL1" from "PROC1" where "PROC"='1R1oK3qxdM') AA;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
	D_COL1                          2
	D_COL1                          2
	D_COL1                          2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

