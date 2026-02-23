#coding:utf-8

"""
ID:          issue-4349
ISSUE:       4349
TITLE:       Using system domain in procedures arguments/returns cause the proc to be unchangeable
DESCRIPTION:
JIRA:        CORE-4018
FBTEST:      bugs.core_4018
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set blob all;
    set count on;

    set term ^;
    create procedure sp_test returns(info rdb$source) as
    begin
      info = 'foo';
      suspend;
    end
    ^
    set term ;^
    commit;

    select 'point-1' as msg, rdb$parameter_name, rdb$parameter_type, rdb$field_source
    from rdb$procedure_parameters
    where rdb$procedure_name = upper('sp_test')
    order by rdb$parameter_name, rdb$parameter_type;

    select 'point-2' as msg, rdb$procedure_source
    from rdb$procedures
    where rdb$procedure_name = upper('sp_test');
    commit;

    set term ^;
    alter procedure sp_test(whoami rdb$user) returns(memo_info rdb$description) as
    begin
        memo_info = 'bar';
        suspend;
    end
    ^
    set term ;^
    commit;

    select 'point-3' as msg, rdb$parameter_name, rdb$parameter_type, rdb$field_source
    from rdb$procedure_parameters
    where rdb$procedure_name = upper('sp_test')
    order by rdb$parameter_name, rdb$parameter_type;

    select 'point-4' as msg, rdb$procedure_source
    from rdb$procedures
    where rdb$procedure_name = upper('sp_test');
    commit;

    drop procedure sp_test;
    commit;

    -- no rows must be issued:
    select 'point-5' as msg, rdb$parameter_name, rdb$parameter_type, rdb$field_source
    from rdb$procedure_parameters
    where rdb$procedure_name = upper('sp_test');

    -- no rows must be issued:
    select 'point-6' as msg, rdb$procedure_source
    from rdb$procedures
    where rdb$procedure_name = upper('sp_test');
    commit;

"""

act = isql_act('db', test_script, substitutions=[('PROCEDURE_SOURCE .*', '')])

expected_stdout = """
    MSG                             point-1
    RDB$PARAMETER_NAME              INFO
    RDB$PARAMETER_TYPE              1
    RDB$FIELD_SOURCE                RDB$SOURCE
    Records affected: 1


    MSG                             point-2
    RDB$PROCEDURE_SOURCE            1a:1e0
    begin
    info = 'foo';
    suspend;
    end
    Records affected: 1


    MSG                             point-3
    RDB$PARAMETER_NAME              MEMO_INFO
    RDB$PARAMETER_TYPE              1
    RDB$FIELD_SOURCE                RDB$DESCRIPTION

    MSG                             point-3
    RDB$PARAMETER_NAME              WHOAMI
    RDB$PARAMETER_TYPE              0
    RDB$FIELD_SOURCE                RDB$USER
    Records affected: 2


    MSG                             point-4
    RDB$PROCEDURE_SOURCE            1a:1e3
    begin
    memo_info = 'bar';
    suspend;
    end
    Records affected: 1

    Records affected: 0

    Records affected: 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

