#coding:utf-8

"""
ID:          issue-6754
ISSUE:       6754
TITLE:       Regression: inline comment of SP parameter with closing parenthesis leads to incorrect SQL when trying to extract metadata
DESCRIPTION:
JIRA:        CORE-6527
FBTEST:      bugs.core_6527
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure sp_test(
        a_base_doc_id int,
        a_base_doc_oper_id int default null -- (one of parameters to standalone procedure)
    )
    as
        declare v_info varchar(100);
    begin

        v_info = 'base_doc='||a_base_doc_id;
    end
    ^
    create or alter function fn_test(
        a_base_doc_id int,
        a_base_doc_oper_id int default null -- (one of parameters to standalone function)
    )
    returns int
    as
        declare v_info varchar(100);
    begin

        v_info = 'base_doc='||a_base_doc_id;
        return 1;
    end
    ^

    create or alter package pg_test as
    begin
        procedure sp_test(
            a_base_doc_id int,
            a_base_doc_oper_id int default null -- (one of parameters to packaged procedure)
        );
        function fn_test(
            a_base_doc_id int,
            a_base_doc_oper_id int default null -- (one of parameters to packaged procedure)
        ) returns int;
    end
    ^
    recreate package body pg_test as
    begin
        procedure sp_test(
            a_base_doc_id int,
            a_base_doc_oper_id int -- (one of parameters to packaged procedure)
        ) as
        begin
           -- nop --
        end

        function fn_test(
            a_base_doc_id int,
            a_base_doc_oper_id int -- (one of parameters to packaged procedure)
        ) returns int as
        begin
            return 1;
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory() # We'll initialize it manually
db_b = db_factory(filename='tmp-issue-6754.fdb')

act = python_act('db')

@pytest.mark.version('>=3.0.8')
def test_1(act: Action, db_b: Database):
    act.isql(switches=[], input=init_script, combine_output = True)
    assert act.clean_stdout == ''
    act.reset()

    meta = act.extract_meta()
    act.reset()

    act.isql(switches=[], use_db=db_b, input=meta, combine_output = True)
    assert act.clean_stdout == ''
    act.reset()
