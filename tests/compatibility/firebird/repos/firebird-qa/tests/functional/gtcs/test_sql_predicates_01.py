#coding:utf-8

"""
ID:          n/a
TITLE:       Support for NOT SINGULAR <subquery>
DESCRIPTION:
  Original test see in:
      https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/C_SQL_PRED_1.script
NOTES:
    [29.09.2025] pzotov
    Test uses pre-created script $QA_ROOT/files/gtcs-shtest-ddl-and-data.sql 
    Original database ($QA_ROOT/backups/gtcs_sh_test.fbk) has problems in RDB$ tables:
    relation 'sales' after restore will have duplicates in rdb$relation_fields.rdb$field_position.
    Query "select * from sales ..." will produce columns in DIFFERENT ORDER on 3.x vs 4.x+.
    Also, this DB was created in dialect 1 which is not actual nowadays.

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
        select * from customers c1
        where not singular (select * from customers c2 where c1.custno = c2.custno)
        ;
        set count off;
        select 'point-01' as msg from rdb$database;

        set count on;
        select * from customers c1
        where
            1 < (select count(*) from customers c2 where c1.custno = c2.custno)
            or 0 = (select count(*) from customers c3 where c3.custno = c3.custno)
        ;
        set count off;
        select 'point-02' as msg from rdb$database;
    """

    act.expected_stdout = """
        MSG point-00
        Records affected: 0
        MSG point-01
        Records affected: 0
        MSG point-02
    """
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ), combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
