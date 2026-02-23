#coding:utf-8

"""
ID:          issue-6758
ISSUE:       6758
TITLE:       COMPUTED BY column looses charset and collate of source field <F> when <F> is
  either of type BLOB or VARCHAR casted to BLOB
DESCRIPTION:
JIRA:        CORE-6531
FBTEST:      bugs.core_6531
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create collation name_coll for utf8 from unicode case insensitive;
    commit;

    create domain dm_name_ci as blob sub_type text character set utf8 collate name_coll;
    commit;

    recreate table test(
        id int
       ,b1 dm_name_ci
       ,calc_b1 computed by ( b1 )
       -----------------------------------------------------
       ,c1 varchar(10) character set utf8 collate name_coll
       ,calc_c1 computed by ( cast(c1 as blob sub_type text character set utf8) collate name_coll ) -- ==> SQLDA: "charset: 0 NONE"
       --,calc_c1 computed by ( (select list(x.c1) from test x where x.id = test.id) ) -- ==> SQLDA: "charset: 0 NONE"
       --,calc_c1 computed by ( cast(c1 as varchar(10) character set utf8) collate name_coll ) -- ==> SQLDA: "charset: 32260 UTF8" // OK
    );

    insert into test(id, b1, c1) values(1,'qWE','qWE');
    insert into test(id, b1, c1) values(2,'QWe','QWe');
    insert into test(id, b1, c1) values(3,'qwE','qwE');
    commit;

    set list on;
    set count on;
    -- set echo on;

    ---------------------------------------------

    select id
    from test where calc_c1 starting with 'qwe'
    order by id;

    ---------------------------------------------

    select id
    from test where calc_b1 starting with 'qwe'
    order by id;

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
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
