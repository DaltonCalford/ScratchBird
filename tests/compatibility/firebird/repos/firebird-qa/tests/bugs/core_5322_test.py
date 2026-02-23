#coding:utf-8

"""
ID:          issue-5598
ISSUE:       5598
TITLE:       Cascade deletion in self-referencing table could raise "no current record for fetch operation" error
DESCRIPTION:
JIRA:        CORE-5322
FBTEST:      bugs.core_5322
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table tdetl(id int);
    commit;

    recreate table tmain (
        id integer not null,
        some_data integer
    );

    recreate table tdetl (
        id integer not null,
        tmain_id integer,
        some_data integer,
        parent_id integer
    );
    commit;

    insert into tmain (id, some_data) values (1, 10);
    commit;

    insert into tdetl (id, tmain_id, some_data, parent_id) values (1, 1, 555, null);
    insert into tdetl (id, tmain_id, some_data, parent_id) values (2, 1, 222, 1);
    commit;

    alter table tmain add constraint pk_tmain primary key (id);
    alter table tdetl add constraint pk_tdetl primary key (id);
    alter table tdetl add constraint fk_tdetl_1 foreign key (tmain_id) references tmain (id) on delete cascade;
    alter table tdetl add constraint fk_tdetl_2 foreign key (parent_id) references tdetl (id) on delete cascade;
    commit;

    delete from tmain where (id = 1);
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.execute()

