#coding:utf-8

"""
ID:          issue-1862
ISSUE:       1862
TITLE:       Execute statement 'select ....' into new....
DESCRIPTION:
JIRA:        CORE-1444
FBTEST:      bugs.core_1444
"""

import pytest
from firebird.qa import *

init_script = """CREATE EXCEPTION DEXC '???';

CREATE TABLE TEST
(
  C1 char(31),
  C2 char(31)
);

commit;

SET TERM ^ ;

CREATE TRIGGER TEST_BI FOR TEST
ACTIVE BEFORE INSERT POSITION 20
AS
declare variable sql varchar(30000);
declare variable wrk_c1 char(31);
declare variable wrk_c2 char(31);
BEGIN
   /* when new.c1,new.... isn't NULL -> execute statement work ok.
      when new.c1,new.c2, is null -> execute statement has some problem */

   sql=' select first 1 RDB$CHARACTER_SET_NAME,RDB$DEFAULT_COLLATE_NAME
         from RDB$CHARACTER_SETS
         where RDB$CHARACTER_SET_NAME is not null';

   execute statement sql
   into new.c1,new.c2;

   select first 1 RDB$CHARACTER_SET_NAME,RDB$DEFAULT_COLLATE_NAME
   from RDB$CHARACTER_SETS
   where RDB$CHARACTER_SET_NAME is not null
   into :wrk_c1,:wrk_c2;

   if (wrk_c1 is not null and
       new.c1 is null) then
      exception dexc 'Is it a bug ? I think yes, it is.';
END
^
set term ; ^

commit;
"""

db = db_factory(init=init_script)

test_script = """/* Bug */
insert into test(C1,C2) values (NULL,NULL);

/* OK */
insert into test(C1,C2) values ('A','A');

commit;

select * from test;
"""

act = isql_act('db', test_script)

expected_stdout = """
C1                              C2
=============================== ===============================
NONE                            NONE
NONE                            NONE

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

