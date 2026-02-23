#coding:utf-8

"""
ID:          gtcs.ref-integ-drop-fk-then-pk
TITLE:       Outcome must be SUCCESS if first we drop FK and after this PK constraint
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/REF_INT.1.ISQL.script

  This test uses pre-created script ( <QA_ROOT>/files/gtcs-ref-integ-init.sql ) which creates two
  tables with PK/FK referencing constraint(parent = department, child = employee).
  We DROP constraints (first FK, then PK) and verify that one may to do DML with both tables
  which had to be failed before with because of PK/FK violation.

  Checked on 4.0.1.2692.

FBTEST:      functional.gtcs.ref_integ_drop_fk_then_pk
"""

import pytest
from firebird.qa import *
import os

db = db_factory()

act = python_act('db')

test_expected_stdout = """
    Records affected: 1
    Records affected: 1
    Records affected: 1
"""

test_expected_stderr = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    
    sql_init = (act.files_dir / 'gtcs-ref-integ-init.sql').read_text()
    sql_addi = '''
        alter table employee drop constraint ref_key;
        alter table department drop constraint dept_key;
        set count on;
        -- All folowing statements should PASS:
        update department d set dept_no = -dept_no where exists(select * from employee e where e.dept_no = d.dept_no) rows 1;
        insert into employee( emp_no, last_name, dept_no) values (12, 'e12', -(select max(dept_no)+1 from department) );
        delete from department d where exists(select * from employee e where e.dept_no = d.dept_no) rows 1;
    '''

    act.expected_stdout = test_expected_stdout
    act.expected_stderr = test_expected_stderr
   
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ) )

    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)
