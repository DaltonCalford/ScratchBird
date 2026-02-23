#coding:utf-8

"""
ID:          issue-5151
ISSUE:       5151
TITLE:       Online validation during DML activity in other connection leads to message "Error while trying to read from file" and "page in use during flush (210), file: cch.cpp line: 2672"
DESCRIPTION:
JIRA:        CORE-4855
FBTEST:      bugs.core_4855
NOTES:
    [30.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
import subprocess
import time
from pathlib import Path
from firebird.qa import *

substitutions = [('[0-9][0-9]:[0-9][0-9]:[0-9][0-9].[0-9][0-9]', ''),
                 ('Relation [0-9]{3,4}', 'Relation'),
                 ('Statement failed, SQLSTATE = HY008', ''),
                 ('operation was cancelled', ''), ('After line .*', '')]

db = db_factory()

act = python_act('db', substitutions=substitutions)

heavy_script = """
    recreate sequence g;
    recreate table test(id int, s varchar( 36 ) unique using index test_s_unq);
    recreate table stop(id int);
    commit;
    set list on;
    set transaction read committed;
    set term ^;
    execute block returns( inserted_rows varchar(20) ) as
    begin
      while ( not exists(select * from stop) ) do
      begin
        insert into test(id, s) values( gen_id(g,1), rpad('', 36, uuid_to_char(gen_uuid())) );
      end
      inserted_rows = iif(gen_id(g,0) > 0, 'OK, LOT OF.', 'FAIL: ZERO!');
      suspend;
    end
    ^
    set term ;^
    commit;
"""

heavy_script_file = temp_file('heavy_script.sql')
heavy_output = temp_file('heavy_script.out')

@pytest.mark.version('>=3.0')
def test_1(act: Action, heavy_script_file: Path, heavy_output: Path, capsys):
    # Preparing script for ISQL that will do 'heavy DML'
    heavy_script_file.write_text(heavy_script)
    with open(heavy_output, mode='w') as heavy_out:
        #############################################
        ###   a s y n c    l a u n c h    i s q l ###
        #############################################
        p_heavy_sql = subprocess.Popen([act.vars['isql'], '-i', str(heavy_script_file),
                                       '-user', act.db.user,
                                       '-password', act.db.password, act.db.dsn],
                                       stdout=heavy_out, stderr=subprocess.STDOUT)
        try:
            time.sleep(4) # todo: reimplement this via query to mon$ tables every 0.1 second
            # Run validation twice
            with act.connect_server() as srv:
                print('Iteration #1:')
                srv.database.validate(database=act.db.db_path, lock_timeout=1,
                                      callback=print)
                print('Iteration #2:')
                srv.database.validate(database=act.db.db_path, lock_timeout=1,
                                      callback=print)
            # Stopping ISQL that is doing now 'heavy DML' (bulk-inserts):
            act.isql(switches=[], input='insert into stop(id) values(1); commit;')
        finally:
            p_heavy_sql.terminate()
    #
    print(heavy_output.read_text())
    # Check
    act.reset()

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    TABLE_TEST_NAME = 'TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'
    TABLE_STOP_NAME = 'STOP' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"STOP"'

    expected_stdout = f"""
        Iteration #1:
        21:16:28.31 Validation started
        21:16:28.31 Relation 128 ({TABLE_TEST_NAME})
        21:16:29.31 Acquire relation lock failed
        21:16:29.31 Relation 128 ({TABLE_TEST_NAME}) : 1 ERRORS found
        21:16:30.04 Relation 129 ({TABLE_STOP_NAME})
        21:16:30.04   process pointer page    0 of    1
        21:16:30.04 Relation 129 ({TABLE_STOP_NAME}) is ok
        21:16:30.04 Validation finished
        Iteration #2:
        21:16:32.46 Validation started
        21:16:32.46 Relation 128 ({TABLE_TEST_NAME})
        21:16:33.46 Acquire relation lock failed
        21:16:33.46 Relation 128 ({TABLE_TEST_NAME}) : 1 ERRORS found
        21:16:35.09 Relation 129 ({TABLE_STOP_NAME})
        21:16:35.09   process pointer page    0 of    1
        21:16:35.09 Relation 129 ({TABLE_STOP_NAME}) is ok
        21:16:35.09 Validation finished
        INSERTED_ROWS                   OK, LOT OF.
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

