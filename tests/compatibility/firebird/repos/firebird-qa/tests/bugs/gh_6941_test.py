#coding:utf-8

"""
ID:          issue-6941
ISSUE:       6941
TITLE:       Dummy (always true) conditions may change the join order
DESCRIPTION:
    Issue from ticket can NOT be reproduced if we restore only metadata: plans do not differ.
    Because of this, it was decided to use database that was provided by ticket author.
    We decompress it, make full restore (metadata + data) and apply test query TWISE:
    run-1: without any 'always true' part statement;
    run-2: _with_ 'always true' part statement (see loop with using 'x_true' variable).
    For each query we make only its preparing and add execution PLAN to the Python set()
    object (see 'plan_set').
    Finally, we check that length of this set is 1.
NOTES:
    [20.02.2023] pzotov
    Confirmed bug on 5.0.0.733; 4.0.3.2844; 3.0.11.33628 (length of plan_set is 2).
    Checked on: 5.0.0.957; 4.0.3.2876; 3.0.11.33664 -- all fine.
"""
import pytest
from firebird.qa import *
import zipfile
from pathlib import Path

db = db_factory()
act = python_act('db')

tmp_fbk = temp_file('gh_6941.tmp.fbk')
db_tmp = db_factory(filename='tmp_gh_6941.fdb', do_not_create=True)

chk_sql = """
    select t1.*
    from table1 t1
    join table2 t2 on t2.fk_t1 = t1.id
    join table3 t3 on t3.id = t1.fk_t3
    where
        %s
        t3.field = 6
        and exists (select 1 from table2 x where x.fk2_t2 = t2.id)
"""

@pytest.mark.version('>=3.0.11')
def test_1(act: Action, tmp_fbk: Path, db_tmp: Database):

    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_6941.zip', at = 'gh_6941.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())

    with act.connect_server() as srv:
        srv.database.restore(backup = tmp_fbk, database = db_tmp.db_path)
        srv.wait()

    plan_set = set()
    # test queries:
    with db_tmp.connect() as con:
        c1 = con.cursor()
        for x_true in ('', '0 = 0 and'):
            c1.execute(chk_sql % x_true) # <<< HERE WE SUBSTITUTE EITHER '' OR '0 = 0 and'
            plan_set.add(c1.statement.plan)
   
    assert len(plan_set) == 1

