#coding:utf-8

"""
ID:          issue-2373
ISSUE:       2373
TITLE:       The log(base, number) built-in function doesn't check parameters and delivers NAN values instead
DESCRIPTION:
JIRA:        CORE-1936
FBTEST:      bugs.core_1936
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select log(2, 00) from rdb$database;
select log(2, -2) from rdb$database;
select log(0, 1) from rdb$database;
select log(0, 2) from rdb$database;
select log(-1, 2) from rdb$database;
select log(-1, 0) from rdb$database;
select log(-1, -1) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
                    LOG
=======================

                    LOG
=======================

                    LOG
=======================

                    LOG
=======================

                    LOG
=======================

                    LOG
=======================

                    LOG
=======================
"""

expected_stderr = """Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Argument for LOG must be positive

Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Argument for LOG must be positive

Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Base for LOG must be positive

Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Base for LOG must be positive

Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Base for LOG must be positive

Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Base for LOG must be positive

Statement failed, SQLSTATE = 42000

expression evaluation not supported

-Base for LOG must be positive

"""

@pytest.mark.version('>=2.5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

