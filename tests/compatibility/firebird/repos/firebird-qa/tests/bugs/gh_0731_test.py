#coding:utf-8

"""
ID:          issue-731
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/731
TITLE:       coalesce fails with data type varchar and a non ascii value [CORE388]
DESCRIPTION:
NOTES:
    [04.09.2024] pzotov
    The issue seems to be fixed long ago.
    Old FB versions can not be checked on current firebird QA.
    ISQL must work with charset = utf8. Otherwise 'Expected end of statement, encountered EOF' will raise on Linux.

    Checked on all recent 3.x ... 6.x -- all fine.
"""

import pytest
from firebird.qa import *

init_sql = """
    recreate table trans_table
    (
        tcode smallint
        not null,
        code smallint
        not null,
        name varchar(10),
        constraint trans_table_primarykey primary key
        (tcode, code)
    );

    recreate table class1
    (
        class_name varchar(10)
        not null,
        class_num smallint
        not null,
        teacher_id integer,
        constraint pk_class1 primary key (class_name, class_num)
    );

    recreate table class2
    (
        class_name varchar(10)
        not null,
        class_num smallint
        not null,
        teacher_id integer,
        constraint pk_class2 primary key (class_name, class_num)
    );

    set term ^;
    create trigger class1_bi for class1 active before insert position 0 as
        declare name varchar(10);
    begin
        select name from trans_table c where c.tcode=2 and c.code=new.class_name
        into :name;
        new.class_name = case when :name is null then new.class_name else :name end;
        -- new.class_name = coalesce(:name, new.class_name);
    end
    ^

    create trigger class2_bi for class2 active before insert position 0 as
        declare name varchar(10);
    begin
        select name from trans_table c where c.tcode=2 and c.code=new.class_name
        into :name;
        -- new.class_name = case when :name is null then new.class_name else :name end;
        new.class_name = coalesce(:name, new.class_name);
    end
    ^
    set term ;^
    commit;
"""
db = db_factory(init = init_sql, charset='win1252')

test_script = """
    set bail on;
    set list on;
    insert into trans_table(tcode, code, name) values (2, 1, 'à');
    -- passed
    insert into class1(class_name, class_num, teacher_id) values (1, 1, null);
    -- failed
    insert into class2(class_name, class_num, teacher_id) values (1, 1, null);
    select 'Passed' as msg from rdb$database;
"""

act = isql_act('db', test_script, substitutions = [ ('[ \t]+',' ') ])

expected_stdout = """
    MSG Passed
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(charset = 'utf8')
    assert act.clean_stdout == act.clean_expected_stdout

