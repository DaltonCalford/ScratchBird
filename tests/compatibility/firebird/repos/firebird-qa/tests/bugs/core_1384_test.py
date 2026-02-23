#coding:utf-8

"""
ID:          issue-1802
ISSUE:       1802
TITLE:       LIKE doesn't work correctly with collations using SPECIALS-FIRST=1
DESCRIPTION:
JIRA:        CORE-1384
FBTEST:      bugs.core_1384
"""

import pytest
from firebird.qa import *

init_script = """
	create collation coll_es for iso8859_1 from external ('ES_ES_CI_AI') 'SPECIALS-FIRST=1';
	create collation coll_fr for iso8859_1 from external ('FR_FR') CASE INSENSITIVE accent insensitive 'SPECIALS-FIRST=1';
	commit;

	create or alter view v_test as
	select
	   iif( _iso8859_1 'Ja ' collate coll_es like _iso8859_1 '% a%' collate coll_es, 1, 0) result_for_es_ci_ai
	   ,iif( _iso8859_1 'ka ' collate coll_fr like _iso8859_1 '% a%' collate coll_fr, 1, 0) result_for_fr_ci_ai
	from rdb$database
	UNION ALL -- added comparison to pattern with diactiric mark:
	select
	   iif( _iso8859_1 'Jà ' collate coll_es like _iso8859_1 '% à %' collate coll_es, 1, 0) result_for_es_ci_ai
	   ,iif( _iso8859_1 'kà ' collate coll_fr like _iso8859_1 '% à %' collate coll_fr, 1, 0) result_for_fr_ci_ai
	from rdb$database
	;
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """
set list on;
select * from v_test;
"""

act = isql_act('db', test_script)

expected_stdout = """
	RESULT_FOR_ES_CI_AI             0
	RESULT_FOR_FR_CI_AI             0
	RESULT_FOR_ES_CI_AI             0
	RESULT_FOR_FR_CI_AI             0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(charset='ISO8859_1')
    assert act.clean_stdout == act.clean_expected_stdout


