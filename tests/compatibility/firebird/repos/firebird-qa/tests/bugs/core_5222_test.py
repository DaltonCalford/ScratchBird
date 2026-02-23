#coding:utf-8

"""
ID:          issue-5502
ISSUE:       5502
TITLE:       SELECT WITH LOCK may raise unexpected update conflict errors under concurrent load
DESCRIPTION:
  Prototype: https://groups.yahoo.com/neo/groups/firebird-support/conversations/messages/128920
  Done with suggestions from dimitr, see letter 01-may-2016 09:15.
  Confirmed on WI-V3.0.0.32366, WI-V3.0.0.32483, 3.0.0.32501 (SS,SC,CS) - it was enough to
  async. start THREE child ISQLs sessions, one or two of them always raise exception after
  few seconds with text: 'deadlock / update conflicts' and could not finish its job.
NOTES:
  [06.06.2022] pzotov
  It was decided to set ReadConsistency = 0 in firebird.conf of FB 4.x+
  There is ability to start Tx in ReadConsistency when this parameter has such (NON-default) value
  by explicitly specifying such isolation level:
  ======
  If ReadConsistency is set to 0 - flags [NO] RECORD VERSION takes effect as in previous Firebird 
  versions. READ COMMITTED READ CONSISTENCY isolation level should be specified explicitly by 
  application - in TPB or using SQL syntax.
  ======
  See letter from pcisar 11-feb-2022 17:53, subj: "pytest + firebird-qa plugin with storing outcomes..."
  Checked on 4.0.1.2692, 3.0.8.33535 - both on Linux and Windows.

JIRA:        CORE-5222
FBTEST:      bugs.core_5222
"""

from __future__ import annotations
from typing import List
import pytest
import subprocess
import time
import re
from pathlib import Path
from firebird.qa import *

init_script = """
    create or alter procedure p_increment as begin end;
    commit;
    recreate table gen_tab(
        id bigint primary key
    );
    recreate table gen_log (
        id bigint primary key,
        id_new bigint not null,
        id_diff bigint not null
    );

    set term ^;
    alter procedure p_increment (a_iter_cnt int default 1000)
    returns (
        proc_start timestamp,
        selected_id bigint,
        proc_finish timestamp
    ) as
        declare i bigint;
        declare id_new bigint;
    begin
        i = 1;
        proc_start = 'now';
        while (i <= a_iter_cnt ) do
        begin
            in autonomous transaction do
            begin

                select id
                from gen_tab with lock -- this raised SQLSTATE = 40001 / -update conflicts with concurrent update
                into selected_id;
                i = i + 1;

                id_new = selected_id + 1;
                insert into gen_log(id, id_new, id_diff)
                            values( :selected_id, :id_new, :id_new - :selected_id);

                update gen_tab set id = :id_new
                where id = :selected_id;

            end
        end
        proc_finish = 'now';

        suspend; -- outputs row with three fields: 'PROC_START <timestamp>', 'SELECTED_ID <id>', 'PROC_FINISH <timestamp>'
    end ^
    set term ;^
    commit;

    insert into gen_tab (id) values (0);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

test_script = """
    commit;
    set list on;
    set transaction read committed no record_version lock timeout 3;
    select current_timestamp as before_proc
    from rdb$database;

    select * from p_increment(10);

    select current_timestamp as after_proc
    from rdb$database;
"""

expected_stdout = """
    EXPECTED, LOG #0: BEFORE_PROC
    EXPECTED, LOG #0: PROC_START
    EXPECTED, LOG #0: SELECTED_ID
    EXPECTED, LOG #0: PROC_FINISH
    EXPECTED, LOG #0: AFTER_PROC

    EXPECTED, LOG #1: BEFORE_PROC
    EXPECTED, LOG #1: PROC_START
    EXPECTED, LOG #1: SELECTED_ID
    EXPECTED, LOG #1: PROC_FINISH
    EXPECTED, LOG #1: AFTER_PROC

    EXPECTED, LOG #2: BEFORE_PROC
    EXPECTED, LOG #2: PROC_START
    EXPECTED, LOG #2: SELECTED_ID
    EXPECTED, LOG #2: PROC_FINISH
    EXPECTED, LOG #2: AFTER_PROC
"""

PLANNED_DML_ATTACHMENTS = 3

run_sql = temp_file('core_5222_run.sql')
dml_logs = temp_files([f'tmp_dml_5222_{i+1}' for i in range(PLANNED_DML_ATTACHMENTS)])

@pytest.mark.version('>=3.0')
def test_1(act: Action, run_sql: Path, dml_logs: List[Path], capsys):
    pattern = re.compile("BEFORE_PROC*|PROC_START*|SELECTED_ID*|PROC_FINISH*|AFTER_PROC*")
    run_sql.write_text(test_script)
    # Launching several concurrent child ISQL processes which perform `run_sql` script
    f_logs = []
    p_dml = []
    try:
        for dml_log in dml_logs: # Contains PLANNED_DML_ATTACHMENTS items
            f = open(dml_log, mode='w')
            f_logs.append(f)
            p_dml.append(subprocess.Popen([act.vars['isql'],
                                           '-i', str(run_sql),
                                           '-user', act.db.user,
                                           '-password', act.db.password,
                                           act.db.dsn],
                                          stdout=f, stderr=subprocess.STDOUT))
        #
        time.sleep(PLANNED_DML_ATTACHMENTS * 5)
    finally:
        for f in f_logs:
            f.close()
        for p in p_dml:
            p.terminate()
    #
    # 1. Each log _should_ contain ONLY following lines:
    #    BEFORE_PROC                     2016-05-03 09:27:57.6210
    #    PROC_START                      2016-05-03 09:27:57.6210
    #    SELECTED_ID                     1569
    #    PROC_FINISH                     2016-05-03 09:28:04.0740
    #    AFTER_PROC                      2016-05-03 09:28:04.0740
    # 2. _NO_ log should contain 'SQLSTATE = 40001'
    #
    # Open every log and print 1st word from each line, ignoring values of timestamp and ID.
    i = 0
    for dml_log in dml_logs:
        for line in dml_log.read_text().splitlines():
            if line.split():
                if pattern.match(line):
                    print(f'EXPECTED, LOG #{i}: {line.split()[0]}')
                else:
                    print(f'UNEXPECTED, LOG #{i}: {line}')
        i += 1
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
