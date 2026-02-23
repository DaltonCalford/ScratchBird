#coding:utf-8

"""
ID:          issue-6698
ISSUE:       6698
TITLE:       Comments before the first line of code are removed (when extract metadata)
DESCRIPTION:
  Lot of comments (multi-line blocks) are inserted before each clause/token of created SP
  (in its declaration of name, input and output parameters, sections of variables and after
  final 'end').
  Comments before 'AS' clause will be removed after SP commit (i.e. they must not be shown
  when metadata is axtracted - at least for current versions of FB).
  First comment _after_ 'AS' clause (and before 1st 'declare variable' statement) *must*
  be preserved.
  Comment after final 'end' of procedure must also be preserved.
JIRA:        CORE-6466
FBTEST:      bugs.core_6466
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure sp_test1
    		/*
    		comment-100
    		*/
    	(
    		/*
    		comment-150
    		*/
    	int_a int
    		/*
    		comment-200
    		*/
    	 ,
    		/*
    		comment-230
    		*/
    	 int_b int
    		/*
    		comment-250
    		*/
    	)
    		/*
    		comment-300
    		*/
    returns
    		/*
    		comment-400
    		*/
    	(
    		/*
    		comment-500
    		*/
          out_a
    		/*
    		comment-600
    		*/
    	  int
    		/*
    		comment-700
    		*/
    	 ,out_b int
    		/*
    		comment-750
    		*/
    	)
    		/*
    		comment-800
    		*/
    as
    		/*
    		comment-900
    		*/
    	declare
    		/*
    		comment-910
    		*/
    	int_c
    		/*
    		comment-920
    		*/
    	int  = 2
    		/*
    		comment-930
    		*/
    	;
    		/*
    		comment-940
    		*/
    begin
    	/* code */
    	int_b=int_a+int_c+2;
    	suspend;
    end
    		/*
    		comment-950
    		*/
    ^
    commit
    ^
    set term ;^
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('^((?!comment-[\\d]+).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    comment-900
    comment-910
    comment-920
    comment-930
    comment-940
    comment-950
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.extract_meta()
    assert act.clean_stdout == act.clean_expected_stdout
