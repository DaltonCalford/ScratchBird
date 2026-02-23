#coding:utf-8

"""
ID:          issue-7243
ISSUE:       7243
TITLE:       Some UNICODE characters can lead to wrong CONTAINING evaluation when lower/upper uses different number of bytes in its encoding
NOTES:
    [22.02.2023] pzotov
	Confirmed bug on 5.0.0.523, got exception
		Statement failed, SQLSTATE = 22001
		arithmetic exception, numeric overflow, or string truncation
		-string right truncation
		-expected length 2, actual 2	
    Cheched on 5.0.0.958; 4.0.3.2903 - all fine.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;
    set blob all;
    create table tmp_test(id integer, note blob sub_type 1 segment size 80 character set utf8 collate unicode);
    insert into tmp_test(id, note) values (1,'ɬ');
    commit;

    set count on;
    select a.id,a.note as note_blob_id
	from tmp_test a
    where a.note containing 'ɬ';
"""

expected_stdout = """
	ID                              1
	NOTE_BLOB_ID                    80:0
	ɬ
	Records affected: 1
"""

substitutions = [ ('NOTE_BLOB_ID .*', ''), ('[ \t]+', ' ') ]

act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=4.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True, charset = 'utf8' )
    assert act.clean_stdout == act.clean_expected_stdout
