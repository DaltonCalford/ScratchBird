#coding:utf-8

"""
ID:          issue-1211
ISSUE:       1211
TITLE:       Accent ignoring collation for unicode
DESCRIPTION:
JIRA:        CORE-824
FBTEST:      bugs.core_0824
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """SELECT IIF('eeaauoeeeaauo' = 'ÉÈÀÂÛÔÊéèàâûô' COLLATE UNICODE_CI_AI ,'true','false'),'true','''eeaauoeeeaauo'' = ''ÉÈÀÂÛÔÊéèàâûô'' COLLATE UNICODE_CI_AI' FROM RDB$DATABASE;
SELECT IIF('EEAAUOEEEAAUO' = 'ÉÈÀÂÛÔÊéèàâûô' COLLATE UNICODE_CI_AI ,'true','false'),'true','''EEAAUOEEEAAUO'' = ''ÉÈÀÂÛÔÊéèàâûô'' COLLATE UNICODE_CI_AI' FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """
CASE   CONSTANT CONSTANT
====== ======== =======================================================
true   true     'eeaauoeeeaauo' = 'ÉÈÀÂÛÔÊéèàâûô' COLLATE UNICODE_CI_AI


CASE   CONSTANT CONSTANT
====== ======== =======================================================
true   true     'EEAAUOEEEAAUO' = 'ÉÈÀÂÛÔÊéèàâûô' COLLATE UNICODE_CI_AI

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

