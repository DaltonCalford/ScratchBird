#coding:utf-8

"""
ID:          issue-3419
ISSUE:       3419
TITLE:       The insert failed because a column definition includes validation constraints. validation error for variable
DESCRIPTION:
JIRA:        CORE-3038
FBTEST:      bugs.core_3038
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    create domain money as numeric(15,4);
    commit;

    create table cl_cashprice (id integer not null, price money not null);
    commit;

    set term ^ ;
    create procedure testp (i_id integer, i_price type of column cl_cashprice.price)
    as
      declare variable goodsprice money;
    begin
      goodsprice = null;
      if (:goodsprice is not null)
        then goodsprice = 6;
    end^
    set term ; ^
    commit;

    execute procedure testp(1, 6);
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
