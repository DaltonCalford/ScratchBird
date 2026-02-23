#coding:utf-8

"""
ID:          issue-1605
ISSUE:       1605
TITLE:       "CH" and "LL" are not separate spanish alphabet letters since 1994
DESCRIPTION:
JIRA:        CORE-1179
FBTEST:      bugs.core_1179
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    recreate table test_utf8(id int, esp varchar(10));
    commit;

    insert into test_utf8 values(-2,'CH');
    insert into test_utf8 values(-1,'LL');
    insert into test_utf8 values( 1,'C');
    insert into test_utf8 values( 2,'CA');
    insert into test_utf8 values( 3,'CZ');
    insert into test_utf8 values( 5,'D');

    insert into test_utf8 values( 6,'L');
    insert into test_utf8 values( 7,'LA');
    insert into test_utf8 values( 8,'LZ');
    insert into test_utf8 values(10,'M');
    commit;

    recreate table test_iso( id int, esp varchar(10) character set ISO8859_1 );
    commit;


    insert into test_iso values(-2,'CH');
    insert into test_iso values(-1,'LL');
    insert into test_iso values( 1,'C');
    insert into test_iso values( 2,'CA');
    insert into test_iso values( 3,'CZ');
    insert into test_iso values( 5,'D');

    insert into test_iso values( 6,'L');
    insert into test_iso values( 7,'LA');
    insert into test_iso values( 8,'LZ');
    insert into test_iso values(10,'M');
    commit;

    select id, esp from test_utf8 order by esp collate utf8;
    select id, esp from test_iso order by esp collate es_es;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
              ID ESP
    ============ ==========
               1 C
               2 CA
              -2 CH
               3 CZ
               5 D
               6 L
               7 LA
              -1 LL
               8 LZ
              10 M


              ID ESP
    ============ ==========
               1 C
               2 CA
              -2 CH
               3 CZ
               5 D
               6 L
               7 LA
              -1 LL
               8 LZ
              10 M
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

