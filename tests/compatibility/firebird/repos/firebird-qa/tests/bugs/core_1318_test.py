#coding:utf-8

"""
ID:          issue-1737
ISSUE:       1737
TITLE:       Error "Identifier ... is too long using multiple (nested) derived tables
DESCRIPTION:
JIRA:        CORE-1318
FBTEST:      bugs.core_1318
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select 0*count(*) cnt
    from (
    select A1.ID
    from(
            select A2.ID from(
                select A3.ID from(
                    select A4.ID from(
                        select A5.ID from(
                            select A6.ID from(
                                select A7.ID from(
                                    select A8.ID from(
                                        select A9.ID from(
                                            select A10.ID from(
                                                select rdb$relations.rdb$relation_id as id from rdb$relations where  rdb$relations.rdb$relation_id = 1
                                                ) as A10
                                                union
                                                select rdb$relations.rdb$relation_id as id from rdb$relations where  rdb$relations.rdb$relation_id = 2
                                            ) as A9
                                            union
                                            select rdb$relations.rdb$relation_id as id from rdb$relations where rdb$relations.rdb$relation_id = 3
                                    ) as A8
                                    union
                                    select rdb$relations.rdb$relation_id as id from rdb$relations where rdb$relations.rdb$relation_id = 4
                                ) as A7
                                union
                                select rdb$relations.rdb$relation_id as id from rdb$relations where rdb$relations.rdb$relation_id = 5
                            ) as A6
                            union
                            select rdb$relations.rdb$relation_id as id from rdb$relations where rdb$relations.rdb$relation_id = 6
                        ) as A5
                        union
                        select rdb$relations.rdb$relation_id as id from rdb$relations where rdb$relations.rdb$relation_id = 7
                    ) as A4
                    union
                    select rdb$relations.rdb$relation_id as id from rdb$relations where rdb$relations.rdb$relation_id = 8
                ) as A3
                union
                select rdb$relations.rdb$relation_id as id from rdb$relations where rdb$relations.rdb$relation_id = 9
            ) as A2
            union
            select rdb$relations.rdb$relation_id as id from rdb$relations where rdb$relations.rdb$relation_id = 10
        ) as A1
        union
        select rdb$relations.rdb$relation_id as id from rdb$relations where rdb$relations.rdb$relation_id = 11
    )
    ;
"""

act = isql_act('db', test_script, substitutions=[('[ ]+', ' ')])

expected_stdout = """
    CNT                             0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

