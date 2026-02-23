#coding:utf-8

"""
ID:          exception.create-01
FBTEST:      functional.exception.create.01
TITLE:       CREATE EXCEPTION
DESCRIPTION:
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create exception test 'message to show';
    commit;

    set list on;
    set width exc_name 31;
    set width exc_msg 80;
    select
        e.rdb$exception_name exc_name
        ,e.rdb$exception_number exc_number
        ,e.rdb$message exc_msg
    from rdb$exceptions e;
"""

act = isql_act('db', test_script)

expected_stdout = """
    EXC_NAME                        TEST
    EXC_NUMBER                      1
    EXC_MSG                         message to show
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
