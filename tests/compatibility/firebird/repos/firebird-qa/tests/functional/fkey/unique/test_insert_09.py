#coding:utf-8

"""
ID:          fkey.unique.insert-09
FBTEST:      functional.fkey.unique.insert_09
TITLE:       Check correct work fix with foreign key
DESCRIPTION:
  Check foreign key work.
  Master table has unique field.
  Master transaction inserts record into master_table without commit.
  Detail transaction inserts record in detail_table.
  Expected: referential integrity error.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t_detl(id int);
    commit;
    recreate table t_main(
         id int constraint t_main_pk primary key using index t_main_pk
        ,uniq_ref int constraint t_main_uk unique using index t_main_uk
    );
    commit;
    recreate table t_detl(
        id int constraint t_detl_pk primary key using index t_detl_pk,
        master_uniq_ref int constraint t_detl_fk_mur references t_main(uniq_ref) using index t_detl_fk_mur
    );
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    commit;
    set transaction read committed record_version no wait;
    set term ^;
    execute block as
    begin
        insert into t_main(id, uniq_ref) values(1, 1);
        in autonomous transaction do
        insert into t_detl(id, master_uniq_ref) values(100, 1);
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('-At block line: [\\d]+, col: [\\d]+',
                                                  '-At block line')])


@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    expected_stdout = f"""
        Statement failed, SQLSTATE = 23000
        violation of FOREIGN KEY constraint "T_DETL_FK_MUR" on table {SQL_SCHEMA_PREFIX}"T_DETL"
        -Foreign key reference target does not exist
        -Problematic key value is ("MASTER_UNIQ_REF" = 1)
        -At block line: 5, col: 9
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
