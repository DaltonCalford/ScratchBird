#coding:utf-8

"""
ID:          issue-3438
ISSUE:       3438
TITLE:       New generators are created with wrong value when more than 32K generators was previously created
DESCRIPTION:
  Confirmed bug on 2.5.0.26074:
  sequence 'g_1' that is created after <TOTAL_SEQUENCES_COUNT> iterations
  has current value = 59049 (instead of expected 0).
NOTES:
  Re-implemented in order to generate SQL script with more than 32K create / get gen_id / drop sequences.
  Total number of created sequences is set by 'TOTAL_SEQUENCES_COUNT' variable.
  In Firebird 3.x+ we have to make EVERY DDL operation (create / drop) in AUTONOMOUS transaction
  because physically appearance of generator in database will be on COMMIT only.
  Discussed with dimitr 06-mar-2015 23:44.

  [27.05.2022] pzotov
    Re-implemented for work in firebird-qa suite. 
    Checked on: 3.0.8.33535, 4.0.1.2692, 5.0.0.497.

JIRA:        CORE-3058
FBTEST:      bugs.core_3058
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    GEN_ID                          0
"""


@pytest.mark.version('>=3')
def test_1(act: Action):

    TOTAL_SEQUENCES_COUNT = 33000

    script='''
        set bail on;
        set list on;
        set term #;
    '''

    for i in range(1,TOTAL_SEQUENCES_COUNT+1):
        script = '\n'.join(
                              (  script
                                ,'create sequence g_%d#' % i
                                ,'execute block as declare c int; begin c = gen_id(g_%d, %d); end#' % (i, i**2)
                                ,'drop sequence g_%d#' % i
                              )
                           )

    script = '\n'.join(
                          (   script
                             ,'set term ;#'
                             ,'create sequence g_%d;' % 1
                             ,'commit;'
                             ,'select gen_id(g_1,0) from rdb$database;'
                          )
                      )


    act.expected_stdout = expected_stdout
    act.isql(switches=['-user', act.db.user, '-password', act.db.password, act.db.db_path], connect_db = False, credentials=False, input = script, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
