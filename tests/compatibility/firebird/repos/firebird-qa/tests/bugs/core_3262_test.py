#coding:utf-8

"""
ID:          issue-3630
ISSUE:       3630
TITLE:       LIST() may overwrite last part of output with zero characters
DESCRIPTION:
JIRA:        CORE-3262
FBTEST:      bugs.core_3262
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table vc (s varchar(8000));
    commit;

    insert into vc values (cast('A' as char(4000)) || 'B');

    set list on;

    select char_length(s), position('A' in s), position('B' in s) from vc;

    with q (l) as (select list(s) from vc)
      select char_length(l), position('A' in l), position('B' in l) from q;


    update vc set s = (cast('A' as char(5000)) || 'B');

    select char_length(s), position('A' in s), position('B' in s) from vc;


    with q (l) as (select list(s) from vc)
      select char_length(l), position('A' in l), position('B' in l) from q;


    with q (l) as (select reverse(list(s)) from vc)
      select char_length(l), position('A' in l), position('B' in l) from q;

    with q (l) as (select list(s) from vc)
      select ascii_val(substring(l from 4066 for 1)), ascii_val(substring(l from 4067 for 1)) from q;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CHAR_LENGTH                     4001
    POSITION                        1
    POSITION                        4001

    CHAR_LENGTH                     4001
    POSITION                        1
    POSITION                        4001

    CHAR_LENGTH                     5001
    POSITION                        1
    POSITION                        5001

    CHAR_LENGTH                     5001
    POSITION                        1
    POSITION                        5001

    CHAR_LENGTH                     5001
    POSITION                        5001
    POSITION                        1

    ASCII_VAL                       32
    ASCII_VAL                       32
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

