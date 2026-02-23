#coding:utf-8

"""
ID:          issue-4759
ISSUE:       4759
TITLE:       Raise the 1024 connections limit (FD_SETSIZE) on Windows SS/SC
DESCRIPTION:
  Test tries to establish MAX_CONN_CNT = 2047 connections and then close all of them.
  When connection is establishes, its attachment_id is added to the Python set(),
  and length of this set is compared with MAX_CONN_CNT when all we establish all connections.
  Then test closes all connections.

  NOTE-1.
  if number of established connections is more than 2047 then 1st of them will not be served by network server
  (this is network server current implementation; it can be changed later, see letter from Vlad, 10.01.2021 15:40).

  NOTE-2.
  If current FB server mode  is 'Classic' then test actually does nothing and console output also remains empty.
  Test in such case looks as 'always successful' but actually it does not performed!
JIRA:        CORE-4439
FBTEST:      bugs.core_4439
NOTES:
    [28.05.2022] pzotov
    Checked on 5.0.0.501, 4.0.1.2692, 3.0.8.33535 (SS/SC/CS)
"""

import pytest
from firebird.qa import *
from firebird.driver.types import DatabaseError

db = db_factory()

act = python_act('db')


@pytest.mark.version('>=3')
@pytest.mark.platform('Windows')
def test_1(act: Action, capsys):
    if act.get_server_architecture() in ('SuperServer', 'SuperClassic'):
        MAX_CONN_CNT=2047

        con_list=[]
        con_set=set()
        con_fail=0
        for i in range(0,MAX_CONN_CNT):
            try:
                con_list.append( act.db.connect() )
                con_set.add( con_list[-1].info.id )
            except DatabaseError as e:
                print(f'Error on attempt to establish connection N {i+1} of {MAX_CONN_CNT+1}:')
                print(e.__str__())
                con_fail=1
            if con_fail:
                break

        ###################
        print(len(con_set))
        ###################
        for c in con_list:
            try:
                c.close()
            except DatabaseError as e:
                print(f'Error on attempt to close connection:')
                print(e.__str__())


        act.expected_stdout = str(MAX_CONN_CNT)
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
    else:
        assert True
