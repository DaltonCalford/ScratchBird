#coding:utf-8

"""
ID:          issue-6102
ISSUE:       6102
TITLE:       No permission for SELECT access to BLOB field if a TABLE is accessed using VIEW
DESCRIPTION:
JIRA:        CORE-5841
FBTEST:      bugs.core_5841
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set blob all;

    create or alter user tmp$c5841 password '123';
    commit;

    create table test (
            name_fld varchar(64),
            blob_fld blob,
            bool_fld boolean,
            primary key (name_fld)
           );

    create view v_test as
    select
        name_fld,
        blob_fld,
        bool_fld
        from test
    ;

    grant select on test to view v_test;
    grant select on v_test to public;
    commit;

    insert into test (
        name_fld,
        blob_fld,
        bool_fld)
    values (
        upper('tmp$c5841'),
        lpad('', 70, 'qwerty'),
        true
    );

    commit;

    connect '$(DSN)' user tmp$c5841 password '123';

    set bail off;
    set count on;
    select
         name_fld
         ,blob_fld -- content of this blob field was inaccessible before bug fix
         ,bool_fld
    from v_test
    ;
    rollback;

    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c5841 ;
    commit;
"""

act = isql_act('db', test_script, substitutions=[('BLOB_FLD.*', 'BLOB_FLD')])

expected_stdout = """
    NAME_FLD                        TMP$C5841
    BLOB_FLD                        80:0
    qwertyqwertyqwertyqwertyqwertyqwertyqwertyqwertyqwertyqwertyqwertyqwer
    BOOL_FLD                        <true>
    Records affected: 1
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
