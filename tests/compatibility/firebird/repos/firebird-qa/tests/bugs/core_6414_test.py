#coding:utf-8

"""
ID:          issue-6652
ISSUE:       6652
TITLE:       Error message "expected length N, actual M" contains wrong value of M when charset UTF8 is used in the field declaration of a table
DESCRIPTION:
  All attempts to create/alter table with not-null column with size that not enough space to fit default value must fail.
  Length of such column can be declared either directly or via domain - and both of these ways must fail.

  All failed statements must have SQLSTATE = 22001.
  Confirmed wrong result on 4.0.0.2225: some statements failed with SQLSTATE=22000 (malformed string),
  some issue wrong value of "actual M" for length.
JIRA:        CORE-6414
FBTEST:      bugs.core_6414
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
	-- must fail with "expected length 1, actual 6"
	recreate table test(nm varchar(1) character set utf8 default    'qwerty' not null);

	-- must fail with "expected length 1, actual 6"
	recreate table test(nm varchar(1) character set utf8 default    '€€€€€€' not null);

	create domain dm_ascii varchar(1) character set utf8 default    'qwertyu' not null;

	create domain dm_utf8 varchar(1) character set utf8 default    '€€€€€€€' not null;

	-- must fail with "expected length 1, actual 7"
	recreate table test(nm dm_ascii);

	-- must fail with "expected length 1, actual 7"
	recreate table test(nm dm_utf8);

	create domain dm_utf8_nullable varchar(1) character set utf8 default    '€€€€€€€€';

	-- must fail with "expected length 1, actual 8"
	recreate table test(nm dm_utf8_nullable not null);

	recreate table test(nm dm_utf8_nullable); -- must pass

	-- must fail with "expected length 1, actual 8" (because table 'test' exists)
	alter domain dm_utf8_nullable set not null;

    ----------------------------------------------------------

	recreate table test(id int);
	alter domain dm_utf8_nullable set not null;


	alter table test
	    add nm2 varchar(1) character set utf8 default '€€'
		not null -- leads to "expected length 1, actual 2"
	;


	alter table test
	    add nm3 varchar(1) character set utf8 default '€€€'
	   ,alter nm3 set not null -- leads to "expected length 1, actual 3"
	;

	alter table test
	    add nm4 varchar(3) character set utf8 default '€€€'
	   ,alter nm4 type varchar(4)
	   ,alter nm4 set default '€€€€€'
       ,alter nm4 set not null -- leads to "expected length 4, actual 5"
	;

	alter table test
	    add nm5 varchar(1) character set utf8
	   ,alter nm5 type dm_utf8_nullable -- leads to "expected length 1, actual 8"
	;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stderr = """
	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 1, actual 6

	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 1, actual 6

	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 1, actual 7

	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 1, actual 7

	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 1, actual 8

	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 1, actual 8

	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 1, actual 2

	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 1, actual 3

	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 4, actual 5

	Statement failed, SQLSTATE = 22001
	arithmetic exception, numeric overflow, or string truncation
	-string right truncation
	-expected length 1, actual 8
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
