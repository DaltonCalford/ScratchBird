#coding:utf-8

"""
ID:          gtcs.window-func-04
TITLE:       Set of miscelaneous tests for verification of windowed functions
DESCRIPTION:
  Statements from this test are added to initial SQL which is stored in: /files/gtcs-window-func.sql
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_WINDOW_FUNC_04.script
FBTEST:      functional.gtcs.window_func_04
"""

import os
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

test_expected_stdout = """
    MSG                             point-1
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     2.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       2.30
    NTH_VALUE                       2.30
    NTH_VALUE                       <null>
    NTH_VALUE                       <null>
    LAST_VALUE                      2.30
    LAST_VALUE                      2.30
    LAST_VALUE                      2.30
    MSG                             point-1
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     4.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       4.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       <null>
    LAST_VALUE                      4.30
    LAST_VALUE                      4.30
    LAST_VALUE                      4.30
    MSG                             point-1
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     6.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       6.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       <null>
    LAST_VALUE                      6.30
    LAST_VALUE                      6.30
    LAST_VALUE                      6.30
    MSG                             point-1
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     8.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       8.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       <null>
    LAST_VALUE                      8.30
    LAST_VALUE                      8.30
    LAST_VALUE                      8.30
    MSG                             point-1
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     10.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       10.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       <null>
    LAST_VALUE                      10.30
    LAST_VALUE                      10.30
    LAST_VALUE                      10.30
    MSG                             point-1
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     2.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       2.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       3.40
    LAST_VALUE                      3.40
    LAST_VALUE                      3.40
    LAST_VALUE                      3.40
    MSG                             point-1
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     4.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       4.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       6.40
    LAST_VALUE                      6.40
    LAST_VALUE                      6.40
    LAST_VALUE                      6.40
    MSG                             point-1
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     6.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       6.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       9.40
    LAST_VALUE                      9.40
    LAST_VALUE                      9.40
    LAST_VALUE                      9.40
    MSG                             point-1
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     8.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       8.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       12.40
    LAST_VALUE                      12.40
    LAST_VALUE                      12.40
    LAST_VALUE                      12.40
    MSG                             point-1
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     10.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       10.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       15.40
    LAST_VALUE                      15.40
    LAST_VALUE                      15.40
    LAST_VALUE                      15.40
    MSG                             point-1
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     2.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       2.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       3.40
    LAST_VALUE                      3.40
    LAST_VALUE                      3.40
    LAST_VALUE                      3.40
    MSG                             point-1
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     4.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       4.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       6.40
    LAST_VALUE                      6.40
    LAST_VALUE                      6.40
    LAST_VALUE                      6.40
    MSG                             point-1
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     6.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       6.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       9.40
    LAST_VALUE                      9.40
    LAST_VALUE                      9.40
    LAST_VALUE                      9.40
    MSG                             point-1
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     8.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       8.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       12.40
    LAST_VALUE                      12.40
    LAST_VALUE                      12.40
    LAST_VALUE                      12.40
    MSG                             point-1
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     10.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       10.30
    NTH_VALUE                       2.30
    NTH_VALUE                       3.40
    NTH_VALUE                       15.40
    LAST_VALUE                      15.40
    LAST_VALUE                      15.40
    LAST_VALUE                      15.40
    MSG                             point-1
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    FIRST_VALUE                     <null>
    FIRST_VALUE                     2.30
    FIRST_VALUE                     2.30
    NTH_VALUE                       <null>
    NTH_VALUE                       2.30
    NTH_VALUE                       2.30
    NTH_VALUE                       <null>
    NTH_VALUE                       3.40
    NTH_VALUE                       3.40
    LAST_VALUE                      <null>
    LAST_VALUE                      <null>
    LAST_VALUE                      <null>
    MSG                             point-2
    PERSON                          1
    SUM                             9.10
    FIRST_VALUE                     41.10
    NTH_VALUE                       41.10
    NTH_VALUE                       33.10
    LAST_VALUE                      9.10
    MSG                             point-2
    PERSON                          2
    SUM                             17.10
    FIRST_VALUE                     41.10
    NTH_VALUE                       41.10
    NTH_VALUE                       33.10
    LAST_VALUE                      17.10
    MSG                             point-2
    PERSON                          3
    SUM                             25.10
    FIRST_VALUE                     41.10
    NTH_VALUE                       41.10
    NTH_VALUE                       33.10
    LAST_VALUE                      25.10
    MSG                             point-2
    PERSON                          4
    SUM                             33.10
    FIRST_VALUE                     41.10
    NTH_VALUE                       41.10
    NTH_VALUE                       33.10
    LAST_VALUE                      33.10
    MSG                             point-2
    PERSON                          5
    SUM                             41.10
    FIRST_VALUE                     41.10
    NTH_VALUE                       41.10
    NTH_VALUE                       <null>
    LAST_VALUE                      41.10
    MSG                             point-3
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    LAG                             <null>
    LEAD                            3.40
    LAG                             <null>
    LEAD                            4.30
    LAG                             <null>
    LAG                             -2.30
    LEAD                            4.30
    MSG                             point-3
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    LAG                             3.40
    LEAD                            6.30
    LAG                             3.40
    LEAD                            6.40
    LAG                             3.40
    LAG                             3.40
    LEAD                            6.40
    MSG                             point-3
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    LAG                             4.30
    LEAD                            6.40
    LAG                             3.40
    LEAD                            8.30
    LAG                             4.30
    LAG                             3.40
    LEAD                            8.30
    MSG                             point-3
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    LAG                             6.40
    LEAD                            9.40
    LAG                             6.40
    LEAD                            10.30
    LAG                             6.40
    LAG                             6.40
    LEAD                            10.30
    MSG                             point-3
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    LAG                             9.40
    LEAD                            12.40
    LAG                             9.40
    LEAD                            15.40
    LAG                             9.40
    LAG                             9.40
    LEAD                            15.40
    MSG                             point-3
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    LAG                             2.30
    LEAD                            3.40
    LAG                             <null>
    LEAD                            6.30
    LAG                             2.30
    LAG                             <null>
    LEAD                            6.30
    MSG                             point-3
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    LAG                             6.30
    LEAD                            6.40
    LAG                             4.30
    LEAD                            9.40
    LAG                             6.30
    LAG                             4.30
    LEAD                            9.40
    MSG                             point-3
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    LAG                             8.30
    LEAD                            9.40
    LAG                             6.40
    LEAD                            12.40
    LAG                             8.30
    LAG                             6.40
    LEAD                            12.40
    MSG                             point-3
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    LAG                             10.30
    LEAD                            12.40
    LAG                             9.40
    LEAD                            15.40
    LAG                             10.30
    LAG                             9.40
    LEAD                            15.40
    MSG                             point-3
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    LAG                             12.40
    LEAD                            15.40
    LAG                             12.40
    LEAD                            <null>
    LAG                             12.40
    LAG                             12.40
    LEAD                            -1.00
    MSG                             point-3
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    LAG                             3.40
    LEAD                            4.30
    LAG                             2.30
    LEAD                            6.40
    LAG                             3.40
    LAG                             2.30
    LEAD                            6.40
    MSG                             point-3
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    LAG                             6.40
    LEAD                            8.30
    LAG                             6.30
    LEAD                            9.40
    LAG                             6.40
    LAG                             6.30
    LEAD                            9.40
    MSG                             point-3
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    LAG                             9.40
    LEAD                            10.30
    LAG                             8.30
    LEAD                            12.40
    LAG                             9.40
    LAG                             8.30
    LEAD                            12.40
    MSG                             point-3
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    LAG                             12.40
    LEAD                            15.40
    LAG                             10.30
    LEAD                            <null>
    LAG                             12.40
    LAG                             10.30
    LEAD                            -1.00
    MSG                             point-3
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    LAG                             15.40
    LEAD                            <null>
    LAG                             12.40
    LEAD                            <null>
    LAG                             15.40
    LAG                             12.40
    LEAD                            -1.00
    MSG                             point-3
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    LAG                             <null>
    LEAD                            2.30
    LAG                             <null>
    LEAD                            3.40
    LAG                             <null>
    LAG                             <null>
    LEAD                            3.40
    MSG                             point-4
    PERSON                          1
    SUM                             9.10
    LAG                             <null>
    LEAD                            17.10
    LAG                             <null>
    LEAD                            33.10
    LAG                             -9.10
    LEAD                            33.10
    MSG                             point-4
    PERSON                          2
    SUM                             17.10
    LAG                             9.10
    LEAD                            25.10
    LAG                             <null>
    LEAD                            41.10
    LAG                             -17.10
    LEAD                            41.10
    MSG                             point-4
    PERSON                          3
    SUM                             25.10
    LAG                             17.10
    LEAD                            33.10
    LAG                             9.10
    LEAD                            <null>
    LAG                             9.10
    LEAD                            -1.00
    MSG                             point-4
    PERSON                          4
    SUM                             33.10
    LAG                             25.10
    LEAD                            41.10
    LAG                             17.10
    LEAD                            <null>
    LAG                             17.10
    LEAD                            -1.00
    MSG                             point-4
    PERSON                          5
    SUM                             41.10
    LAG                             33.10
    LEAD                            <null>
    LAG                             25.10
    LEAD                            <null>
    LAG                             25.10
    LEAD                            -1.00
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    sql_init = (act.files_dir / 'gtcs-window-func.sql').read_text()

    sql_addi= \
    """
        set list on;

        select
            'point-1' as msg,
            e.*,
            first_value(val) over (order by val nulls first, id),
            first_value(val) over (order by val nulls last, id),
            first_value(val) over (partition by e.person order by val nulls last, id),
            nth_value(val, 1) over (order by val nulls first, id),
            nth_value(val, 1) over (order by val nulls last, id),
            nth_value(val, 1) over (partition by e.person order by val nulls last, id),
            nth_value(val, 2) over (order by val nulls first, id),
            nth_value(val, 2) over (order by val nulls last, id),
            nth_value(val, 2) over (partition by e.person order by val nulls last, id),
            last_value(val) over (order by val nulls first, id),
            last_value(val) over (order by val nulls last, id),
            last_value(val) over (partition by e.person order by val nulls last, id)
        from entries e
        order by id;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-2' as msg,
            person,
            sum(val),
            first_value(sum(val)) over (order by person desc),
            nth_value(sum(val), 1) over (order by person desc),
            nth_value(sum(val), 2) over (order by person desc),
            last_value(sum(val)) over (order by person desc)
        from entries
        group by person
        order by person;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-3' as msg,
            e.*,
            lag(val) over (order by val nulls first, id),
            lead(val) over (order by val nulls first, id),
            lag(val, 2) over (order by val nulls first, id),
            lead(val, 3) over (order by val nulls first, id),
            lag(val, 1, -val) over (order by val nulls first, id),
            lag(val, 2, -val) over (order by val nulls first, id),
            lead(val, 3, -1.00) over (order by val nulls first, id)
        from entries e
        order by id;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-4' as msg,
            person,
            sum(val),
            lag(sum(val)) over (order by person),
            lead(sum(val)) over (order by person),
            lag(sum(val), 2) over (order by person),
            lead(sum(val), 3) over (order by person),
            lag(sum(val), 2, -sum(val)) over (order by person),
            lead(sum(val), 3, -1.00) over (order by person)
        from entries
        group by person
        order by person;
    """

    act.expected_stdout = test_expected_stdout

    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ) )

    assert act.clean_stdout == act.clean_expected_stdout
