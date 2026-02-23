#coding:utf-8

"""
ID:          gtcs.window-func-03
TITLE:       Set of miscelaneous tests for verification of windowed functions
DESCRIPTION:
  Statements from this test are added to initial SQL which is stored in: /files/gtcs-window-func.sql
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_WINDOW_FUNC_03.script
FBTEST:      functional.gtcs.window_func_03
"""

import os
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

test_expected_stdout = """
    MSG                             point-01
    ID                              1
    NAME                            Person 1
    DENSE_RANK                      1
    DENSE_RANK                      5
    RANK                            1
    RANK                            5
    ROW_NUMBER                      1
    ROW_NUMBER                      5
    MSG                             point-01
    ID                              2
    NAME                            Person 2
    DENSE_RANK                      2
    DENSE_RANK                      4
    RANK                            2
    RANK                            4
    ROW_NUMBER                      2
    ROW_NUMBER                      4
    MSG                             point-01
    ID                              3
    NAME                            Person 3
    DENSE_RANK                      3
    DENSE_RANK                      3
    RANK                            3
    RANK                            3
    ROW_NUMBER                      3
    ROW_NUMBER                      3
    MSG                             point-01
    ID                              4
    NAME                            Person 4
    DENSE_RANK                      4
    DENSE_RANK                      2
    RANK                            4
    RANK                            2
    ROW_NUMBER                      4
    ROW_NUMBER                      2
    MSG                             point-01
    ID                              5
    NAME                            Person 5
    DENSE_RANK                      5
    DENSE_RANK                      1
    RANK                            5
    RANK                            1
    ROW_NUMBER                      5
    ROW_NUMBER                      1
    MSG                             point-02
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    DENSE_RANK                      2
    RANK                            2
    ROW_NUMBER                      2
    MSG                             point-02
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    DENSE_RANK                      4
    RANK                            5
    ROW_NUMBER                      5
    MSG                             point-02
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    DENSE_RANK                      5
    RANK                            6
    ROW_NUMBER                      6
    MSG                             point-02
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    DENSE_RANK                      7
    RANK                            9
    ROW_NUMBER                      9
    MSG                             point-02
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    DENSE_RANK                      9
    RANK                            12
    ROW_NUMBER                      12
    MSG                             point-02
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    DENSE_RANK                      3
    RANK                            3
    ROW_NUMBER                      3
    MSG                             point-02
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    DENSE_RANK                      6
    RANK                            7
    ROW_NUMBER                      7
    MSG                             point-02
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    DENSE_RANK                      8
    RANK                            10
    ROW_NUMBER                      10
    MSG                             point-02
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    DENSE_RANK                      10
    RANK                            13
    ROW_NUMBER                      13
    MSG                             point-02
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    DENSE_RANK                      11
    RANK                            15
    ROW_NUMBER                      15
    MSG                             point-02
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    DENSE_RANK                      3
    RANK                            3
    ROW_NUMBER                      4
    MSG                             point-02
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    DENSE_RANK                      6
    RANK                            7
    ROW_NUMBER                      8
    MSG                             point-02
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    DENSE_RANK                      8
    RANK                            10
    ROW_NUMBER                      11
    MSG                             point-02
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    DENSE_RANK                      10
    RANK                            13
    ROW_NUMBER                      14
    MSG                             point-02
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    DENSE_RANK                      11
    RANK                            15
    ROW_NUMBER                      16
    MSG                             point-02
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    DENSE_RANK                      1
    RANK                            1
    ROW_NUMBER                      1
    MSG                             point-03
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    DENSE_RANK                      2
    RANK                            2
    ROW_NUMBER                      2
    MSG                             point-03
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    DENSE_RANK                      1
    RANK                            1
    ROW_NUMBER                      1
    MSG                             point-03
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    DENSE_RANK                      1
    RANK                            1
    ROW_NUMBER                      1
    MSG                             point-03
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    DENSE_RANK                      1
    RANK                            1
    ROW_NUMBER                      1
    MSG                             point-03
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    DENSE_RANK                      1
    RANK                            1
    ROW_NUMBER                      1
    MSG                             point-03
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    DENSE_RANK                      3
    RANK                            3
    ROW_NUMBER                      3
    MSG                             point-03
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    DENSE_RANK                      2
    RANK                            2
    ROW_NUMBER                      2
    MSG                             point-03
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    DENSE_RANK                      2
    RANK                            2
    ROW_NUMBER                      2
    MSG                             point-03
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    DENSE_RANK                      2
    RANK                            2
    ROW_NUMBER                      2
    MSG                             point-03
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    DENSE_RANK                      2
    RANK                            2
    ROW_NUMBER                      2
    MSG                             point-03
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    DENSE_RANK                      4
    RANK                            4
    ROW_NUMBER                      4
    MSG                             point-03
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    DENSE_RANK                      3
    RANK                            3
    ROW_NUMBER                      3
    MSG                             point-03
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    DENSE_RANK                      3
    RANK                            3
    ROW_NUMBER                      3
    MSG                             point-03
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    DENSE_RANK                      3
    RANK                            3
    ROW_NUMBER                      3
    MSG                             point-03
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    DENSE_RANK                      3
    RANK                            3
    ROW_NUMBER                      3
    MSG                             point-03
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    DENSE_RANK                      1
    RANK                            1
    ROW_NUMBER                      1
    MSG                             point-04
    PERSON                          1
    SUM                             4
    SUM                             4
    SUM                             10
    MSG                             point-04
    PERSON                          2
    SUM                             3
    SUM                             3
    SUM                             6
    MSG                             point-04
    PERSON                          3
    SUM                             3
    SUM                             3
    SUM                             6
    MSG                             point-04
    PERSON                          4
    SUM                             3
    SUM                             3
    SUM                             6
    MSG                             point-04
    PERSON                          5
    SUM                             3
    SUM                             3
    SUM                             6
    MSG                             point-05
    SUM                             16
    SUM                             16
    SUM                             34
    MSG                             point-06
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    SRN                             1
    ROW_NUMBER                      1
    MSG                             point-06
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    SRN                             1
    ROW_NUMBER                      2
    MSG                             point-06
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    SRN                             1
    ROW_NUMBER                      3
    MSG                             point-06
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    SRN                             1
    ROW_NUMBER                      4
    MSG                             point-06
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    SRN                             1
    ROW_NUMBER                      5
    MSG                             point-06
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    SRN                             2
    ROW_NUMBER                      6
    MSG                             point-06
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    SRN                             2
    ROW_NUMBER                      7
    MSG                             point-06
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    SRN                             2
    ROW_NUMBER                      8
    MSG                             point-06
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    SRN                             2
    ROW_NUMBER                      9
    MSG                             point-06
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    SRN                             2
    ROW_NUMBER                      10
    MSG                             point-06
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    SRN                             3
    ROW_NUMBER                      11
    MSG                             point-06
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    SRN                             3
    ROW_NUMBER                      12
    MSG                             point-06
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    SRN                             3
    ROW_NUMBER                      13
    MSG                             point-06
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    SRN                             3
    ROW_NUMBER                      14
    MSG                             point-06
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    SRN                             3
    ROW_NUMBER                      15
    MSG                             point-06
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    SRN                             4
    ROW_NUMBER                      16
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    sql_init = (act.files_dir / 'gtcs-window-func.sql').read_text()

    sql_addi= \
    """
        set list on;

        select
            'point-01' as msg,
            p.*,
            dense_rank() over (order by id),
            dense_rank() over (order by id desc),
            rank() over (order by id),
            rank() over (order by id desc),
            row_number() over (order by id),
            row_number() over (order by id desc)
        from persons p
        order by id;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-02' as msg,
            e.*,
            dense_rank() over (order by val nulls first),
            rank() over (order by val nulls first),
            row_number() over (order by val nulls first)
        from entries e
        order by id;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-03' as msg,
            e.*,
            dense_rank() over (partition by person order by val nulls first, dat nulls first),
            rank() over (partition by person order by val nulls first, dat nulls first),
            row_number() over (partition by person order by val nulls first, dat nulls first)
        from entries e
        order by id;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-04' as msg,
            person,
            sum(dr),
            sum(r),
            sum(rn)
        from (
            select
                e.*,
                dense_rank() over (partition by person) dr,
                rank() over (partition by person) r,
                row_number() over (partition by person) rn
              from entries e
        ) x
        group by
            person
        order by person;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-05' as msg,
            sum(dr),
            sum(r),
            sum(rn)
          from (
            select
                e.*,
                dense_rank() over (partition by person) dr,
                rank() over (partition by person) r,
                row_number() over (partition by person) rn
              from entries e
          ) x;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-06' as msg,
            id,
            person,
            dat,
            val,
            sum(rn) srn,
            row_number() over (order by id)
        from (
            select
                e.*,
                row_number() over (partition by person order by id) rn
              from entries e
        ) x
        group by
            id,
            person,
            dat,
            val
        order by id;
    """

    act.expected_stdout = test_expected_stdout

    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ) )

    assert act.clean_stdout == act.clean_expected_stdout
