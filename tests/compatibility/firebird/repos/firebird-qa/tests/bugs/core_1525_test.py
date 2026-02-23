#coding:utf-8

"""
ID:          issue-1939
ISSUE:       1939
TITLE:       Computed field + index not working in WHERE
DESCRIPTION:
JIRA:        CORE-1525
FBTEST:      bugs.core_1525
NOTES:
    [25.06.2025] pzotov
    Separated expected PLAN for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test_1 (
      id integer not null,
      last_day date,
      comp_last_day computed by (coalesce(last_day, cast('2999-12-31' as date)))
    );


    insert into test_1 values (1, '2007-10-10');
    insert into test_1 values (2, null);
    commit;

    set list on;

    select *
    from test_1
    where cast ('2007-09-09' as date) < comp_last_day;

    create index idx_1 on test_1 computed by ( coalesce(last_day, cast('2999-12-31' as date)) );
    commit;
    set plan on;

    select *
    from test_1
    where cast ('2007-09-09' as date) < comp_last_day;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=2.0.7')
def test_1(act: Action):

    # 25.06.2025 Separate PLAN depending on major FB version:
    ##########################
    expected_plan = 'PLAN (TEST_1 INDEX (IDX_1))' if act.is_version('<6') else 'PLAN ("PUBLIC"."TEST_1" INDEX ("PUBLIC"."IDX_1"))'

    expected_stdout = f"""
        ID                              1
        LAST_DAY                        2007-10-10
        COMP_LAST_DAY                   2007-10-10
        ID                              2
        LAST_DAY                        <null>
        COMP_LAST_DAY                   2999-12-31

        {expected_plan}

        ID                              1
        LAST_DAY                        2007-10-10
        COMP_LAST_DAY                   2007-10-10
        ID                              2
        LAST_DAY                        <null>
        COMP_LAST_DAY                   2999-12-31
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
