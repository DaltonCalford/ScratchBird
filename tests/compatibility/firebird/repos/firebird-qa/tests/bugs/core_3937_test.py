#coding:utf-8

"""
ID:          issue-4270
ISSUE:       4270
TITLE:       eActivate/Activate INDEX or RESTORE not possible with NULL in unique index.
DESCRIPTION:
JIRA:        CORE-3937
FBTEST:      bugs.core_3937
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create generator gen_new_table_id;
    recreate table testtable (
        id integer not null,
        field1 integer not null,
        field2 integer not null,
        field3 varchar(10),
        field4 varchar(10)
    );
    alter table testtable add constraint pk_testtable primary key (id);
    create unique index testtable_idx1 on testtable (field1, field2, field3, field4);
    alter index testtable_idx1 inactive;

    set term ^ ;
    create or alter trigger testtable_bi for testtable
    active before insert position 0
    as
    begin
      if (new.id is null) then
        new.id = gen_id(gen_new_table_id,1);
    end
    ^
    set term ;^
    --commit;

    insert into testtable (field1, field2, field3, field4) values (1, 2, null, null);
    insert into testtable (field1, field2, field3, field4) values (1, 2, '', '');
    commit;

    alter index testtable_idx1 inactive;
    alter index testtable_idx1 active; -- NOTE: could NOT reproduce error on WI-V2.5.1.26351
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
