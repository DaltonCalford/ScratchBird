#coding:utf-8

"""
ID:          issue-3072
ISSUE:       3072
TITLE:       Write note into log when automatic sweep is started
DESCRIPTION:
JIRA:        CORE-2668
FBTEST:      bugs.core_2668
"""

import pytest
import time
import subprocess
import re
from pathlib import Path
from difflib import unified_diff
from firebird.qa import *
from firebird.driver import DbWriteMode, ShutdownMode, ShutdownMethod

db = db_factory()

act = python_act('db')

test_script = """
recreate table test(s varchar(36) unique);
insert into test(s) values('LOCKED_FOR_PAUSE');
commit;

set transaction read committed WAIT;

update test set s = s where s = 'LOCKED_FOR_PAUSE';

set term ^;
execute block as
    declare n int = 150;
    declare v_role varchar(31);
begin
    while (n > 0) do
        in autonomous transaction do
        insert into test(s) values( rpad('', 36, uuid_to_char(gen_uuid()) ) )
        returning :n-1 into n;

    v_role = left(replace( uuid_to_char(gen_uuid()), '-', ''), 31);

    begin
        execute statement ('update test set s = s where s = ?') ('LOCKED_FOR_PAUSE')
        on external
            'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
            as user 'SYSDBA' password 'masterkey' role v_role
        with autonomous transaction;
    when any do
        begin
        end
    end

end
^
set term ;^
set heading off;
select '-- shutdown me now --' from rdb$database;
"""

tmp_script = temp_file('work_script.sql')

@pytest.mark.es_eds
@pytest.mark.version('>=3')
def test_1(act: Action, tmp_script: Path):
    tmp_script.write_text(test_script)
    with act.connect_server() as srv:
        srv.database.set_sweep_interval(database=act.db.db_path, interval=100)
        srv.database.set_write_mode(database=act.db.db_path, mode=DbWriteMode.ASYNC)
        p_work_sql = subprocess.Popen([act.vars['isql'], '-i', str(tmp_script),
                                       '-user', act.db.user,
                                       '-password', act.db.password, act.db.dsn],
                                      stderr = subprocess.STDOUT)
        time.sleep(3)
        try:
            srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.FULL,
                                  method=ShutdownMethod.FORCED, timeout=0)
        finally:
            p_work_sql.terminate()
        srv.database.bring_online(database=act.db.db_path)
        srv.info.get_log()
        fblog_before = srv.readlines()
        with act.db.connect() as con_for_sweep_start:
            con_for_sweep_start.begin()
            time.sleep(2)
            srv.info.get_log()
        fblog_after = srv.readlines()
        pattern = re.compile('Sweep\\s+.*SWEEPER', re.IGNORECASE)
        success = False
        for line in unified_diff(fblog_before, fblog_after):
            if line.startswith('+') and pattern.search(line):
                success = True
        assert success
