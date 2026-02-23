#coding:utf-8

"""
ID:          issue-3294
ISSUE:       3294
TITLE:       PK index is not used for derived tables
DESCRIPTION:
JIRA:        CORE-2910
FBTEST:      bugs.core_2910
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table r$tmp (
        posting_id integer
    );

    create table tmp (
        posting_id integer not null
    );

    alter table tmp add constraint pk_tmp primary key (posting_id);
    commit;

    set plan on;
    select r.posting_id as r$posting_id, t.posting_id from (
          select posting_id
          from r$tmp
        ) r left join (
            select posting_id from
              (select
                posting_id
              from tmp)
        ) t on r.posting_id = t.posting_id;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN JOIN (R R$TMP NATURAL, T TMP INDEX (PK_TMP))
"""

expected_stdout_6x = """
    PLAN JOIN ("R" "PUBLIC"."R$TMP" NATURAL, "T" "PUBLIC"."TMP" INDEX ("PUBLIC"."PK_TMP"))
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
