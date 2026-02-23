#coding:utf-8

"""
ID:          issue-7916
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7916
TITLE:       Query issue conversion error from string
NOTES:
    [10.02.2024] pzotov
    Confirmed bug on 6.0.0.250
    Checked on 6.0.0.257 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table session$test (sess_user char(63));
    recreate table staff$test (staffid smallint, primary key (staffid) using index staff$test_staffid);

    insert into session$test values ('TEST');
    insert into session$test values ('1');
    insert into staff$test values (1);

    set list on;
    -- set explain on;

    select sess.sess_user, stf.staffid 
      from session$test sess
           left join rdb$database rdb
              on 1 = 1
           left join staff$test stf
              on trim(sess.sess_user) similar to '[0-9]+' 
             and stf.staffid = cast(trim(sess.sess_user) as smallint)
    order by stf.staffid + 0
    ;
"""

act = isql_act('db', test_script, substitutions = [('[ \t]+', ' ')])

expected_stdout = """
    SESS_USER TEST
    STAFFID <null>

    SESS_USER 1
    STAFFID 1
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
