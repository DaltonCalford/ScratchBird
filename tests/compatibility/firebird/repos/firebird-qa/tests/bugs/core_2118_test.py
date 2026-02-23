#coding:utf-8

"""
ID:          issue-2550
ISSUE:       2550
TITLE:       UPDATE OR INSERT with subquery used in the MATCHING part doesn't insert record
DESCRIPTION:
NOTES:
[14.08.2020]
  Removed usage of generator because gen_id() result differs in FB 4.x vs previous versions
  since fixed core-6084. Use hard-coded value for ID that is written into table MACRO.
JIRA:        CORE-2118
FBTEST:      bugs.core_2118
"""

import pytest
from firebird.qa import *

init_script = """
    create table macro (
        id integer not null,
        t1 integer,
        code varchar(50)
    );

    create table param (
        id integer not null,
        p1 integer
    );

    commit;
    alter table macro add constraint pk_macro primary key (id);
    alter table param add constraint pk_param primary key (id);
    commit;
    alter table macro add constraint fk_macro_1 foreign key (t1) references param (id);
    commit;

    insert into param (id, p1) values (2, 11);
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """
    update or insert into macro (id, t1, code) values ( 1, (select id from param where p1 = 11), 'fsdfdsf') matching (t1);
    commit;
    set list on;
    select id, t1, code from macro;
"""

act = isql_act('db', test_script, substitutions=[('=.*', ''), ('[ \t]+', ' ')])

expected_stdout = """
    ID                              1
    T1                              2
    CODE                            fsdfdsf
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

