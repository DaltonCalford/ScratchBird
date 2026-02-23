#coding:utf-8

"""
ID:          issue-2454
ISSUE:       2454
TITLE:       I/O statistics for stored procedures are not accounted in monitoring tables
DESCRIPTION:
    We open TWO cursors within the same attachments and:
    1) make query to procedure inside cursor-1 (trivial count from table there);
    2) ask MON$ tables inside cur-2 with aquiring IO statistics (fetches) for cur-1 statement.
    Number of fetches should be not less then 202400 - see results for 2.1.x, 2.5.x and 3.0 below.
NOTES:
  [17.12.2016]
      Value of fetches in 3.0.2 and 4.0.0 was significantly reduced (~ twice) since ~25-nov-2016
      See results for: 4.0.0.459 and 3.0.2.32641
      Possible reason:
          https://github.com/FirebirdSQL/firebird/commit/8d5b1ff46ed9f22be4a394b941961c522e063ed1
          https://github.com/FirebirdSQL/firebird/commit/dac882c97e2642e260abef475de75c490c5e4bc7
          "Introduced small per-relation cache of physical numbers of data pages.
          It allows to reduce number of pointer page fetches and improves performance."
  [24.07.2025] pzotov
      Changed DB page_size to 8192 because this is minimal size for 6.x
      Changed minimal threshold for fetches depending on major version.
      Actual number of fetches is: 3.x: 102457; 4.x ... 6.x: 102741.
      Checked on 6.0.0.1061; 5.0.3.1686; 4.0.6.3223; 3.0.13.33818
JIRA:        CORE-2017
"""

import pytest
from firebird.qa import *

init_script = """
    create table T (C integer);
    commit;

    set term ^ ;

    execute block
    as
    declare i int = 0;
    begin
      while (i < 100000) do
      begin
        insert into T values (:i);
        i = i + 1;
      end
    end ^
    commit ^

    create procedure sp_test
    returns (i bigint)
    as
    begin
     select count(*)
     from T
     into :i;
     suspend;
    end ^

    commit ^
"""

db = db_factory(page_size = 8192, init=init_script)

act = python_act('db')

expected_stdout = """
    IO statistics for procedure is OK
"""

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):

    ###############
    MIN_FETCHES_CNT = 102456 if act.is_version('<4') else 102740
    ###############

    sql_io = f"""
        select
           iif( i.mon$page_fetches >= {MIN_FETCHES_CNT}, 'IO statistics for procedure is OK',
                'Strange low value for fetches: ' || i.mon$page_fetches || ' -  less than {MIN_FETCHES_CNT=}'
              ) as fetches_result
        from rdb$database r
            left join mon$statements m on
                m.mon$sql_text containing 'select * from sp_test'
                and m.mon$sql_text NOT containing 'mon$statements'
            left join  mon$io_stats i on
                m.mon$stat_id = i.mon$stat_id and i.mon$stat_group = 3
        ;
    """

    act.expected_stdout = expected_stdout
    with act.db.connect() as con:
        stt1 = con.cursor()
        stt2 = con.cursor()
        stt2.execute('select * from sp_test')
        stt2.fetchall()
        stt1.execute(sql_io)
        for row in stt1:
            print(row[0])
        act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
