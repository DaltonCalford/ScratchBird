#coding:utf-8

"""
ID:          procedure.create-10
ISSUE:       1161
TITLE:       Type Flag for Stored Procedures
DESCRIPTION:
FBTEST:      functional.procedure.create.16
JIRA:        CORE-779
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    create or alter procedure with_suspend (nom1 varchar(20) character set iso8859_1 collate fr_fr)
    returns (nom3 varchar(20) character set iso8859_1 collate iso8859_1) as
        declare variable nom2 varchar(20) character set iso8859_1 collate fr_ca;
    begin
        nom2=nom1;
        nom3=nom2;
        suspend;
    end ^

    create or alter procedure no_suspend returns(p1 smallint) as
    begin
        p1=1;
    end ^
    set term ;^
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    set list on;
    select p.rdb$procedure_name, p.rdb$procedure_type
    from rdb$procedures p
    where upper(p.rdb$procedure_name) in ( upper('with_suspend'), upper('no_suspend') )
    order by 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$PROCEDURE_NAME              NO_SUSPEND
    RDB$PROCEDURE_TYPE              2

    RDB$PROCEDURE_NAME              WITH_SUSPEND
    RDB$PROCEDURE_TYPE              1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
