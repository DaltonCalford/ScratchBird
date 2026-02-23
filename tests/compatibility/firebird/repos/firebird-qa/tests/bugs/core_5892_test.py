#coding:utf-8

"""
ID:          issue-6150
ISSUE:       6150
TITLE:       SQL SECURITY DEFINER context is not properly evaluated for monitoring tables
DESCRIPTION:
  Test is based on ticket sample: we create non-privileged user and allow him to call TWO procedures.
  First SP is declared with DEFINER rights (i.e. with rights of SYSDBA), second - with rights of INVOKER.
  When first SP is called by this (non-privileged!) user then he should see two other connections:
  1) that was done by him (but this is other attachment)
  2) that was done by SYSDBA.
  When second SP is called then this user should see only ONE connection (first from previous list).
  Also this test checks ability to work with new context variable 'EFFECTIVE_USER' from 'SYSTEM' namespace.
JIRA:        CORE-5892
FBTEST:      bugs.core_5892
"""

import pytest
from firebird.qa import *

db = db_factory()

test_user = user_factory('db', name='TMP$C5892', password='123')

act = python_act('db')

expected_stdout = """
    definer_-_who_am_i              TMP$C5892
    definer_-_who_else_here         SYSDBA
    definer_-_effective_user        SYSDBA

    definer_-_who_am_i              TMP$C5892
    definer_-_who_else_here         TMP$C5892
    definer_-_effective_user        SYSDBA

    invoker_-_who_am_i              TMP$C5892
    invoker_-_who_else_here         TMP$C5892
    invoker_-_effective_user        TMP$C5892
"""

sp_definer_ddl = """
    create or alter procedure sp_test_definer returns( another_name varchar(31), another_conn_id int, execution_context varchar(31) ) SQL SECURITY DEFINER
    as
    begin
        execution_context = rdb$get_context('SYSTEM', 'EFFECTIVE_USER');
        for 
            select a.mon$user, a.mon$attachment_id 
            from mon$attachments a 
            where
                a.mon$system_flag is distinct from 1
                and a.mon$attachment_id != current_connection
            order by a.mon$attachment_id 
        into 
            another_name,
            another_conn_id
        do suspend;
    end
"""

sp_invoker_ddl = """
    create or alter procedure sp_test_invoker returns( another_name varchar(31), another_conn_id int, execution_context varchar(31) ) SQL SECURITY INVOKER
    as
    begin
        execution_context = rdb$get_context('SYSTEM', 'EFFECTIVE_USER');
        for 
            select a.mon$user, a.mon$attachment_id 
            from mon$attachments a 
            where 
                a.mon$system_flag is distinct from 1 
                and a.mon$attachment_id != current_connection
                and a.mon$user = current_user
            order by a.mon$attachment_id 
        into 
            another_name,
            another_conn_id
        do suspend;
    end
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, test_user: User, capsys):
    sql_chk_definer = 'select current_user as "definer_-_who_am_i", d.another_name as "definer_-_who_else_here", d.execution_context as "definer_-_effective_user" from rdb$database r left join sp_test_definer d on 1=1'
    sql_chk_invoker = 'select current_user as "invoker_-_who_am_i", d.another_name as "invoker_-_who_else_here", d.execution_context as "invoker_-_effective_user" from rdb$database r left join sp_test_invoker d on 1=1'
    with act.db.connect() as con1, \
         act.db.connect(user=test_user.name, password=test_user.password) as con2, \
         act.db.connect(user=test_user.name, password=test_user.password) as con3:
        #
        con1.execute_immediate(sp_definer_ddl)
        con1.execute_immediate(sp_invoker_ddl)
        con1.commit()
        con1.execute_immediate('grant execute on procedure sp_test_definer to public')
        con1.execute_immediate('grant execute on procedure sp_test_invoker to public')
        con1.commit()
        #
        with con2.cursor() as c2:
            c2.execute(sql_chk_definer)
            act.print_data_list(c2)
        #
        with con2.cursor() as c2:
            c2.execute(sql_chk_invoker)
            act.print_data_list(c2)
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
