#coding:utf-8

"""
ID:          issue-5866
ISSUE:       5866
TITLE:       Invalid blob id when add a new blob column of type text and update another field
DESCRIPTION:
JIRA:        CORE-5600
FBTEST:      bugs.core_5600
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    recreate table operation (
        id bigint not null,
        name blob sub_type text character set utf8 default '' not null,
        d_value double precision default 0 not null
    );

    alter table operation add constraint operation_pk_1 primary key ( id );

    insert into operation(id, name, d_value )
    select row_number()over() , 'foo', 1
    from rdb$types
    rows 3
    ;
    commit;

    alter table operation add surname blob sub_type text character set utf8 default 'bar' not null;

    update operation set id=-id where id=2;

    -- NB: do _not_ put "COMMIT;" here!

    set blob all;
    set list on;
    select
         id
        ,name as blob_id_1
        ,surname as blob_id_2
    from operation where abs(id) in (1,2,3);
"""

act = isql_act('db', test_script, substitutions=[('BLOB_ID_.*', '')])

expected_stdout = """
    ID                              1
    foo
    bar

    ID                              -2
    foo
    bar

    ID                              3
    foo
    bar
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
