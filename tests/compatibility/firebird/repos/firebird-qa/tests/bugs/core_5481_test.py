#coding:utf-8

"""
ID:          issue-5751
ISSUE:       5751
TITLE:       Available indices are not used in some cases if ORDER BY expression is a filtered one
DESCRIPTION:
JIRA:        CORE-5481
FBTEST:      bugs.core_5481
NOTES:
    [24.09.2023] pzotov
        Execution plan changed in FB 5.x since build 5.0.0.1211 (14-sep-2023).
        Expected output has been splitted on that remains actual for FB 4.x and one that issued for 5.x+.
        Confirmed by dimitr, letter 24.09.2023 13:30

    [01.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.884; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
    [15.01.2026] pzotov
        Execution plan has changed since 6.0.0.1393-f7068cd.
        New plan can be used instead of old one.

        See  e8de18c2, "Some adjustments to the selectivity factors".
        See  565bfcd6, "Fix missing inversion when the bounded ORDER plan is converted into the SORT one..."

        NOTE. Firebird 6.x often tries to SORT(INDEX) instead of ORDER...INDEX if cost looks cheaper.
        See letter from dimitr, 14.01.2026 13:05.
        Checked on 6.0.0.1393-f7068cd.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate view v_test as select 1 x from rdb$database;
    commit;
    recreate table balances
    (
        balanceid bigint not null,
        orgaccountid bigint not null,
        balancedate date not null
    );

    recreate table org_accounts
    (
        orgaccountid bigint not null primary key
    );

    alter table balances add constraint pk_balances primary key (balanceid);
    alter table balances add constraint fk_balances_orgaccounts foreign key (orgaccountid) references org_accounts (orgaccountid);
    alter table balances add constraint balances_balancedate_orgaccount unique (orgaccountid, balancedate);

    create descending index balances_balancedate_desc on balances (balancedate);

    recreate view v_test as
    select b.*
    from balances b
    where
        orgaccountid=18 and
        balancedate<='01.01.2017'
    order by balancedate desc
    rows 1
    ;
    commit;

    set plan on;
    select * from v_test;
    commit;

    ALTER TABLE BALANCES DROP CONSTRAINT BALANCES_BALANCEDATE_ORGACCOUNT;
    commit;

    select * from v_test; -- plan here should remains the same!
    commit;
"""

act = isql_act('db', test_script)

expected_stdout_4x = """
    PLAN (V_TEST B ORDER BALANCES_BALANCEDATE_DESC INDEX (FK_BALANCES_ORGACCOUNTS))
    PLAN (V_TEST B ORDER BALANCES_BALANCEDATE_DESC INDEX (FK_BALANCES_ORGACCOUNTS))
"""

expected_stdout_5x = """
    PLAN SORT (V_TEST B INDEX (BALANCES_BALANCEDATE_ORGACCOUNT))
    PLAN (V_TEST B ORDER BALANCES_BALANCEDATE_DESC INDEX (FK_BALANCES_ORGACCOUNTS))
"""

expected_stdout_6x = """
    PLAN SORT ("PUBLIC"."V_TEST" "B" INDEX ("PUBLIC"."BALANCES_BALANCEDATE_ORGACCOUNT"))
    PLAN SORT ("PUBLIC"."V_TEST" "B" INDEX ("PUBLIC"."FK_BALANCES_ORGACCOUNTS", "PUBLIC"."BALANCES_BALANCEDATE_DESC"))
"""


@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

