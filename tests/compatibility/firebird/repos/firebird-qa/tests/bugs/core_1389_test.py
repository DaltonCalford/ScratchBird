#coding:utf-8

"""
ID:          issue-1807
ISSUE:       1807
TITLE:       Indexed MIN/MAX aggregates produce three index reads instead of the expected one indexed read
DESCRIPTION:
JIRA:        CORE-1389
FBTEST:      bugs.core_1389
"""

import pytest
from firebird.qa import *
from firebird.driver import DbInfoCode

init_script = """
    recreate table test(x int, y int);
    commit;
    insert into test(x, y)
    select -1, 1 from (select 1 i from rdb$types rows 200) a, (select 1 i from rdb$types rows 200) b;
    commit;

    create index test_x on test(x);
    create descending index test_y on test(y);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """Number of indexed reads: 1
Number of indexed reads: 1
Number of indexed reads: 1
Number of indexed reads: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        c = con.cursor()
        c.execute("select rdb$relation_id from rdb$relations where trim(rdb$relation_name)=upper('test')")
        test_rel = c.fetchone()[0]
        #
        sql_set=['select min(x) from test',
                 'select x from test order by x rows 1',
                 'select max(y) from test',
                 'select y from test order by y desc rows 1']
        previous_idx_counter = 0
        for cmd in sql_set:
            c.execute(cmd).fetchone()
            counts = con.info.get_info(DbInfoCode.READ_IDX_COUNT)
            for k, cumulative_idx_counter in counts.items():
                if k == test_rel:
                    print('Number of indexed reads:', cumulative_idx_counter - previous_idx_counter)
                    previous_idx_counter = cumulative_idx_counter
        #
        output = capsys.readouterr()
        assert output.out == expected_stdout
