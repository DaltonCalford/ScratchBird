#coding:utf-8

"""
ID:          issue-821
ISSUE:       821
TITLE:       ORDER BY has no effect
DESCRIPTION:
JIRA:        CORE-475
FBTEST:      bugs.core_0475
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure getChilds as begin end;
    recreate table test (
        code integer not null primary key using index test_pk,
        name varchar(2) not null unique,
        parent integer,
        foreign key (parent) references test(code) using index test_fk
    );

    set term ^;
    create or alter procedure getChilds(par integer) returns (code integer,children integer) as
    begin
        for
            select
                m.code, Min(c.code) from
            test m
            left join test c on m.code = c.parent
            where m.parent = :par or (m.parent is null and :par is null)
            group by m.code
        into :code,:children
        do
            suspend;
    end
    ^
    set term ;^
    commit;

    insert into test values (0,'A',null);
    insert into test values (1,'AA',0);
    insert into test values (3,'AB',0);
    insert into test values (4,'AC',0);
    insert into test values (2,'AD',0);
    insert into test values (5,'B',null);
    insert into test values (6,'BA',5);
    insert into test values (7,'BB',5);
    insert into test values (8,'BC',5);
    insert into test values (9,'BD',5);
    insert into test values (10,'BE',5);
    insert into test values (11,'BF',5);

    set list on;

    select *
    from getChilds(0)
    inner join test
    on getChilds.code = test.code
    order by name
    ;


    select *
    from getChilds(0)
    inner join test
    on getChilds.code = test.code
    order by name desc
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CODE                            1
    CHILDREN                        <null>
    CODE                            1
    NAME                            AA
    PARENT                          0

    CODE                            3
    CHILDREN                        <null>
    CODE                            3
    NAME                            AB
    PARENT                          0

    CODE                            4
    CHILDREN                        <null>
    CODE                            4
    NAME                            AC
    PARENT                          0

    CODE                            2
    CHILDREN                        <null>
    CODE                            2
    NAME                            AD
    PARENT                          0



    CODE                            2
    CHILDREN                        <null>
    CODE                            2
    NAME                            AD
    PARENT                          0

    CODE                            4
    CHILDREN                        <null>
    CODE                            4
    NAME                            AC
    PARENT                          0

    CODE                            3
    CHILDREN                        <null>
    CODE                            3
    NAME                            AB
    PARENT                          0

    CODE                            1
    CHILDREN                        <null>
    CODE                            1
    NAME                            AA
    PARENT                          0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

