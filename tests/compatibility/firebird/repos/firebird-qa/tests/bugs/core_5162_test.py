#coding:utf-8

"""
ID:          issue-5445
ISSUE:       5445
TITLE:       SEC$ tables and tag/attributes
DESCRIPTION:
JIRA:        CORE-5162
FBTEST:      bugs.core_5162
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set term ^;
    execute block as
    begin
        begin
        execute statement 'drop user tmp$c5162 using plugin Legacy_UserManager' with autonomous transaction;
            when any do begin end
        end

        begin
        execute statement 'drop user tmp$c5162 using plugin Srp' with autonomous transaction;
            when any do begin end
        end
    end^
    set term ;^
    commit;

    create user tmp$c5162 password '123' firstname 'John' using plugin Legacy_UserManager;

    alter user tmp$c5162 using plugin Legacy_UserManager tags (
         key1 = '1val11'
        ,key2 = '2val22'
        ,key3 = '3val33'
    );

    create user tmp$c5162 password '123' firstname 'Mary' using plugin Srp tags (
         key1 = 'val111'
        ,key2 = 'val222'
        ,key3 = 'val333'
    );
    commit;

    --set list on;
    set width usrname 10;
    set width firstnm 10;
    set width su_plg 20;
    set width sa_key 6;
    set width sa_val 6;
    set width sa_plg 20;

    select
         sa.sec$user_name as usrname
        ,sa.sec$key as sa_key
        ,sa.sec$value as sa_val
        ,sa.sec$plugin as sa_plg
    from sec$user_attributes sa
    where sa.sec$user_name ='TMP$C5162'
    order by usrname, sa_plg;

    select
         su.sec$user_name as usrname
        ,su.sec$first_name as firstnm
        ,su.sec$plugin as su_plg
        ,sa.sec$key as sa_key
        ,sa.sec$value as sa_val
        ,sa.sec$plugin as sa_plg
    from sec$users su left join sec$user_attributes sa using(sec$user_name, sec$plugin)
    where su.sec$user_name ='TMP$C5162'
    order by usrname, sa_plg;

    commit;

    -- cleanup:
    drop user tmp$c5162 using plugin Legacy_UserManager;
    drop user tmp$c5162 using plugin Srp;
"""

act = isql_act('db', test_script, substitutions=[('=+', '')])

expected_stdout = """
    USRNAME    SA_KEY SA_VAL SA_PLG
    ========== ====== ====== ====================
    TMP$C5162  KEY1   val111 Srp
    TMP$C5162  KEY2   val222 Srp
    TMP$C5162  KEY3   val333 Srp


    USRNAME    FIRSTNM    SU_PLG               SA_KEY SA_VAL SA_PLG
    ========== ========== ==================== ====== ====== ========
    TMP$C5162  John       Legacy_UserManager   <null> <null> <null>
    TMP$C5162  Mary       Srp                  KEY1   val111 Srp
    TMP$C5162  Mary       Srp                  KEY2   val222 Srp
    TMP$C5162  Mary       Srp                  KEY3   val333 Srp
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

