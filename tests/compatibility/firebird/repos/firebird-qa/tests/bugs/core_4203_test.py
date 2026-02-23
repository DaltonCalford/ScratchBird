#coding:utf-8

"""
ID:          issue-4528
ISSUE:       4528
TITLE:       Cannot create packaged routines with [VAR]CHAR parameters
DESCRIPTION:
JIRA:        CORE-4203
FBTEST:      bugs.core_4203
NOTES:
    [29.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create package pkg_test as
    begin
       function f1(x char(3)) returns char(6) ;
    end
    ^
    commit ^

    create package body pkg_test as
    begin
        function f1(x char(3)) returns char(6) as
        begin
            return x;
        end
    end
    ^

    show package pkg_test
    ^
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'PUBLIC.'
    expected_stdout = f"""
        {SQL_SCHEMA_PREFIX}PKG_TEST
        Header source:
        begin
               function f1(x char(3)) returns char(6) ;
            end

        Body source:
        begin
                function f1(x char(3)) returns char(6) as
                begin
                    return x;
                end
            end
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

