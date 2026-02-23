#coding:utf-8

"""
ID:          issue-4467
ISSUE:       4467
TITLE:       EXECUTE BLOCK's TYPE OF parameters with NONE charset may have be transformed to the connection charset
DESCRIPTION:
JIRA:        CORE-4140
FBTEST:      bugs.core_4140
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
	create domain dm_utf8 varchar(50) character set utf8 collate unicode_ci_ai;
	create domain dm_none varchar(50);
	recreate table test(s varchar(50) character set none);
	commit;
	insert into test values('ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ'); -- http://en.wikipedia.org/wiki/Windows-1252
	commit;
	--show version;
	set term ^;
	execute block returns( msg varchar(35), txt_str type of dm_none, oct_len int, chr_len int) as
	begin
		msg = 'inp=dm_none, out=test.s, arg=utf8';
		execute statement (
			'execute block (c type of dm_none = ?) returns (txt_str type of column test.s, oct_len int, chr_len int) as '
			|| 'begin '
			|| '  txt_str = (select s from test where s starting with :c); '
			|| '  oct_len = octet_length(txt_str); '
			|| '  chr_len = char_length(txt_str); '
			|| '  suspend; '
			|| 'end') ( _utf8 'ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ' )
		into txt_str, oct_len, chr_len;
		suspend;

		msg = 'inp=dm_none, out=test.s, arg=w1252';
		execute statement (
			'execute block (c type of dm_none = ?) returns (txt_str type of column test.s, oct_len int, chr_len int) as '
			|| 'begin '
			|| '  txt_str = (select s from test where s starting with :c); '
			|| '  oct_len = octet_length(txt_str); '
			|| '  chr_len = char_length(txt_str); '
			|| '  suspend; '
			|| 'end') ( _win1252 'ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ' )
		into txt_str, oct_len, chr_len;
		suspend;


		msg = 'inp=test.s, out=dm_none, arg=utf8';
		execute statement (
			'execute block (c type of column test.s = ?) returns ( txt_str type of dm_none, oct_len int, chr_len int) as '
			|| 'begin '
			|| '  txt_str = (select s from test where s starting with :c); '
			|| '  oct_len = octet_length(txt_str); '
			|| '  chr_len = char_length(txt_str); '
			|| '  suspend; '
			|| 'end') ( _utf8 'ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ' )
		into txt_str, oct_len, chr_len;
		suspend;

		msg = 'inp=test.s, out=dm_none, arg=w1252';
		execute statement (
			'execute block (c type of column test.s = ?) returns ( txt_str type of dm_none, oct_len int, chr_len int) as '
			|| 'begin '
			|| '  txt_str = (select s from test where s starting with :c); '
			|| '  oct_len = octet_length(txt_str); '
			|| '  chr_len = char_length(txt_str); '
			|| '  suspend; '
			|| 'end') ( _win1252 'ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ' )
		into txt_str, oct_len, chr_len;
		suspend;

		/*
		msg = 'inp=utf8, out=utf8, arg=utf8';
		execute statement (
			'execute block (c type of dm_utf8 = ?) returns ( txt_str type of dm_utf8, oct_len int, chr_len int) as '
			|| 'begin '
			|| '  txt_str = (select s from test where s starting with :c); '
			|| '  oct_len = octet_length(txt_str); '
			|| '  chr_len = char_length(txt_str); '
			|| '  suspend; '
			|| 'end') ( _utf8 'ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ' )
		into txt_str, oct_len, chr_len;
		suspend;
		*/

		msg = 'inp=utf8, out=utf8, arg=w1252';
		execute statement (
			'execute block (c type of dm_utf8 = ?) returns ( txt_str type of dm_utf8, oct_len int, chr_len int) as '
			|| 'begin '
			|| '  txt_str = (select s from test where s starting with :c); '
			|| '  oct_len = octet_length(txt_str); '
			|| '  chr_len = char_length(txt_str); '
			|| '  suspend; '
			|| 'end') ( _win1252 'ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ' )
		into txt_str, oct_len, chr_len;
		suspend;


		msg = 'inp=test.s, out=test.s, arg=utf8';
		execute statement (
			'execute block (c type of column test.s = ?) returns ( txt_str type of column test.s, oct_len int, chr_len int) as '
			|| 'begin '
			|| '  txt_str = (select s from test where s starting with :c); '
			|| '  oct_len = octet_length(txt_str); '
			|| '  chr_len = char_length(txt_str); '
			|| '  suspend; '
			|| 'end') ( _utf8 'ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ' )
		into txt_str, oct_len, chr_len;
		suspend;

		msg = 'inp=test.s, out=test.s, arg=w1252';
		execute statement (
			'execute block (c type of column test.s = ?) returns ( txt_str type of column test.s, oct_len int, chr_len int) as '
			|| 'begin '
			|| '  txt_str = (select s from test where s starting with :c); '
			|| '  oct_len = octet_length(txt_str); '
			|| '  chr_len = char_length(txt_str); '
			|| '  suspend; '
			|| 'end') ( _win1252 'ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ' )
		into txt_str, oct_len, chr_len;
		suspend;

    end
	^
	set term ;^
	commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
	MSG                                 TXT_STR                                                 OCT_LEN      CHR_LEN
	=================================== ================================================== ============ ============
	inp=dm_none, out=test.s, arg=utf8   ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ                             32           32
	inp=dm_none, out=test.s, arg=w1252  ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ                             32           32
	inp=test.s, out=dm_none, arg=utf8   ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ                             32           32
	inp=test.s, out=dm_none, arg=w1252  ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ                             32           32
	inp=utf8, out=utf8, arg=w1252       <null>                                                   <null>       <null>
	inp=test.s, out=test.s, arg=utf8    ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ                             32           32
	inp=test.s, out=test.s, arg=w1252   ÃÂÁÀÄÅÇØßÐÑÞÆŠŒŽ                             32           32
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

