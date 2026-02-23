#coding:utf-8

"""
ID:          issue-4141
ISSUE:       4141
TITLE:       fb server die when carry out the LEFT + INNER JOIN
DESCRIPTION:
JIRA:        CORE-3798
FBTEST:      bugs.core_3798
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table table1 (
        id integer not null,
        name varchar(30),
        id_breed integer
    );
    alter table table1 add constraint pk_table1 primary key (id);

    recreate table table2 (
        id integer not null,
        id_table1 integer
    );
    alter table table2 add constraint pk_table2 primary key (id);

    recreate table table3 (
        id integer not null,
        id_breed integer
    );
    alter table table3 add constraint pk_table3 primary key (id);
    commit;

    select table2.id
    from table2
        left join table1 on table1.id = table2.id_table1
        inner join table3 on table1.id_breed = table3.id_breed
    where table1.id = 1
    ;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
