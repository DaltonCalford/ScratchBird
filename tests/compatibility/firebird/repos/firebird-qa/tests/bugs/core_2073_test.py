#coding:utf-8

"""
ID:          issue-2508
ISSUE:       2508
TITLE:       Expression indexes bug: incorrect result for the inverted boolean
DESCRIPTION:
JIRA:        CORE-1000
FBTEST:      bugs.core_2073
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table tmp_date1 (
        date1 date,
        date2 date
    );
    commit;
    set term ^;
    execute block as
        declare variable d date;
    begin
        d = '01.01.2008';
        while (d < '01.08.2008') do begin
            insert into tmp_date1(date1, date2) values(:d, :d + 100);
            d = d + 1;
        end
    end^
    set term ;^
    commit;

    create index tmp_date1_idx1 on tmp_date1 computed by (date1+0);
    create index tmp_date1_idx2 on tmp_date1 (date1);
    commit;

    set plan on;
    select count(*) from tmp_date1 t where '01.03.2008' between t.date1+0 and t.date2;
    select count(*) from tmp_date1 t  where '01.03.2008' >= t.date1;
"""


substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN (T INDEX (TMP_DATE1_IDX1))
    COUNT 61
    PLAN (T INDEX (TMP_DATE1_IDX2))
    COUNT 61

"""

expected_stdout_6x = """
    PLAN ("T" INDEX ("PUBLIC"."TMP_DATE1_IDX1"))
    COUNT 61
    PLAN ("T" INDEX ("PUBLIC"."TMP_DATE1_IDX2"))
    COUNT 61
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
