#coding:utf-8

"""
ID:          monitoring-tables-04
TITLE:       SYSDBA must see all attachments and their transactions, non-privileged user - only those which was of his login.
DESCRIPTION:
FBTEST:      functional.monitoring.04
"""

import pytest
import time
from firebird.qa import *
from firebird.driver import *

db = db_factory()
test_usr1 = user_factory('db', name='u01', password='123')
test_usr2 = user_factory('db', name='u02', password='456')

act = python_act('db', substitutions=[('[=]{1,}', ''), ('[ \t]+', ' ')])


test_expected_stdout = """
    CHECK_NO  WHO_AM_I     WHO_ELSE                  TID_ROWN ISOL_MODE  ISOL_DESCR
    1         U01          U01                              1         1  SNAPSHOT
    1         U01          U01                              2         1  SNAPSHOT
    Records affected: 2

    CHECK_NO  WHO_AM_I     WHO_ELSE                  TID_ROWN ISOL_MODE  ISOL_DESCR
    2         SYSDBA       SYSDBA                           1         1  SNAPSHOT
    2         SYSDBA       U01                              1         1  SNAPSHOT
    2         SYSDBA       U01                              2         1  SNAPSHOT
    2         SYSDBA       U02                              1         2  READ_COMMITTED
    2         SYSDBA       U02                              2         1  SNAPSHOT
    2         SYSDBA       U02                              3         0  CONSISTENCY
    Records affected: 6
"""

@pytest.mark.version('>=3')
def test_1(act: Action, test_usr1: User, test_usr2: User):

    init_script = f"""
        create or alter view v_who as
        select
            current_user as who_am_i
            ,a.mon$user who_else
            ,dense_rank()over(partition by a.mon$user order by t.mon$transaction_id) tid_rown

            -- 07.05.2022: it is recommended to set ReadConsistency = 0 in the firebird.conf for FB 4.x+, see 'firebird-qa/README.rst'.
            -- If it is so then mon$transactions.mon$isolation_mode will be 2 for transactions which were started as READ CONSISTENCY,
            -- but otherwise this value withh be 4. Because of this, we have to check here engine version and if it is NOT '3.' (i.e. 4, 5 and later)
            -- then we must REPLACE '4' with '2':
            ,iif( not rdb$get_context('SYSTEM','ENGINE_VERSION') starting with '3.' and t.mon$isolation_mode = 4, 2, t.mon$isolation_mode) isol_mode

            -- 15.01.2019: removed detailed info about read committed TIL because of read consistency TIL that 4.0 introduces.
            -- Any record with t.mon$isolation_mode = 4 now is considered just as read committed, w/o any detalization (this not much needed here).
            ,decode( t.mon$isolation_mode, 0,'CONSISTENCY', 1,'SNAPSHOT', 2, 'READ_COMMITTED', 3, 'READ_COMMITTED', 4, 'READ_COMMITTED', '??' ) as isol_descr
        from
            mon$attachments a
            LEFT join mon$transactions t using(mon$attachment_id)
        where
            a.mon$attachment_id is distinct from current_connection
            and a.mon$system_flag is distinct from 1 -- remove Cache Writer and Garbage Collector from resultset
        order by a.mon$user, t.mon$transaction_id;
        commit;
        
        grant select on v_who to public;
        commit;
    """
    act.isql(switches=['-q'], input = init_script)
    act.reset()

    with act.db.connect() as con1 \
         ,act.db.connect(user = test_usr1.name, password = test_usr1.password) as con2 \
         ,act.db.connect(user = test_usr1.name, password = test_usr1.password) as con3 \
         ,act.db.connect(user = test_usr2.name, password = test_usr2.password) as con4 \
         ,act.db.connect(user = test_usr2.name, password = test_usr2.password) as con5 \
         ,act.db.connect(user = test_usr2.name, password = test_usr2.password) as con6:

        con1.begin()
        con2.begin()
        con3.begin()

        con4.begin(TPB(isolation=Isolation.READ_COMMITTED_RECORD_VERSION).get_buffer())
        con5.begin(TPB(isolation=Isolation.CONCURRENCY).get_buffer())
        con6.begin(TPB(isolation=Isolation.CONSISTENCY).get_buffer())

        test_script = \
        f'''
            set width who_am_i 12;
            set width who_else 12;
            set width isol_descr 30;
            set count on;
            connect '{act.db.dsn}' user '{test_usr1.name}' password '{test_usr1.password}';
            select 1 as check_no, v.* from v_who v;
            commit;

            connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}' ;

            select 2 as check_no, v.* from v_who v;
            commit;
        '''

        act.expected_stdout = test_expected_stdout
        act.isql(switches=['-q'], input = test_script, connect_db = False )
        assert act.clean_stdout == act.clean_expected_stdout
