#coding:utf-8

"""
ID:          n/a
TITLE:       Check result of GROUP BY + ORDER BY when applying SINGULAR to result of query with aggregate function
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/C_SQL_PRED_11.script
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

        select t1.sales_rep, sum(t1.total_value)
        from sales t1
        where singular
        (
            select *
            from sales t2
            where
                t2.sales_rep = t1.sales_rep
                and t2.total_value >
                (
                    select avg(t3.total_value)
                    from sales t3
                    where t3.sales_rep = 137
                )
        )
        group by t1.sales_rep
        order by t1.sales_rep descending
        ;

        set count off;
        select 'point-01' as msg from rdb$database;
        set count on;

        select t1.sales_rep, sum(t1.total_value)
        from sales t1
        where 1 =
        (
            select count(*)
            from sales t2
            where
                t2.sales_rep = t1.sales_rep
                and t2.total_value >
                (
                    select avg(t3.total_value)
                    from sales t3
                    where t3.sales_rep = 137
                )
        )
        group by t1.sales_rep
        order by t1.sales_rep descending
        ;

        set count off;
        select 'point-02' as msg from rdb$database;
    """
    
    act.expected_stdout = """
        MSG point-00
        SALES_REP 137
        SUM 141000
        SALES_REP 125
        SUM 151500
        Records affected: 2
        MSG point-01
        SALES_REP 137
        SUM 141000
        SALES_REP 125
        SUM 151500
        Records affected: 2
        MSG point-02
    """
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ), combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
