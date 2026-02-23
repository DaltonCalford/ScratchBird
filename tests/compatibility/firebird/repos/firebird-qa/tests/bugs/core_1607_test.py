#coding:utf-8

"""
ID:          issue-2028
ISSUE:       2028
TITLE:       Correlated subquery is optimized badly if it depends on the union stream
DESCRIPTION:
JIRA:        CORE-1607
FBTEST:      bugs.core_1607
NOTES:
    [23.03.2025] pzotov
    Separated expected_out because plans differ on 6.x vs previous versions since commit fc12c0ef
    ("Unnest IN/ANY/EXISTS subqueries and optimize them using semi-join algorithm (#8061)").
    Checked on 6.0.0.687-730aa8f; 5.0.3.1633-25a0817 
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;

    select 1
    from ( select rdb$relation_name, ( select 1 from rdb$database ) as c from rdb$relations ) r
    where exists ( select * from rdb$relation_fields f where f.rdb$relation_name = r.rdb$relation_name );

    select 1
    from (
      select * from rdb$relations
      union all
      select * from rdb$relations
    ) r
    where exists ( select * from rdb$relation_fields f where f.rdb$relation_name = r.rdb$relation_name );

    select ( select first 1 r.rdb$relation_name
             from rdb$relations r
             where r.rdb$relation_id = d.rdb$relation_id - 1 )
    from (
      select * from rdb$database
      union all
      select * from rdb$database
    ) d;
"""

act = isql_act('db', test_script)


@pytest.mark.version('>=3.0')
def test_1(act: Action):
    if act.is_version('<6'):
        expected_stdout = """
            PLAN (R RDB$DATABASE NATURAL)
            PLAN (F INDEX (RDB$INDEX_4))
            PLAN (R RDB$RELATIONS NATURAL)

            PLAN (F INDEX (RDB$INDEX_4))
            PLAN (R RDB$RELATIONS NATURAL, R RDB$RELATIONS NATURAL)

            PLAN (R INDEX (RDB$INDEX_1))
            PLAN (D RDB$DATABASE NATURAL, D RDB$DATABASE NATURAL)
        """
    else:
        expected_stdout = """
            PLAN ("R" "SYSTEM"."RDB$DATABASE" NATURAL)
            PLAN HASH ("R" "SYSTEM"."RDB$RELATIONS" NATURAL, "F" NATURAL)
            PLAN HASH ("R" "SYSTEM"."RDB$RELATIONS" NATURAL, "R" "SYSTEM"."RDB$RELATIONS" NATURAL, "F" NATURAL)
            PLAN ("R" INDEX ("SYSTEM"."RDB$INDEX_1"))
            PLAN ("D" "SYSTEM"."RDB$DATABASE" NATURAL, "D" "SYSTEM"."RDB$DATABASE" NATURAL)
        """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

