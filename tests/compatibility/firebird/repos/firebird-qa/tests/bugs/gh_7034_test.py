#coding:utf-8

"""
ID:          issue-7034
ISSUE:       7034
TITLE:       Server crashes while fetching from a scrollable cursor in PSQL
DESCRIPTION:
FBTEST:      bugs.gh_7034
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^ ;
    create table ca (
        f1 integer,
        f2 integer
    )^

    		commit^

    insert into ca (f1, f2) values (1, 3)^
    insert into ca (f1, f2) values (2, 3)^
    insert into ca (f1, f2) values (2, 4)^
    insert into ca (f1, f2) values (2, 3)^
    commit^

    create or alter procedure gt( privilegy integer)
    returns ( result integer) as
    begin
        result=0;
        suspend;
    end^

    		commit^

    create or alter view psa(result) as
    select (select result from gt(36)) from rdb$database
    ^

    		commit^

    create or alter view vca(f1, f2) as
    select c.* from psa pca
    inner join ca c on pca.result=0
    where pca.result=0
    ^

    		commit^

    set heading off ^

    execute block returns (aid integer) as
        declare bdate type of column ca.f1;
        declare edate type of column ca.f1;
        declare da scroll cursor for
        (
            select ca.id from
            (
                select distinct sacc.f2 id from
                vca sacc
                where 1=1 and f1 between :bdate and :edate
            ) ca
            order by 1
        );
    begin
        select min(f1) from vca into :bdate;
        select max(f1) from vca into :edate;
        open da;
        fetch first from da into :aid; --crash
        suspend;
    end
    ^

"""

act = isql_act('db', test_script)

expected_stdout = """
    3
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
