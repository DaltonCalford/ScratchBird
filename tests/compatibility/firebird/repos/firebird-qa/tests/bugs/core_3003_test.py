#coding:utf-8

"""
ID:          issue-3385
ISSUE:       3385
TITLE:       Procedure suspend check may cause restore to fail
DESCRIPTION:
JIRA:        CORE-3003
FBTEST:      bugs.core_3003
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='c3003-ods11.fbk')

test_script = """
    set list on;
    select rdb$procedure_name, rdb$procedure_source
    from rdb$procedures
    where upper( rdb$procedure_name ) in ( upper('sp_01'), upper('sp_02') )
    order by rdb$procedure_name
    ;

    select RDB$PROCEDURE_NAME, RDB$PARAMETER_NAME, RDB$PARAMETER_TYPE,RDB$PARAMETER_MECHANISM
    from rdb$procedure_parameters
    where upper( rdb$procedure_name ) in ( upper('sp_01'), upper('sp_02') )
    ;
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$PROCEDURE_SOURCE.*', '')])

expected_stdout = """
    RDB$PROCEDURE_NAME              SP_01
    RDB$PROCEDURE_SOURCE            1a:1
    begin
      n = 1;
    end

    RDB$PROCEDURE_NAME              SP_02
    RDB$PROCEDURE_SOURCE            1a:4
    declare n int;
    begin
      select n from sp_01 into n;
    end

    RDB$PROCEDURE_NAME              SP_01
    RDB$PARAMETER_NAME              N
    RDB$PARAMETER_TYPE              1
    RDB$PARAMETER_MECHANISM         0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

