#coding:utf-8

"""
ID:          issue-5100
ISSUE:       5100
TITLE:       Regression: GRANT UPDATE(<some_column>) on <T> acts like grant update on ALL columns of <T>
DESCRIPTION:
JIRA:        CORE-4802
FBTEST:      bugs.core_4802
NOTES:
    [03.07.2025] pzotov
    Reimplemented: removed usage of hard-coded values for user and role name.
    SQL schema and double quotes must be taken in acount when specifying data in expected output.
    Checked on 6.0.0.892; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

u_big_bro = user_factory('db', name='BIG_BROTHER', password='123')
u_senior = user_factory('db', name='SENIOR_MNGR', password='456')
u_junior = user_factory('db', name='JUNIOR_MNGR', password='789')
r_senior = role_factory('db', name='FLD_FOR_SENIORS_UPDATER')
r_junior = role_factory('db', name='FLD_FOR_JUNIORS_UPDATER')

substitutions = [ ('[ \t]+', ' '), ('GRANT.*TMP.*', ''), ('-Effective user is.*', '') ]
act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action, u_big_bro: User, u_senior: User, u_junior: User, r_senior: Role, r_junior: Role):

    test_script = f"""
        set wng off;

        recreate table test(fld_for_seniors varchar(70), fld_for_juniors varchar(70));
        commit;

        grant select on test to PUBLIC;

        grant update(fld_for_seniors) on test to {u_big_bro.name};
        commit;

        grant update(fld_for_seniors) on test to {r_senior.name};
        grant update(fld_for_juniors) on test to {r_junior.name};

        grant {r_senior.name} to {u_senior.name};
        grant {r_junior.name} to {u_junior.name};
        commit;

        insert into test values( 'created by ' || upper(current_user), 'created by ' || lower(current_user) );
        commit;
        set list on;

        connect '{act.db.dsn}' user '{u_big_bro.name}' password '{u_big_bro.password}';
        select current_user, current_role from rdb$database;
        update test set fld_for_seniors = 'updated by '||upper(current_user)||', role: '||upper(current_role);
        select * from test;

        update test set fld_for_juniors = 'updated by '||lower(current_user)||', role: '||lower(current_role);
        select * from test;
        commit;
        ---------------------------------------------------------------

        connect '{act.db.dsn}' user '{u_senior.name}' password '{u_senior.password}' role '{r_senior.name}';
        select current_user, current_role from rdb$database;
        update test set fld_for_seniors = 'updated by '||upper(current_user)||', role: '||upper(current_role);
        select * from test;

        update test set fld_for_juniors ='updated by '||lower(current_user)||', role: '||lower(current_role);
        select * from test;
        commit;
        ---------------------------------------------------------------

        connect '{act.db.dsn}' user '{u_junior.name}' password '{u_junior.password}' role '{r_junior.name}';
        select current_user, current_role from rdb$database;
        update test set fld_for_seniors = 'updated by '||upper(current_user)||', role: '||upper(current_role);
        select * from test;

        update test set fld_for_juniors ='updated by '||lower(current_user)||', role: '||lower(current_role);
        select * from test;
        commit;
    """

    FLD_JUNIORS_NAME = 'TEST.FLD_FOR_JUNIORS' if act.is_version('<6') else '"PUBLIC"."TEST"."FLD_FOR_JUNIORS"'
    FLD_SENIORS_NAME = 'TEST.FLD_FOR_SENIORS' if act.is_version('<6') else '"PUBLIC"."TEST"."FLD_FOR_SENIORS"'
    expected_stdout = f"""
        USER                            {u_big_bro.name.upper()}
        ROLE                            NONE
        FLD_FOR_SENIORS                 updated by {u_big_bro.name.upper()}, role: NONE
        FLD_FOR_JUNIORS                 created by {act.db.user.lower()}

        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to COLUMN {FLD_JUNIORS_NAME}

        FLD_FOR_SENIORS                 updated by {u_big_bro.name.upper()}, role: NONE
        FLD_FOR_JUNIORS                 created by {act.db.user.lower()}
        USER                            {u_senior.name.upper()}
        ROLE                            {r_senior.name.upper()}
        FLD_FOR_SENIORS                 updated by {u_senior.name.upper()}, role: {r_senior.name.upper()}
        FLD_FOR_JUNIORS                 created by {act.db.user.lower()}

        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to COLUMN {FLD_JUNIORS_NAME}

        FLD_FOR_SENIORS                 updated by {u_senior.name.upper()}, role: {r_senior.name.upper()}
        FLD_FOR_JUNIORS                 created by {act.db.user.lower()}
        USER                            {u_junior.name.upper()}
        ROLE                            {r_junior.name.upper()}

        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to COLUMN {FLD_SENIORS_NAME}

        FLD_FOR_SENIORS                 updated by {u_senior.name.upper()}, role: {r_senior.name.upper()}
        FLD_FOR_JUNIORS                 created by {act.db.user.lower()}
        FLD_FOR_SENIORS                 updated by {u_senior.name.upper()}, role: {r_senior.name.upper()}
        FLD_FOR_JUNIORS                 updated by junior_mngr, role: fld_for_juniors_updater
    """

    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input =  test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
