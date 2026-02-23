#coding:utf-8

"""
ID:          issue-3584
ISSUE:       3584
TITLE:       The cursor identified in the UPDATE or DELETE statement is not positioned on
  a row. no current record for fetch operation in SELECT query
DESCRIPTION:
JIRA:        CORE-3210
FBTEST:      bugs.core_3210
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3210.fbk')

test_script = """
    set list on;
    select count(*) cnt
    from (
        select
        depozit.number || ' ' ||
        coalesce(
                    iif(
                        nast.bul_id is not null
                        , coalesce(bulgarians.name,'') || ' ' || coalesce(bulgarians.family,'')
                        , coalesce(foreigners.name_cyr,foreigners.name_lat)
                    )
                ,''
                ) as name
        from
            depozit left join nast on nast.id = depozit.nast_id
            left join bulgarians on bulgarians.id = nast.bul_id
            left join foreigners on foreigners.id = nast.for_id
        order by 1 --- ==> no current record for fetch operation
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    CNT                             171
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

