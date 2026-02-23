#coding:utf-8

"""
ID:          issue-6657
ISSUE:       6657
TITLE:       Truncation of strings to put in MON$ tables do not work correctly
DESCRIPTION:
  We create two context variables, one consists only of ASCII characters, second contains UTF-8 character for euro currensy.
  Both variables have lenght = 90 *characters*.
  These variables are stored in mon$context_variables WITHOUT any adjusting to character set, i.e. it is NONE.
  When we query to them from mon$context_variables, output of first must be truncated to 80 *octets*.
  Output of second ALSO must be exactly 80 *octets*, but this leads to unreadable last character: not all bytes
  of 'euro' currency (which requires 3 bytes in utf-8) will be issued.

  This is considered now as EXPECTED RESULT(!), in contrary to previous one which raised exception SQLSTATE = 22001
  (string right truncation / -expected length 80, actual 90). See comments in the ticket by Adriano, 21-oct-2020 03:31 PM.

  In order to avoid Python-specific exception ('UnicodeDecodeError: utf8') we avoid to display second (utf8) variable.
  Rather, we just show its octet_length and char_length.
JIRA:        CORE-6419
FBTEST:      bugs.core_6419
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
	set list on;
	set count on;
	set term ^;
	execute block as
	begin
		rdb$set_context('USER_SESSION', '12345678901234567890123456789012345678901234567890123456789012345678901234567890abcdefghij', '1');
		rdb$set_context('USER_SESSION', 'ASDFGHJKLP€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€', '1');
	end
	^
	set term ;^


	select
		c.mon$variable_name as mon_context_name
	   ,octet_length(c.mon$variable_name) mon_context_length
	from mon$context_variables c
	where mon$variable_name starting with '1234567890'
	;

	select
		 char_length(c.mon$variable_name) utf8_context_char_length
	    ,octet_length(c.mon$variable_name) utf8_context_octet_length
	from mon$context_variables c
	where mon$variable_name starting with 'ASDFGHJKLP€€€'
	;
    -- NOTE. DO NOT output mon$variable_name for second query, result will be: UnicodeDecodeError: utf8
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
	MON_CONTEXT_NAME                12345678901234567890123456789012345678901234567890123456789012345678901234567890
	MON_CONTEXT_LENGTH              80
	Records affected: 1

	UTF8_CONTEXT_CHAR_LENGTH        80
	UTF8_CONTEXT_OCTET_LENGTH       80
	Records affected: 1
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
