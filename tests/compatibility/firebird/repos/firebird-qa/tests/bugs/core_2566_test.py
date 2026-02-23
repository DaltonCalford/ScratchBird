#coding:utf-8

"""
ID:          issue-2976
ISSUE:       2976
TITLE:       internal error [335544384]
DESCRIPTION:
JIRA:        CORE-2566
FBTEST:      bugs.core_2566
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table tab_partner (
        id_partner integer not null,
        stav char(1) not null,
        id_bankucet integer
    );

    create table typ_stav (hodnota char(1) not null);

    create view x_partner (id_partner, stav) as
    select p.id_partner, p.stav
    from tab_partner p
    left join typ_stav tss on p.stav=tss.hodnota
    ;

    insert into tab_partner(id_partner, stav, id_bankucet) values(0, 'A', null);

    select *
    from x_partner;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID_PARTNER                      0
    STAV                            A
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

