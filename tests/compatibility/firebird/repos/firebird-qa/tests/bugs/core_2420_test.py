#coding:utf-8

"""
ID:          issue-2836
ISSUE:       2836
TITLE:       Parsing error in EXECUTE STATEMENT with named parameters
DESCRIPTION:
JIRA:        CORE-2420
FBTEST:      bugs.core_2420
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """recreate table sch (cod int, num int, name int, prevcod int, udl char(1), root int);
set term ^;
create or alter procedure getschdet(cod int, datedoc date, datedoc2 date,
       p1 int, p2 int, p3 int, p4 int, p5 int, p6 int,
       p7 int, p8 int, p9 int, p10 int, p11 int, p12 int, p13 int)
  returns (summa int)
as
begin
  suspend;
end ^
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        # Test fails if next raises an exception
        try:
            c.execute("""execute block as
            declare datedoc date;
            declare cod int;
            declare num int;
            declare name int;
            declare summa int;
            begin
             for execute statement (
                 ' select s.cod,s.num, s.name,sum(g.summa) from sch s
                          left join getschdet(s.cod,:datedoc ,:datedoc,0,0,0,0,0,0,0,0,0,0,0,1,3) g on 1=1
                    where s.num in (''50'',''51'') and s.udl<>''У'' and s.root=1
                     and not exists (select s2.cod from sch s2 where s2.prevcod=s.cod)
                   group by 1,2,3') (datedoc := :datedoc)
              into :cod, :num, :name, :summa
              do exit;
            end""")
        except DatabaseError as exc:
            pytest.fail(f"SQL execution failed with: {str(exc)}")



