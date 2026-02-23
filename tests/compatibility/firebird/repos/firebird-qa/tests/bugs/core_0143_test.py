#coding:utf-8

"""
ID:          issue-472
ISSUE:       472
TITLE:       Using where params in SUM return incorrect results
DESCRIPTION:
NOTES:
[30.10.2019] NB: new datatype in FB 4.0 was introduces: numeric(38,0).
  It can lead to additional ident of values when we show them in form "SET LIST ON",
  so we have to ignore all internal spaces - see added 'substitution' section below.
JIRA:        CORE-143
FBTEST:      bugs.core_0143
"""

import pytest
from firebird.qa import *

substitutions = [('[ \t]+', ' ')]

db = db_factory()

test_script = """
    recreate table yeardata
    (
      id integer not null,
      ayear integer,
      avalue numeric( 18, 2),
     constraint pk_yeardata primary key (id)
    );
    commit;

    insert into yeardata(id, ayear, avalue) values (1, 2005, 3.40);
    insert into yeardata(id, ayear, avalue) values (2, 2005, 6.60);
    insert into yeardata(id, ayear, avalue) values (3, 2004, 5.20);
    insert into yeardata(id, ayear, avalue) values (4, 2004, 5.80);
    insert into yeardata(id, ayear, avalue) values (5, 2004, 5.00);
    commit;

    set list on;
    select
         sum(case when ayear = 2004 then avalue else null end) as avalue_2004_1
        ,sum(case when ayear = 2005 then avalue else null end) as avalue_2005_1
    from yeardata;

    set term ^;
    execute block returns( avalue_2004_2 numeric( 18, 2), avalue_2005_2 numeric( 18, 2)) as
    begin
        execute statement
        (
            'select
                 sum(case when ayear = ? then avalue else null end)
                ,sum(case when ayear = ? then avalue else null end)
            from yeardata'
        ) ( 2004, 2005 )
        into avalue_2004_2, avalue_2005_2;
        suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """
    AVALUE_2004_1                   16.00
    AVALUE_2005_1                   10.00
    AVALUE_2004_2                   16.00
    AVALUE_2005_2                   10.00
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

