#coding:utf-8

"""
ID:          issue-3291
ISSUE:       3291
TITLE:       Exception handling with EXECUTE STATEMENT
DESCRIPTION:
  Unable to catch exceptions that are thrown inside a dynamic builded execute block.
JIRA:        CORE-2907
FBTEST:      bugs.core_2907
"""

import pytest
from firebird.qa import *

init_script = """CREATE OR ALTER EXCEPTION EX_TEST 'test';

SET TERM ^ ;
CREATE OR ALTER procedure sp_1
as
declare variable v_stmt varchar(256);
begin

  v_stmt = 'execute block as '||
           'begin '||
           ' exception ex_test; '||
           'end';
  execute statement v_stmt;

end
^
SET TERM ; ^



SET TERM ^ ;
CREATE OR ALTER procedure sp_2
as
begin
  begin
    execute procedure sp_1;

    when exception ex_test do
    begin
      exit;
    end
  end
end
^
SET TERM ; ^

SET TERM ^ ;
CREATE OR ALTER procedure sp_3
as
begin
  begin
    execute procedure sp_1;
    when any do
    begin
      exit;
    end
  end
end
^
SET TERM ; ^
commit;

"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """EXECUTE PROCEDURE SP_2;
EXECUTE PROCEDURE SP_3;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
