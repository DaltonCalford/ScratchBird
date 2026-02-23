#coding:utf-8

"""
ID:          issue-3547
ISSUE:       3547
TITLE:       Empty result when select from SP that contains two CTE (second of them with GROUP BY clause) and INNER join
DESCRIPTION:
JIRA:        CORE-3173
FBTEST:      bugs.core_3173
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter procedure sp_main as begin end;
    create or alter procedure sp_aux1 as begin end;
    commit;

    recreate table zzz_tbl (
        id integer,
        company_id integer,
        hire_date date
    );
    insert into zzz_tbl (id, company_id, hire_date)
    values (123456, 654321, '01.10.2004');
    commit;

    set term ^;
    create or alter procedure sp_aux1 returns (val integer)
    as begin
      val=1;
      suspend;
    end
    ^
    commit ^

    create or alter procedure sp_main (
        COMPANY_ID integer,
        A_MONTH_BEG date)
    returns (
        PERSON_ID integer)
    AS
    begin
        for
            with
            inp as(
              select
                 company_id
                ,dateadd(1 month to p1)-1 p2
              from(
               select
                  :company_id company_id,:a_month_beg p1
                  --654321 company_id,cast('01.09.2010' as date) p1
                from
                --rdb$database
                sp_aux1
              )t
            )

            ,person_nfo as
            (
            select n.id person_id,i.p2
            --from inp i join zzz_tbl n on n.company_id = i.company_id
            from zzz_tbl n left join inp i on n.company_id = i.company_id
            group by n.id,p2
            )

            select f.person_id
            from person_nfo f
            join zzz_tbl nt on nt.hire_date <= f.p2
        into :person_id
        do suspend;
    end

    ^ set term ;^
    commit;

"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select * from sp_main(654321, '01.09.2010');
"""

act = isql_act('db', test_script)

expected_stdout = """
    PERSON_ID                       123456
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

