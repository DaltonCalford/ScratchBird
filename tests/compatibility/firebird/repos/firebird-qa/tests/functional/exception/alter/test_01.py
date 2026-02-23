#coding:utf-8

"""
ID:          exception.alter
FBTEST:      functional.exception.alter.01
TITLE:       ALTER EXCEPTION
DESCRIPTION:
"""

import pytest
from firebird.qa import *

init_script = """
    create exception test 'message to show';
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    alter exception test 'new message';
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
    EXC_MSG                         new message
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
