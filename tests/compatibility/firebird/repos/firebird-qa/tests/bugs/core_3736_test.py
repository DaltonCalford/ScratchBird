#coding:utf-8

"""
ID:          issue-4081
ISSUE:       4081
TITLE:       WITH LOCK clause is allowed for users with read-only rights on some table, thus
  blocking others from updating this table
DESCRIPTION:
JIRA:        CORE-3736
FBTEST:      bugs.core_3736
NOTES:
    [28.06.2025] pzotov
    Reimplemented: use variables to be used (via f-notations) in expected_out_* instead of hard-coding.
    Only error messages are checked in this test (rather than both stdout and stderr).

    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

user_1_ro = user_factory('db', name='tmp$c3736_ro', password='tmp$c3736_ro')
user_1_ud = user_factory('db', name='tmp$c3736_ud', password='tmp$c3736_ud')

READ_ONLY_STTM = "select current_user, 'select_read_only', id from t_read_only_for_non_sys"
WITH_LOCK_STTM = "select current_user, 'select_with_lock', id from t_read_only_for_non_sys for update with lock"

test_script = f"""
    set wng off;
    set list on;

    recreate table t_read_only_for_non_sys(id int);
    commit;
    insert into t_read_only_for_non_sys values(1);
    insert into t_read_only_for_non_sys values(2);
    insert into t_read_only_for_non_sys values(3);
    insert into t_read_only_for_non_sys values(4);
    insert into t_read_only_for_non_sys values(5);
    commit;

    grant select on t_read_only_for_non_sys to tmp$c3736_ro;
    commit;
    grant update,delete,select on t_read_only_for_non_sys to tmp$c3736_ud;
    commit;

    set transaction no wait;

    update t_read_only_for_non_sys set id = id where id >= 4;

    set term ^;
    execute block returns( who_am_i type of column sec$users.sec$user_name, my_action varchar(20), what_i_see int )
    as
      declare v_dbname type of column mon$database.mon$database_name;
      declare v_usr_ro type of column sec$users.sec$user_name = 'tmp$c3736_ro';
      declare v_pwd_ro varchar(20) = 'tmp$c3736_ro';
      declare v_connect varchar(255);
    begin
      select 'localhost:' || d.mon$database_name
      from mon$database d
      into v_dbname;

      for
        execute statement q'#{READ_ONLY_STTM}#'
        on external (v_dbname)
        as user (v_usr_ro) password (v_pwd_ro)
        into who_am_i, my_action, what_i_see
      do
        suspend;

      for
        execute statement q'#{WITH_LOCK_STTM}#'
        on external (v_dbname)
        as user (v_usr_ro) password (v_pwd_ro)
        into who_am_i, my_action, what_i_see
      do
        suspend;


    end
    ^
    set term ;^
    rollback;

    set transaction no wait;

    update t_read_only_for_non_sys set id = id where id >= 4;

    set term ^;
    execute block returns( who_am_i type of column sec$users.sec$user_name, my_action varchar(20), what_i_see int )
    as
      declare v_dbname type of column mon$database.mon$database_name;
      declare v_usr_ud type of column sec$users.sec$user_name = 'tmp$c3736_ud';
      declare v_pwd_ud varchar(20) = 'tmp$c3736_ud';
      declare v_connect varchar(255);
    begin
      select 'localhost:' || d.mon$database_name
      from mon$database d
      into v_dbname;

      for
        execute statement q'#{WITH_LOCK_STTM}#'
        on external (v_dbname)
        as user (v_usr_ud) password (v_pwd_ud)
        into who_am_i, my_action, what_i_see
      do
        suspend;

    end
    ^
    set term ;^
    rollback;
"""

# Only error messages are checked in this test:
substitutions = [ ('(-)?At block line(:)?\\s+\\d+.*', ''),
                  ('Data source : Firebird::.*', 'Data source : Firebird::'),
                  ('335544878 : concurrent transaction number.*', '335544878 : concurrent transaction number'),
                ]

act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.es_eds
@pytest.mark.version('>=3.0')
def test_1(act: Action, user_1_ro: User, user_1_ud: User):

    expected_stderr_3x = f"""
        Statement failed, SQLSTATE = 42000
        Execute statement error at isc_dsql_prepare :
        335544352 : no permission for UPDATE access to TABLE T_READ_ONLY_FOR_NON_SYS
        Statement : {WITH_LOCK_STTM}
        Data source : Firebird::

        Statement failed, SQLSTATE = 42000
        Execute statement error at isc_dsql_fetch :
        335544336 : deadlock
        335544451 : update conflicts with concurrent update
        335544878 : concurrent transaction number
        Statement : {WITH_LOCK_STTM}
        Data source : Firebird::
    """

    expected_stderr_5x = f"""
        Statement failed, SQLSTATE = 42000
        Execute statement error at isc_dsql_prepare :
        335544352 : no permission for UPDATE access to TABLE T_READ_ONLY_FOR_NON_SYS
        335545254 : Effective user is {user_1_ro.name.upper()}
        Statement : {WITH_LOCK_STTM}
        Data source : Firebird::
        
        Statement failed, SQLSTATE = 42000
        Execute statement error at isc_dsql_fetch :
        335544336 : deadlock
        335544451 : update conflicts with concurrent update
        335544878 : concurrent transaction number
        Statement : {WITH_LOCK_STTM}
        Data source : Firebird::
    """

    expected_stderr_6x = f"""
        Statement failed, SQLSTATE = 42000
        Execute statement error at isc_dsql_prepare :
        335544352 : no permission for UPDATE access to TABLE "PUBLIC"."T_READ_ONLY_FOR_NON_SYS"
        335545254 : Effective user is {user_1_ro.name.upper()}
        Statement : {WITH_LOCK_STTM}
        Data source : Firebird::

        Statement failed, SQLSTATE = 42000
        Execute statement error at isc_dsql_fetch :
        335544336 : deadlock
        335544451 : update conflicts with concurrent update
        335544878 : concurrent transaction number
        Statement : {WITH_LOCK_STTM}
        Data source : Firebird::
    """

    act.expected_stderr = expected_stderr_3x if act.is_version('<4') else expected_stderr_5x if act.is_version('<6') else expected_stderr_6x
    act.execute(combine_output = False) # ::: NB ::: we need to parse only error messages.
    assert act.clean_stderr == act.clean_expected_stderr
