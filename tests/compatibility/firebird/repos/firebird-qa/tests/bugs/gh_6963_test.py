#coding:utf-8

"""
ID:          issue-6963
ISSUE:       6963
TITLE:       grant references not working
DESCRIPTION:
FBTEST:      bugs.gh_6963
NOTES:
    [04.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table tmain_a(
        id int primary key
       ,pid int
    );

    commit;

    create or alter user tmp$gh6963 password '123' using plugin Srp;
    grant select,references on tmain_a to tmp$gh6963;
    grant create table to tmp$gh6963; -- NB: do NOT grant rights to ALTER any and DROP any table!

    commit;
    connect '$(DSN)' user tmp$gh6963 password '123';

    alter table tmain_a add constraint test_fk_self_without_casc
        foreign key(pid) references tmain_a(id)
    ;

    -- must FAIL because current non-privileged user is NOT owner of 'tmain_a' table:
    alter table tmain_a add constraint test_fk_self_with_cascade
        foreign key(pid) references tmain_a(id)
        on update cascade on delete cascade
    ;

    -- ##################################################
    -- Following two statements must pass because user 'tmp$gh6963' has right to create table
    -- and add FK in it with reference to another table (for which he is not owner), no matter
    -- is this FK defined with or without CASCADE option:
    -- ##################################################
    -- 1. This WORKED before fix:
    create table tdetl_a_without_casc(
        id int primary key,
        pid int,
        foreign key(id) references tmain_a(id)
    );

    -- 2. This DID NOT work before fix, failed with:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -CREATE TABLE TDETL_A_WITH_CASC failed
    -- -no permission for ALTER access to TABLE TMAIN_A
    -- -Effective user is TMP$GH6963
    create table tdetl_a_with_casc(
        id int primary key,
        pid int,
        foreign key(id) references tmain_a(id)
            on update cascade on delete cascade
    );

    commit;
    connect '$(DSN)' user sysdba password 'masterkey';

    set list on;
    set count on;
    select rdb$relation_name
    from rdb$relations
    where
        rdb$relation_name starting with upper('tmain_') or
        rdb$relation_name starting with upper('tdetl_')
    order by rdb$relation_name
    ;
    commit;

    drop user tmp$gh6963 using plugin Srp;

"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TMAIN_A failed
    -no permission for ALTER access to TABLE TMAIN_A
    -Effective user is TMP$GH6963
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE TMAIN_A failed
    -no permission for ALTER access to TABLE TMAIN_A
    -Effective user is TMP$GH6963
    RDB$RELATION_NAME               TDETL_A_WITHOUT_CASC
    RDB$RELATION_NAME               TDETL_A_WITH_CASC
    RDB$RELATION_NAME               TMAIN_A
    Records affected: 3
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TMAIN_A" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TMAIN_A"
    -Effective user is TMP$GH6963
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TMAIN_A" failed
    -no permission for ALTER access to TABLE "PUBLIC"."TMAIN_A"
    -Effective user is TMP$GH6963
    RDB$RELATION_NAME               TDETL_A_WITHOUT_CASC
    RDB$RELATION_NAME               TDETL_A_WITH_CASC
    RDB$RELATION_NAME               TMAIN_A
    Records affected: 3
"""

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
