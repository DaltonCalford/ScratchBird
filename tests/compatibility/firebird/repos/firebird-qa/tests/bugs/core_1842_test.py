#coding:utf-8

"""
ID:          issue-2271
ISSUE:       2271
TITLE:       DEFAULT values are unnecessary evaluated
DESCRIPTION:
JIRA:        CORE-1842
FBTEST:      bugs.core_1842
"""

import pytest
from firebird.qa import *

init_script = """create table tb_date (
    tb_date_id integer not null primary key,
    f_date date default 0);
commit;"""

db = db_factory(init=init_script)

test_script = """INSERT INTO TB_DATE (
    TB_DATE_ID, F_DATE)
  VALUES (
    1, '09-MAY-1945');
insert into tb_date (
    tb_date_id, f_date)
  values (
    2, null);
commit;
SELECT TB_DATE_ID, F_DATE FROM TB_DATE;
"""

act = isql_act('db', test_script)

expected_stdout = """
  TB_DATE_ID      F_DATE
============ ===========
           1 1945-05-09
           2      <null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

