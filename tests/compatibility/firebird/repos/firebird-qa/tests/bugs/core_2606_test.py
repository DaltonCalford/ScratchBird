#coding:utf-8

"""
ID:          issue-3016
ISSUE:       3016
TITLE:       Multibyte CHAR value requested as VARCHAR is returned with padded spaces
DESCRIPTION:
JIRA:        CORE-2606
FBTEST:      bugs.core_2606
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_script = """
set list on;

select s.rdb$character_set_name as client_charset
from mon$attachments a
join rdb$character_sets s on a.mon$character_set_id = s.rdb$character_set_id
where a.mon$attachment_id=current_connection;

--set sqlda_display on;
--set planonly;
--set echo on;

select cast('A' as char character set utf8) || '.' as char1_utf8 from rdb$database;
select cast('A' as varchar(1) character set utf8) || '.' as varc1_utf8 from rdb$database;
select cast('A' as char character set ascii) || '.' char1_ascii from rdb$database;
select cast('A' as varchar(1) character set ascii) || '.' varc1_ascii from rdb$database;
"""

expected_stdout_a = """
    CLIENT_CHARSET                  UTF8
    CHAR1_UTF8                      A.
    VARC1_UTF8                      A.
    CHAR1_ASCII                     A.
    VARC1_ASCII                     A.
"""

expected_stdout_b = """
    CLIENT_CHARSET                  SJIS_0208
    CHAR1_UTF8                      A.
    VARC1_UTF8                      A.
    CHAR1_ASCII                     A.
    VARC1_ASCII                     A.
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.script = test_script
    act.expected_stdout = expected_stdout_a
    act.execute(charset='UTF8')
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
    act.expected_stdout = expected_stdout_b
    act.execute(charset='SJIS_0208')
    assert act.clean_stdout == act.clean_expected_stdout



