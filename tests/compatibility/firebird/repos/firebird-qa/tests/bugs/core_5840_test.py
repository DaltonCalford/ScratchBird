#coding:utf-8

"""
ID:          issue-6101
ISSUE:       6101
TITLE:       Ignor of reference privilege
DESCRIPTION:
JIRA:        CORE-5840
FBTEST:      bugs.core_5840
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set wng off;

    create or alter user tmp$c5840 password '123';
    revoke all on all from tmp$c5840;
    grant create table to tmp$c5840;
    commit;

    create table test1(
        id int not null primary key using index test1_pk,
        pid int
    );
    commit;

    create table test3(id int primary key, pid int, constraint test3_fk foreign key(pid) references test1(id) using index test3_fk);
    commit;

    connect '$(DSN)' user 'tmp$c5840' password '123';

    -- this should FAIL, table must not be created at all:
    create table test2(
        id int primary key,
        pid int,
        constraint test2_fk foreign key(pid) references test1(id) using index test2_fk
    );
    commit;

    --set echo on;
    alter table test3 drop constraint test3_fk; -- this WAS allowed (error!)
    commit;

    alter table test1 add constraint test1_fk foreign key(pid) references test1(id) using index test1_fk;
    commit;

    alter table test1 drop constraint test1_fk; -- this was prohibited BEFORE this ticket; we only check this again here
    commit;

    set echo off;

    set list on;
    select rdb$relation_name from rdb$relations where rdb$system_flag is distinct from 1;
    commit;

    -- cleanup:
    connect '$(DSN)' user 'SYSDBA' password 'masterkey'; -- mandatory!
    drop user tmp$c5840;
    commit;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_3x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE TABLE TEST2 failed
    -no permission for REFERENCES access to TABLE TEST1
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST3 failed
    -no permission for ALTER access to TABLE TEST3
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST1 failed
    -no permission for ALTER access to TABLE TEST1
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST1 failed
    -no permission for ALTER access to TABLE TEST1
    RDB$RELATION_NAME               TEST1
    RDB$RELATION_NAME               TEST3
"""

expected_stdout_5x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE TABLE TEST2 failed
    -no permission for REFERENCES access to TABLE TEST1
    -Effective user is TMP$C5840
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST3 failed
    -no permission for ALTER access to TABLE TEST3
    -Effective user is TMP$C5840
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST1 failed
    -no permission for ALTER access to TABLE TEST1
    -Effective user is TMP$C5840
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TEST1 failed
    -no permission for ALTER access to TABLE TEST1
    -Effective user is TMP$C5840
    RDB$RELATION_NAME               TEST1
    RDB$RELATION_NAME               TEST3
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -CREATE TABLE "PUBLIC"."TEST2" failed
    -no permission for REFERENCES access to TABLE "PUBLIC"."TEST1"
    -Effective user is TMP$C5840
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TEST3" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TEST3"
    -Effective user is TMP$C5840
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TEST1" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TEST1"
    -Effective user is TMP$C5840
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TEST1" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TEST1"
    -Effective user is TMP$C5840
    RDB$RELATION_NAME               TEST1
    RDB$RELATION_NAME               TEST3
"""


@pytest.mark.version('>=3.0')
def test_2(act: Action):

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
