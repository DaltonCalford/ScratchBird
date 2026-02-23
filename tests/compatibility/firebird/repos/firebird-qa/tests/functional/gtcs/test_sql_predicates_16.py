#coding:utf-8

"""
ID:          n/a
TITLE:       Check of applying SINGULAR to result produced by a VIEW
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/C_SQL_PRED_16.script
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

        select *
        from dept_budget v1
        where singular
        (
            select * from fullemp v2
            where v2.department = v1.department
        )
        order by v1.deptno
        ;

        set count off;
        select 'point-01' as msg from rdb$database;
        set count on;

        select *
        from dept_budget v1
        where
            1 = (
                    select count(*) from fullemp v2
                    where v2.department = v1.department
                )
        order by v1.deptno
        ;

        set count off;
        select 'point-02' as msg from rdb$database;
    """
    
    act.expected_stdout = """
        MSG point-00
        DEPTNO 000
        DEPARTMENT Corporate Headquarters
        MGRNO 120
        REPORTS 1
        DEPT_LEVEL 1
        HEAD_DEPT 000
        DEPTNO 330
        DEPARTMENT Accounting
        MGRNO 112
        REPORTS 1
        DEPT_LEVEL 3
        HEAD_DEPT 300
        Records affected: 2
        MSG point-01
        DEPTNO 000
        DEPARTMENT Corporate Headquarters
        MGRNO 120
        REPORTS 1
        DEPT_LEVEL 1
        HEAD_DEPT 000
        DEPTNO 330
        DEPARTMENT Accounting
        MGRNO 112
        REPORTS 1
        DEPT_LEVEL 3
        HEAD_DEPT 300
        Records affected: 2
        MSG point-02
    """
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ), combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
