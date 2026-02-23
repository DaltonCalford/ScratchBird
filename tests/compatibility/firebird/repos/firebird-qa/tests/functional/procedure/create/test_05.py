#coding:utf-8

"""
ID:          procedure.create-05
TITLE:       CREATE PROCEDURE - PSQL Stataments
DESCRIPTION:
FBTEST:      functional.procedure.create.05
"""

import pytest
from firebird.qa import *

db = db_factory()

SP_BODY = """
        declare variable p1 smallint;
    begin
        /* comments */
         p1=1+1;                           /* assigment */
         exception exc_test;                   /* exception */
         execute procedure dummy(:p1);    /* call sp   */
         execute procedure dummy2(:p1) returning_values :p1;
         execute procedure sp_test;         /*recursive call */
         exit;

         for select id from tb into :p1
         do begin
             p1 = p1 + 2;
         end

         insert into tb(id) values(:p1);
         update tb set text='new text' where id=:p1;
         delete from tb where text=:p1+1;
         select id from tb where text='ggg' into :p1;
         if(p1 is not null)then begin
           p1=null;
         end
    
         if (p1 is null) then p1=2;
         else 
             begin
                 p1=2;
             end

         post_event 'my event';
         post_event p1;

         while(p1>30)do begin
          p1=p1-1;
         end
         begin
           exception exc_test;
           when any do p1=45;
         end
    end
"""

test_script = f"""

    create exception exc_test 'test exception';
    create table tb(id int, text varchar(32));
    commit;
    
    set term ^;
    create procedure dummy (id int) as
    begin
      id=id;
    end
    ^

    create procedure dummy2 (id int) returns(newid int) as
    begin
      newid=id;
    end
    ^

    create procedure sp_test as
    {SP_BODY}
    ^
    set term ;^
    commit;
    show procedure sp_test;
"""

act = isql_act('db', test_script, substitutions = [('=====*','')])

@pytest.mark.version('>=3')
def test_1(act: Action):

    expected_stdout_5x = f"""
        Procedure text:
        {SP_BODY}
    """

    expected_stdout_6x = f"""
        Procedure: PUBLIC.SP_TEST
        Procedure text:
        {SP_BODY}
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
