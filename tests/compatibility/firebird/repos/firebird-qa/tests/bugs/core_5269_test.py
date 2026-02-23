#coding:utf-8

"""
ID:          issue-5547
ISSUE:       5547
TITLE:       FBTRACEMGR should understand 'role <name>' command switch (needed to explicitly
  connect with role with 'TRACE_ANY_ATTACHMENT' privilege)
DESCRIPTION:
  We create two users and one of them is granted with role that allows him to watch other users activity.
  Than we start FBSVCMGR utility with specifying this user and his ROLE so that he can start wathing.
  After this we make trivial query to database from another user.
  Finally, we check trace log that was recived by watching user: this log must contain phrases about
  preprating, starting and executing statements.
JIRA:        CORE-5269
FBTEST:      bugs.core_5269
"""

import pytest
from firebird.qa import *

substitutions = [('\t+', ' '),
                 ('^((?!ROLE_|PREPARE_STATEMENT|EXECUTE_STATEMENT_START|EXECUTE_STATEMENT_FINISH).)*$', ''),
                 ('.*PREPARE_STATEMENT', 'PREPARE_STATEMENT'),
                 ('.*EXECUTE_STATEMENT_START', 'EXECUTE_STATEMENT_START'),
                 ('.*EXECUTE_STATEMENT_FINISH', 'EXECUTE_STATEMENT_FINISH')]

db = db_factory()

user_a = user_factory('db', name='TMP$C5269_1', password='123')
user_b = user_factory('db', name='TMP$C5269_2', password='456')
test_role = role_factory('db', name='role_for_trace_any_attachment')

act = python_act('db', substitutions=substitutions)

expected_stdout_a = """
    ROLE_GRANTEE                    TMP$C5269_2
    ROLE_GRANTOR                    SYSDBA
    ROLE_NAME                       ROLE_FOR_TRACE_ANY_ATTACHMENT
    ROLE_OWNER                      SYSDBA
"""

expected_stdout_b = """
2016-08-06T11:51:38.9360 (2536:01FD0CC8) PREPARE_STATEMENT
2016-08-06T11:51:38.9360 (2536:01FD0CC8) EXECUTE_STATEMENT_START
2016-08-06T11:51:38.9360 (2536:01FD0CC8) EXECUTE_STATEMENT_FINISH
"""

trace = ['time_threshold = 0',
         'log_initfini = false',
         'log_statement_start = true',
         'log_statement_finish = true',
         'max_sql_length = 5000',
         'log_statement_prepare = true',
         ]

test_script = """
set list on;
select p.rdb$user as role_grantee, p.rdb$grantor as role_grantor, r.rdb$role_name as role_name, r.rdb$owner_name as role_owner
from rdb$user_privileges p
join rdb$roles r on p.rdb$relation_name = r.rdb$role_name
where p.rdb$user = upper('TMP$C5269_2');
"""

@pytest.mark.trace
@pytest.mark.version('>=4.0')
def test_1(act: Action, user_a: User, user_b: User, test_role: Role):
    with act.db.connect() as con:
        con.execute_immediate('alter role role_for_trace_any_attachment set system privileges to TRACE_ANY_ATTACHMENT')
        con.commit()
        con.execute_immediate('grant role_for_trace_any_attachment to user TMP$C5269_2')
        con.commit()
    act.expected_stdout = expected_stdout_a
    act.isql(switches=[], input=test_script)
    assert act.clean_stdout == act.clean_expected_stdout
    # Run trace
    with act.trace(db_events=trace), act.db.connect(user='TMP$C5269_1', password='123') as con:
        c = con.cursor()
        c.execute('select current_user from rdb$database')
    # Check
    act.reset()
    act.expected_stdout = expected_stdout_b
    act.trace_to_stdout()
    assert act.clean_stdout == act.clean_expected_stdout
