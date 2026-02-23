#coding:utf-8

"""
ID:          issue-4045
ISSUE:       4045
TITLE:       String truncation error when selecting from a VIEW with UNION inside
DESCRIPTION:
JIRA:        CORE-3697
FBTEST:      bugs.core_3697
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3697-ods11.fbk')

test_script = """
	-- Confirmed on 2.5.1:
	-- Statement failed, SQLSTATE = 22001
	-- arithmetic exception, numeric overflow, or string truncation
	-- -string right truncation

	create or alter view TREE_TEST (
		ID, TEXT, PARENTID, CONDITIONS,
		STMT, RULFNAME )
	as
	select 0, 'Организации', NULL, '', ''
	 , 'KODORG'
	 from rdb$database
	union
	select uidorg
	 , nameorg
	 , '0'
	 , ' G.UIDORG = ' || '''' || uidorg || ''''
	 , ''
	 , 'KODORG'
	 from org_delivery
	;
	commit;

	set list on;
	select id, text, parentid, conditions, stmt, rulfname
	from tree_test;
"""

act = isql_act('db', test_script)

expected_stdout = """
	ID                              0
	TEXT                            Организации
	PARENTID                        <null>
	CONDITIONS
	STMT
	RULFNAME                        KODORG

	ID                              7707083893     380000460
	TEXT                            АНГАРСКОЕ ОТДЕЛЕНИЕ № 7690
	PARENTID                        0
	CONDITIONS                       G.UIDORG = '7707083893     380000460'
	STMT
	RULFNAME                        KODORG

	ID                              7724261610     380000326
	TEXT                            ПОЧТАМТ
	PARENTID                        0
	CONDITIONS                       G.UIDORG = '7724261610     380000326'
	STMT
	RULFNAME                        KODORG
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

