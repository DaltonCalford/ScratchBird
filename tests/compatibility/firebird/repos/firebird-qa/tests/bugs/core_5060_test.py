#coding:utf-8

"""
ID:          issue-5347
ISSUE:       5347
TITLE:       Cannot CREATE VIEW that selects from a system table, despite having all grants
DESCRIPTION:
JIRA:        CORE-5060
FBTEST:      bugs.core_5060
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set wng off;
    set count on;
    set list on;

    create or alter user tmp$5060_u1 password '123' revoke admin role;
    create or alter user tmp$5060_u2 password '456' revoke admin role;

    revoke all on all from tmp$5060_u1;
    revoke all on all from tmp$5060_u2;
    commit;

    set term ^;
    execute block as
    begin
      execute statement 'drop role dba_assistant';
      when any do begin end
    end
    ^
    set term ;^
    commit;

    create role dba_assistant;
    commit;

    grant create view to user tmp$5060_u1;
    grant alter any view to user tmp$5060_u1;
    grant drop any view  to user tmp$5060_u1;

    grant create view to role dba_assistant;
    grant alter any view to role dba_assistant;
    grant drop any view  to role dba_assistant;
    grant dba_assistant to tmp$5060_u2;
    commit;

    connect '$(DSN)' user tmp$5060_u1 password '123';

    create or alter view v_tmp$5060_u1 as select 1 i from rdb$database;
    create or alter view v_tmp$5060_u2 as select v.i as u1, r.i as u2 from v_tmp$5060_u1 v join (select 1 i from rdb$database) r using (i);

    select current_user, current_role, v.* from v_tmp$5060_u1 v;
    select current_user, current_role, v.* from v_tmp$5060_u2 v;

    commit;

    connect '$(DSN)' user tmp$5060_u2 password '456' role 'DBA_ASSISTANT';

    create or alter view v_tmp$5060_r1 as select 1 i from rdb$database;
    create or alter view v_tmp$5060_r2 as select v.i as r1, r.i as r2 from v_tmp$5060_r1 v join (select 1 i from rdb$database) r using (i);

    select current_user, current_role, v.* from v_tmp$5060_r1 v;
    select current_user, current_role, v.* from v_tmp$5060_r2 v;

    commit;

    connect '$(DSN)' user 'SYSDBA' password 'masterkey';
    drop user tmp$5060_u1;
    drop user tmp$5060_u2;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    USER                            TMP$5060_U1
    ROLE                            NONE
    I                               1
    Records affected: 1
    USER                            TMP$5060_U1
    ROLE                            NONE
    U1                              1
    U2                              1
    Records affected: 1
    USER                            TMP$5060_U2
    ROLE                            DBA_ASSISTANT
    I                               1
    Records affected: 1
    USER                            TMP$5060_U2
    ROLE                            DBA_ASSISTANT
    R1                              1
    R2                              1
    Records affected: 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

