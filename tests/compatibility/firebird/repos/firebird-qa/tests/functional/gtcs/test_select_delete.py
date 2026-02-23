#coding:utf-8

"""
ID:          n/a
TITLE:       
DESCRIPTION:
  Original test see in:
      https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/SELECT_DELETE_ISQL.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
      https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
      (this database already present in 'files' subdir with name 'gtcs_sp1.fbk')
NOTES:
    [27.09.2025] pzotov
    Checked on 6.0.0.1282; 5.0.4.1715; 4.0.7.3235; 3.0.14.33826.
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup = 'gtcs_sp1.fbk')

test_script = """
    set term ^; 
    create procedure proc1 returns (x varchar(5)) as
    begin
       for select sno from sp into :x
       do
        begin 
         suspend;
         delete from s where sno = :x;
        end
    end
    ^
    create procedure proc2 returns (x varchar(5)) as 
    begin 
       for select sno from sp into :x 
       do 
        begin  
         suspend; 
        end    
         delete from s where sno = :x;
    end 
    ^ 
    create procedure proc3 returns (x varchar(5)) as  
    begin  
       for select sno from sp into :x  
       do  
        begin   
         delete from s where sno = :x;
         suspend;  
        end     
    end  
    ^  
    set term ;^  
    set list on;
    select 'point-00' as msg from rdb$database;
    select * from proc1;
    select * from s;
    select 'point-01' as msg from rdb$database;
    rollback;
    select * from proc2;
    select * from s;
    select 'point-02' as msg from rdb$database;
    rollback;
    select * from proc3;
    select * from s;
    select 'point-03' as msg from rdb$database;
    rollback;
"""

substitutions = [ ('[ \t]+', ' '), ]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = """
        MSG                             point-00

        X                               S1
        X                               S1
        X                               S2
        X                               S2
        X                               S4
        X                               S4
        SNO                             S3
        SNAME                           Blake
        STATUS                          30
        CITY                            Paris
        SNO                             S5
        SNAME                           Adams
        STATUS                          30
        CITY                            Athens
        MSG                             point-01

        X                               S1
        X                               S1
        X                               S2
        X                               S2
        X                               S4
        X                               S4
        SNO                             S1
        SNAME                           Smith
        STATUS                          20
        CITY                            London
        SNO                             S2
        SNAME                           Jones
        STATUS                          10
        CITY                            Paris
        SNO                             S3
        SNAME                           Blake
        STATUS                          30
        CITY                            Paris
        SNO                             S5
        SNAME                           Adams
        STATUS                          30
        CITY                            Athens
        MSG                             point-02

        X                               S1
        X                               S1
        X                               S2
        X                               S2
        X                               S4
        X                               S4
        SNO                             S3
        SNAME                           Blake
        STATUS                          30
        CITY                            Paris
        SNO                             S5
        SNAME                           Adams
        STATUS                          30
        CITY                            Athens
        MSG                             point-03
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
