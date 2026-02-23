#coding:utf-8

"""
ID:          gtcs.trigger-variable-assignment
TITLE:       Variable in the AFTER-trigger must be allowed for assignment OLD value in it
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_21.script

  AP,2005 - can't assign old.* fields in triggers
FBTEST:      functional.gtcs.trigger_variable_assignment
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table u(a int);
    set term ^;
    create trigger trg_u_aid for u after insert or update or delete as
        declare i int;
    begin
        i = old.a;
    end^
    commit^
"""

act_1 = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act_1: Action):
    act_1.execute()
