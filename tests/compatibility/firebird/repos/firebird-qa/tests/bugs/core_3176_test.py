#coding:utf-8

"""
ID:          issue-3550
ISSUE:       3550
TITLE:       Index is not used when view has "subselect" column
DESCRIPTION:
JIRA:        CORE-3176
FBTEST:      bugs.core_3176
NOTES:
    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table tmp (
      id int not null,
      constraint pk_tmp_1 primary key (id)
    );
    create view tmp_view as
    select 1 as id1, (select 1 from rdb$database) as id2 from rdb$database;
    commit;

    set planonly;
    select * from tmp_view tv left join tmp t on t.id=tv.id2;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN (TV RDB$DATABASE NATURAL)
    PLAN (TV RDB$DATABASE NATURAL)
    PLAN JOIN (TV RDB$DATABASE NATURAL, T INDEX (PK_TMP_1))
"""

expected_stdout_6x = """
    PLAN ("TV" "SYSTEM"."RDB$DATABASE" NATURAL)
    PLAN ("TV" "SYSTEM"."RDB$DATABASE" NATURAL)
    PLAN JOIN ("TV" "SYSTEM"."RDB$DATABASE" NATURAL, "T" INDEX ("PUBLIC"."PK_TMP_1"))
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
