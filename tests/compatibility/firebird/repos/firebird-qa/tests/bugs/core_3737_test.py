#coding:utf-8

"""
ID:          issue-4082
ISSUE:       4082
TITLE:       EXECUTE BLOCK parameters definitions are not respected and may cause wrong
  behavior related to character sets
DESCRIPTION:
JIRA:        CORE-3737
FBTEST:      bugs.core_3737
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
	set list on;
	set term ^;
	execute block returns(len_1252 int, len_utf8 int) as
		declare s varchar(16) character set utf8 = 'ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ'; -- http://en.wikipedia.org/wiki/Windows-1252
	begin
		execute statement (
			'execute block (c varchar(16) character set win1252 = ?) returns (n int) as '
			|| 'begin '
			|| '  n = octet_length(c); '
			|| '  suspend; '
			|| 'end') (s)
		into len_1252;

		execute statement (
			'execute block (c varchar(16) character set utf8 = ?) returns (n int) as '
			|| 'begin '
			|| '  n = octet_length(c); '
			|| '  suspend; '
			|| 'end') (s)
		into len_utf8;
		suspend;

	end
	^
	set term ;^
	commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
	LEN_1252                        16
	LEN_UTF8                        32
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

