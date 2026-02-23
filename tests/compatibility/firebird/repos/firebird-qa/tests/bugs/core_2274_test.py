#coding:utf-8

"""
ID:          issue-2700
ISSUE:       2700
TITLE:       MERGE non-standard behaviour, accepts multiple matches
DESCRIPTION:
JIRA:        CORE-2274
FBTEST:      bugs.core_2274
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t_payment_details(operation_id int primary key, person_id int, payment_sum int);
    recreate table t_payment_totals(person_id int primary key, payment_sum int);
    commit;
    insert into t_payment_details(operation_id, person_id, payment_sum) values(0, 10, 0);
    insert into t_payment_details(operation_id, person_id, payment_sum) values(1, 11, 11);
    insert into t_payment_details(operation_id, person_id, payment_sum) values(2, 22, 222);
    insert into t_payment_details(operation_id, person_id, payment_sum) values(3, 11, 3333);
    insert into t_payment_details(operation_id, person_id, payment_sum) values(7, 17, 77777);

    insert into t_payment_totals(person_id, payment_sum) values(10, 100);
    insert into t_payment_totals(person_id, payment_sum) values(11, 111);
    insert into t_payment_totals(person_id, payment_sum) values(22, 222);

    set list on;
    select 'before merge' as msg, e.* from t_payment_totals e order by person_id;

    merge into t_payment_totals t
    using t_payment_details s on s.person_id = t.person_id
    when NOT matched then
        insert(person_id, payment_sum) values( s.person_id, s.payment_sum )
    when MATCHED then
        update set t.payment_sum = t.payment_sum + s.payment_sum
    ;

    select 'after merge' as msg, e.* from t_payment_totals e order by person_id;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    MSG                             before merge
    PERSON_ID                       10
    PAYMENT_SUM                     100

    MSG                             before merge
    PERSON_ID                       11
    PAYMENT_SUM                     111

    MSG                             before merge
    PERSON_ID                       22
    PAYMENT_SUM                     222



    MSG                             after merge
    PERSON_ID                       10
    PAYMENT_SUM                     100

    MSG                             after merge
    PERSON_ID                       11
    PAYMENT_SUM                     111

    MSG                             after merge
    PERSON_ID                       22
    PAYMENT_SUM                     222
"""

expected_stderr = """
    Statement failed, SQLSTATE = 21000
    Multiple source records cannot match the same target during MERGE
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

