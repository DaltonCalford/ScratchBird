#coding:utf-8

"""
ID:          n/a
TITLE:       Support to multiple SINGULAR subqueries
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/C_SQL_PRED_6.script
NOTES:
    [27.09.2025] pzotov
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

        select t1.empno,t1.last_name, t1.first_name
        from sales_people t1
        where singular
        (
            select * from sales t2 where t1.empno=t2.sales_rep
            and t2.total_value > 6000
            and exists
                (select * from customers t3)
        )
        and
        singular
        (
            select * from sales_perf t4 where t4.empno = t1.empno
        )
        order by t1.empno
        ;
        set count off;
        select 'point-01' as msg from rdb$database;
        set count on;

        select t1.empno,t1.last_name, t1.first_name
        from sales_people t1
        where
            1 = (
                    select count(*) from sales t2 where t1.empno=t2.sales_rep
                    and t2.total_value > 6000
                    and exists (select * from customers t3)
                )
            and
            1 = (select count(*) from sales_perf t4 where t4.empno = t1.empno)
        order by t1.empno
        ;
        set count off;
        select 'point-02' as msg from rdb$database;
    """
    
    act.expected_stdout = """
        MSG point-00
        EMPNO 118
        LAST_NAME Griffon
        FIRST_NAME Ronald
        MSG point-01
        EMPNO 118
        LAST_NAME Griffon
        FIRST_NAME Ronald
        Records affected: 1
        MSG point-02
    """
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ), combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
