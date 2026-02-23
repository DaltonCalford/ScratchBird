#coding:utf-8

"""
ID:          issue-1961
ISSUE:       1961
TITLE:       RDB$procedures generator overflow
DESCRIPTION:
  Total number of created procedures is set by 'TOTAL_PROCEDURES_COUNT' variable.

  In order to reduce time:
    * FW is changed OFF
    * test uses LOCAL connection protocol
  Confirmed bug on 2.0.6.13266, got:
    Statement failed, SQLCODE = -607
    unsuccessful metadata update
    -STORE RDB$PROCEDURES failed
    -arithmetic exception, numeric overflow, or string truncation
    -At trigger 'RDB$TRIGGER_28'
  (value of gen_id(rdb$procedures,0) is 32768 when this error raises)
JIRA:        CORE-1544
FBTEST:      bugs.core_1544
NOTES:
[27.05.2022] pzotov
  Re-implemented for work in firebird-qa suite. 
  Checked on: 3.0.8.33535, 4.0.1.2692, 5.0.0.497
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    GEN_RDB_PROC_CURR_VALUE         Expected.
    R                               -9876543321
"""

@pytest.mark.version('>=3')
def test_1(act: Action):

    TOTAL_PROCEDURES_COUNT = 32800

    script='''
        set bail on;
        set list on;
        set term #;
    '''

    for i in range(1,TOTAL_PROCEDURES_COUNT+1):
        script = '\n'.join(
                              (  script
                                ,'create procedure sp_%d returns(r bigint) as begin r = %d; suspend; end#' % (i, (i+1)**3)
                                ,'drop procedure sp_%d#' % i
                              )
                           )

    script = '\n'.join(
                          (   script
                             ,"select iif( gen_id(rdb$procedures,0) >= %d, 'Expected.','UNEXPECTED: ' || gen_id(rdb$procedures,0) || ' - less then %d' ) as GEN_RDB_PROC_CURR_VALUE from rdb$database#" % (TOTAL_PROCEDURES_COUNT, TOTAL_PROCEDURES_COUNT)
                             ,'create procedure sp_1 returns(r bigint) as begin r = -9876543321; suspend; end#'
                             ,'select * from sp_1#'
                             ,'set term ;#'
                          )
                      )
    
    act.expected_stdout = expected_stdout
    act.isql(switches=['-user', act.db.user, '-password', act.db.password, act.db.db_path], connect_db = False, credentials=False, input = script, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
