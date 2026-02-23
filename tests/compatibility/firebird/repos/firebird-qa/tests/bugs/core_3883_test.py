#coding:utf-8

"""
ID:          issue-4220
ISSUE:       4220
TITLE:       Ambiguous field name in the trigger when it does a select from the table
DESCRIPTION:
JIRA:        CORE-3883
FBTEST:      bugs.core_3883
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table regtype (
        code_regtype int constraint pk_regtype primary key,
        name varchar(20),
        multirecord smallint
    );

    recreate table reg (
        code_reg int constraint pk_reg primary key,
        code_regtype int,
        code_horse int
    );

    create exception e_duplicate_reg 'duplicate registration info';

    set term ^ ;
    create or alter trigger reg_bi0 for reg
    active before insert position 0 as
    begin
      if (exists(select 1
                 from reg
                      inner join regtype on reg.code_regtype = regtype.code_regtype
                 where reg.code_horse = new.code_horse and
                       reg.code_regtype = new.code_regtype and
                       regtype.multirecord = 0)) then
        exception e_duplicate_reg;
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
