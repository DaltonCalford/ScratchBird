#coding:utf-8

"""
ID:          issue-5015
ISSUE:       5015
TITLE:       Implement ability to validate tables and indices online (without exclusive access to database)
DESCRIPTION:
JIRA:        CORE-4707
FBTEST:      bugs.core_4707
NOTES:
    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.

    [02.11.2025] pzotov
    Commented out 'request.addfinalizer(drop_connections)' because QA plugin already has similar functionality
    for teardown phase - see class Database, func drop().
    Checked on Windows 6.0.0.1335; 5.0.4.1725; 4.0.7.3237; 3.0.14.33827 (SS and CS)
"""

import pytest
import subprocess
import time
from pathlib import Path
from firebird.qa import *

init_script = """
    set term ^;
    execute block as
    begin
        execute statement 'drop sequence g';
    when any do begin end
    end^
    set term ;^
    commit;
    create sequence g;
    commit;
    recreate table test1(id int, s varchar(1000));
    recreate table test2(id int primary key using index test2_pk, s varchar(1000), t computed by (s) );
    recreate table test3(id int);
    commit;

    insert into test1(id, s) select gen_id(g,1), rpad('', 1000, gen_id(g,0) ) from rdb$types rows 100;
    insert into test2(id, s) select id, s from test1;
    commit;

    create index test2_s on test2(s);
    create index test2_c on test2 computed by(s);
    create index test2_t on test2 computed by(t);
    commit;
"""

db = db_factory(init=init_script)

substitutions=[ ('\\d{2}:\\d{2}:\\d{2}.\\d{2}', ''), ('Relation \\d{3,4}', 'Relation') ]
act = python_act('db', substitutions = substitutions)

hang_script_file = temp_file('hang_script.sql')
hang_output = temp_file('hang_script.out')

@pytest.mark.es_eds
@pytest.mark.version('>=3.0')
def test_1(act: Action, hang_script_file: Path, hang_output: Path, capsys, request):
    
    ## Fializer for FB4
    #def drop_connections():
    #    with act.db.connect() as con4cleanup:
    #        con4cleanup.execute_immediate('delete from mon$attachments where mon$attachment_id != current_connection')
    #        con4cleanup.commit()
    #
    #request.addfinalizer(drop_connections)

    # Following script will hang for sevral seconds (see 'lock timeout' argument - and this will serve as pause
    # during which we can launch fbsvcmgr to validate database:
    hang_script_file.write_text(
        f"""
            set term ^;
            execute block as
            begin
                execute statement 'drop role tmp$r4707';
                when any do begin end
            end ^
            set term ;^
            commit;

            set transaction wait;

            delete from test1;
            insert into test3(id) values(1);
            set list on;
            select 'Starting EB with infinite pause.' as isql_msg from rdb$database;
            set term ^;
            execute block as
            begin
                execute statement 'update test1 set id=-id'
                on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
                as user '{act.db.user}' password '{act.db.password}'
                   role 'TMP$R4707' -- this will force to create new attachment, and its Tx will be paused on INFINITE time.
                ;
                -- Discussed with Vlad, letters 29.03.2025 ... 09.04.2025
                -- subj: "WI-6.0.0.707-4bd4f5f0, Classic. CORE-4707 (online validation): ..."
                -- COMMENTED 30.03.2025: we must know if some error occurred during infinite wait! --> when any do begin end
                -- ::: NB ::: ON LINUX THIS TEST CAN STILL FAIL, THE REASON REMAINS UNKNOWN :::
            end ^
            set term ;^
            select 'EB with pause finished.' as msg_2 from rdb$database;
        """
    )

    # Make asynchronous call of ISQL which will stay several seconds in pause due to row-level lock
    with open(hang_output, mode='w') as hang_out:
        p_hang_sql = subprocess.Popen([act.vars['isql'], '-i', str(hang_script_file),
                                       '-user', act.db.user,
                                       '-password', act.db.password, act.db.dsn],
                                       stdout=hang_out, stderr=subprocess.STDOUT)
        try:
            time.sleep(2)
            # Make SYNC. call of fbsvcmgr in order to validate database which has locks on some relations
            act.svcmgr(switches=['action_validate', 'dbname', str(act.db.db_path),
                                   'val_lock_timeout', '1'])
        finally:
            p_hang_sql.terminate()
    #
    print(hang_output.read_text())
    print(act.stdout)


    expected_stdout_5x = """
        ISQL_MSG                        Starting EB with infinite pause.
        Validation started
        Relation (TEST1)
        Acquire relation lock failed
        Relation (TEST1) : 1 ERRORS found
        Relation (TEST2)
        process pointer page    0 of    1
        Index 1 (TEST2_PK)
        Index 2 (TEST2_S)
        Index 3 (TEST2_C)
        Index 4 (TEST2_T)
        Relation (TEST2) is ok
        Relation (TEST3)
        Acquire relation lock failed
        Relation (TEST3) : 1 ERRORS found
        Validation finished
    """

    expected_stdout_6x = """
        ISQL_MSG                        Starting EB with infinite pause.
        Validation started
        Relation ("PUBLIC"."TEST1")
        Acquire relation lock failed
        Relation ("PUBLIC"."TEST1") : 1 ERRORS found
        Relation ("PUBLIC"."TEST2")
        process pointer page    0 of    1
        Index 1 ("PUBLIC"."TEST2_PK")
        Index 2 ("PUBLIC"."TEST2_S")
        Index 3 ("PUBLIC"."TEST2_C")
        Index 4 ("PUBLIC"."TEST2_T")
        Relation ("PUBLIC"."TEST2") is ok
        Relation ("PUBLIC"."TEST3")
        Acquire relation lock failed
        Relation ("PUBLIC"."TEST3") : 1 ERRORS found
        Validation finished
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
