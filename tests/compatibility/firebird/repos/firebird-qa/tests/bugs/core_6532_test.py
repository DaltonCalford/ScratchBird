#coding:utf-8

"""
ID:          issue-6759
ISSUE:       6759
TITLE:       Results of concatenation with blob has no info about collation of source
  columns (which are declared with such info)
DESCRIPTION:
JIRA:        CORE-6532
FBTEST:      bugs.core_6532
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create collation name_coll for utf8 from unicode case insensitive;
    commit;

    --create domain dm_name_ci as varchar(10) character set utf8 collate name_coll;
    create domain dm_name_ci as blob sub_type text character set utf8 collate name_coll;
    commit;

    recreate table test(
        id int
       ,c1 varchar(10) character set utf8 collate name_coll
       ,b1 dm_name_ci
       --,b1 blob sub_type text character set utf8 collate name_coll -- same result
    );

    insert into test(id, c1, b1) values(1,'qWE','qWE');
    insert into test(id, c1, b1) values(2,'QWe','QWe');
    insert into test(id, c1, b1) values(3,'qwE','qwE');
    commit;

    set count on;
    -- set echo on;
    set list on;

    ---------------------------------------------

    select id from test
    where
        b1 starting with 'qwe' -- Records affected: 3 // OK
    order by id
    ;


    ---------------------------------------------

    select id from test
    where
        b1 || b1 starting with 'qwe' -- Was wrong: "Records affected: 0"
    order by id
    ;

    --------------------------------------------

    select id from test
    where
        c1 || cast(c1 as blob sub_type text character set utf8) collate name_coll starting with 'qwe' -- Was wrong: "Records affected: 0"
    order by id
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ID                              1
    ID                              2
    ID                              3
    Records affected: 3

    ID                              1
    ID                              2
    ID                              3
    Records affected: 3

    ID                              1
    ID                              2
    ID                              3
    Records affected: 3
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
