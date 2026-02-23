#coding:utf-8

"""
ID:          issue-2116
ISSUE:       2116
TITLE:       Arithmetic exception, numeric overflow, or string truncation in utf8 tables
DESCRIPTION:
JIRA:        CORE-1690
FBTEST:      bugs.core_1690
NOTES:
    [25.06.2025] pzotov
    Re-implemented: non-ascii names are used for created table and its column, with max allowed length
    for utf8 charset to prevent "Name longer than database column size" error.

    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='utf8')

substitutions = [('[ \t]+', ' '), ('Table:.*', ''), ('CONSTRAINT INTEG_\\d+', 'CONSTRAINT INTEG')]
act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):
    if act.is_version('<4'):
        # We have ti limit names by 16 unicode characters otherwise get:
        # "SQLSTATE = 42000 / -Name longer than database column size"
        TABLE_NAME = 'àáâãäåæçèéêëìíî'
        FIELD_NAME = 'ÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞ'
    else:
        TABLE_NAME = 'àáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ'
        FIELD_NAME = 'ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþ'
    
    test_script = f"""
        create table "{TABLE_NAME}" ("{FIELD_NAME}" int primary key);
        show table "{TABLE_NAME}";
    """

    expected_stdout_5x = f"""
        {FIELD_NAME} INTEGER Not Null
        CONSTRAINT INTEG_2:
        Primary key ({FIELD_NAME})
    """

    # NB: Names of table and field are enclosed in double quotes in FB 6.x (since 6.0.0.834):
    expected_stdout_6x = f"""
        "{FIELD_NAME}" INTEGER Not Null
        CONSTRAINT INTEG_2:
        Primary key ("{FIELD_NAME}")
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    #act.execute(combine_output = True)
    act.isql(switches = ['-q'], charset = 'utf8', input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
