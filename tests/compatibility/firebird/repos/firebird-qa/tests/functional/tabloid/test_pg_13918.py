#coding:utf-8

"""
ID:          tabloid.pg-13918
TITLE:       Some _TRIVIAL_ queries allow to specify HAVING without group by, and for such
  case one may to get record from EMPTY source rowset(!)
DESCRIPTION:
  Original issue:
     http://www.postgresql.org/message-id/flat/CAKFQuwYSa5Dzvw8KdxhiUAY+fjbO4DRQ-sDqQXPVexvVoTkvQA@mail.gmail.com#CAKFQuwYSa5Dzvw8KdxhiUAY+fjbO4DRQ-sDqQXPVexvVoTkvQA@mail.gmail.com

     See also http://www.postgresql.org/docs/9.5/interactive/sql-select.html
     ===
       The presence of HAVING turns a query into a grouped query even if there is no GROUP BY clause <...>
       Such a query will emit a single row if the HAVING condition is true, zero rows if it is not true.
     ===
FBTEST:      functional.tabloid.pg_13918
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    -- This compiles OK and, moreover, outputs one record:
    select 123456789 as "Yeah!" from rdb$database where 1=0 having 1=1;
    select 987654321 as "Waw!!" from rdb$database where 1=0 having 1=2;

    -- Following will NOT compile:
    -- select i from (select 1 i from rdb$database) where i<0 having 1=0;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Yeah!                           123456789
    Records affected: 1
    Records affected: 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
