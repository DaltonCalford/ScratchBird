#coding:utf-8

"""
ID:          issue-6007
ISSUE:       6007
TITLE:       Conversion error when both GROUP/ORDER BY expressions and WHERE expressions contain literals
DESCRIPTION:
  Minimal requirements for reproduce:
  1) boolean field with reference in WHERE clause;
  2) indexed integer field.
JIRA:        CORE-5743
FBTEST:      bugs.core_5743
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table journal_caisse (
         annule  boolean
        ,periode int
    );
    create index journal_caisse_idx on journal_caisse (periode);
    set planonly;
    select 1 as type_mvt
    from journal_caisse
    where
        annule is false
        and
        (periode = ?)
    group by 1
    ;
    set planonly;

    -- sample from CORE-5749:
    select 'my constant ' as dsc, count( * )
    from rdb$relations a
    where a.rdb$system_flag = -2147483648
    group by 1
    ;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN SORT (JOURNAL_CAISSE INDEX (JOURNAL_CAISSE_IDX))
    PLAN SORT (A NATURAL)
"""

expected_stdout_6x = """
    PLAN SORT ("PUBLIC"."JOURNAL_CAISSE" INDEX ("PUBLIC"."JOURNAL_CAISSE_IDX"))
    PLAN SORT ("A" NATURAL)
"""


@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
