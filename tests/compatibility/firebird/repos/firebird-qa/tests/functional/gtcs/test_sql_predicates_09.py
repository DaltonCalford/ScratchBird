#coding:utf-8

"""
ID:          n/a
TITLE:       Applying SINGULAR to result of query with aggregate function
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/C_SQL_PRED_9.script
NOTES:
    [29.09.2025] pzotov
    Checked on 6.0.0.1282; 5.0.4.1715; 4.0.7.3235; 3.0.14.33826.
"""
import os
import pytest
from firebird.qa import *

db = db_factory()

substitutions = [ ('[ \t]+', ' '), ]
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    sql_init = (act.files_dir / 'gtcs-shtest-ddl-and-data.sql').read_text()
    sql_addi = """
        set list on;
        select 'point-00' as msg from rdb$database;

        set count on;
        select t1.custno, t1.sales_rep, t1.total_value
        from sales t1
        where singular
        (
            select *
            from sales t2
            where t2.sales_rep = t1.sales_rep
            and t2.total_value >
            (
                select avg(t3.total_value)
                from sales t3
                where t3.sales_rep = 137
            )
        )
        order by t1.custno
        ;
        set count off;
        select 'point-01' as msg from rdb$database;
        set count on;

        select t1.custno, t1.sales_rep, t1.total_value
        from sales t1
        where 1 =
        (
            select count(*)
            from sales t2
            where t2.sales_rep = t1.sales_rep
            and t2.total_value >
            (
                select avg(t3.total_value)
                from sales t3
                where t3.sales_rep = 137
            )
        )
        order by t1.custno
        ;
        set count off;
        select 'point-02' as msg from rdb$database;
    """
    
    act.expected_stdout = """
        MSG point-00

        CUSTNO 102
        SALES_REP 125
        TOTAL_VALUE 18500
        CUSTNO 104
        SALES_REP 137
        TOTAL_VALUE 38000
        CUSTNO 105
        SALES_REP 125
        TOTAL_VALUE 25500
        CUSTNO 106
        SALES_REP 137
        TOTAL_VALUE 18000
        CUSTNO 108
        SALES_REP 125
        TOTAL_VALUE 48500
        CUSTNO 109
        SALES_REP 125
        TOTAL_VALUE 15500
        CUSTNO 109
        SALES_REP 125
        TOTAL_VALUE 43500
        CUSTNO 113
        SALES_REP 137
        TOTAL_VALUE 85000
        Records affected: 8
        
        MSG point-01

        CUSTNO 102
        SALES_REP 125
        TOTAL_VALUE 18500
        CUSTNO 104
        SALES_REP 137
        TOTAL_VALUE 38000
        CUSTNO 105
        SALES_REP 125
        TOTAL_VALUE 25500
        CUSTNO 106
        SALES_REP 137
        TOTAL_VALUE 18000
        CUSTNO 108
        SALES_REP 125
        TOTAL_VALUE 48500
        CUSTNO 109
        SALES_REP 125
        TOTAL_VALUE 15500
        CUSTNO 109
        SALES_REP 125
        TOTAL_VALUE 43500
        CUSTNO 113
        SALES_REP 137
        TOTAL_VALUE 85000
        Records affected: 8

        MSG point-02
    """
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ), combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
