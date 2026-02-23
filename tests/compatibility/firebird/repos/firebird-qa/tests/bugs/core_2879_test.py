#coding:utf-8

"""
ID:          issue-3263
ISSUE:       3263
TITLE:       Sweep could raise error : page 0 is of wrong type (expected 6, found 1)
DESCRIPTION:
  Test receives content of firebird.log _before_ and _after_ running query that is show in the ticket.
  Then we compare these two files.
  Difference between them should relate ONLY to sweep start and finish details, and NOT about page wrong type.
JIRA:        CORE-2879
FBTEST:      bugs.core_2879
"""

import pytest
import time
import subprocess
from pathlib import Path
from difflib import unified_diff
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('^((?!start|finish|expected|page|wrong).)*$', '')])

test_script = """
    set list on;
    set term ^;
    execute block returns (dts timestamp, sql varchar(80)) as
        declare i int;
        declare s varchar(256);
    begin
        i = 1;
        while (i < 32767) do
        begin
            s = 'tmp' || :i;
            dts = 'now';
            sql = 'create global temporary table ' || :s || ' (id int);';
            execute statement sql with autonomous transaction;
            suspend;

            dts = 'now';
            sql = 'drop table ' || :s || ';';
            execute statement sql with autonomous transaction;
            suspend;

            i = i + 1;
        end
    end ^
    set term ;^
"""

expected_stdout = """
    Sweep is started by SYSDBA
    Sweep is finished
"""

isql_script = temp_file('test-script.sql')
isql_output = temp_file('test-script.out')

@pytest.mark.version('>=3')
def test_1(act: Action, isql_script: Path, isql_output: Path, capsys):
    isql_script.write_text(test_script)
    with act.connect_server() as srv:
        # Get content of firebird.log BEFORE test
        log_before = act.get_firebird_log()
        with open(isql_output, mode='w') as isql_out:
            p_isql = subprocess.Popen([act.vars['isql'], '-u', act.db.user, '-pas',
                                       act.db.password, act.db.dsn, '-i', str(isql_script)],
                                      stdout=isql_out, stderr=subprocess.STDOUT)
        time.sleep(2)
        # LAUNCH SWEEP while ISQL is working
        srv.database.sweep(database=act.db.db_path)
        p_isql.terminate()
        # Get content of firebird.log AFTER test
        log_after = act.get_firebird_log()
        for line in unified_diff(log_before, log_after):
            if line.startswith('+') and line.split('+'):
                print(line.replace('+', ' '))
        act.expected_stdout = expected_stdout
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout


