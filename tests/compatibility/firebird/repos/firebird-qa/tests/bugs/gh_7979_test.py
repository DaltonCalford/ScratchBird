#coding:utf-8

"""
ID:          issue-7979
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7979
TITLE:       Hang when database with disconnect trigger using MON$ tables is shutting down
DESCRIPTION:
NOTES:
    [02.02.2024] pzotov
    ### ACHTUNG ###
    Bug could NOT be reproduced on Windows.
    Bug can be reproduced only when appropriate code runs OUTSIDE current firebird-QA framework, i.e. directly from OS.
    Because of that, this test creates temporary .py script which is launched further using subprocess.run( [ sys.executable ...] )
    Confirmed problem on 6.0.0.219: 'gfix -shut full -force 0' hangs.
    Checked on 6.0.0.244.

    [15.02.2024] pzotov
    Checked on 4.0.5.3059 (commit #a552f1f3), 5.0.1.1340 (commit #f7171b58).
    NOTE: 3.0.123.3731 (dob=15.02.2024) does NOT pass this test.

    [16.02.2024] pzotov
    Added [temporary] mark for SKIP this test when QA runs agains *fork* of standard FB because it hangs ('disabled_in_forks').
    This mark will be removed after separating QA runs (executing tests against standard FB snapshot on DEDICATED machine).

    NB-1. QA must use command like: 'pytest -m "not disabled_in_forks" ...' when check *fork* of standard FB.
    NB-2. Unfortunately, nowadays QA runs for standard FB and its fork are performed at the same host.
          Lagging problem exists with [back-]porting of some fixes/features into fork after implementation for the same FB-major version.
          This can cause the whole QA-job to be incompleted and missed report for one of even several days.
"""
import sys
import subprocess
from pathlib import Path
import pytest
from firebird.qa import *

init_sql = f"""
    create table logger (dts timestamp default 'now', att_cnt int);
    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION', 'INITIAL_DDL', 1);
    end
    ^
    create trigger logger active on disconnect as
        declare c int;
    begin
        if ( rdb$get_context('USER_SESSION', 'INITIAL_DDL') is null ) then
        begin
            select count(*) from mon$attachments where mon$attachment_id = current_connection into :c;
            insert into logger(att_cnt) values(:c);
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init = init_sql)
act = python_act('db', substitutions = [ ('^((?!(Attributes|ATT_CNT|Records affected)).)*$', ''), ('[ \t]+', ' ') ])
tmp_run_py = temp_file('tmp_7979_run_external.py')
tmp_log_py = temp_file('tmp_7979_run_external.log')
tmp_sql_py = temp_file('tmp_7979_check_result.sql')

@pytest.mark.disabled_in_forks
@pytest.mark.version('>=4.0.5')
def test_1(act: Action, tmp_run_py: Path, tmp_log_py: Path, tmp_sql_py: Path, capsys):
    if act.platform == 'Windows':
        pytest.skip('Could not reproduce bug on Windows')
    if act.get_server_architecture() != 'SuperServer':
        pytest.skip('Applies only to SuperServer')

    py_run_ext = ' '.join( [ sys.executable, '-u', f'{str(tmp_run_py)}'] )

    ##########################################################################################
    ###   G E N E R A T I O N     O F    T E M P O R A R Y    P Y T H O N    S C R I P T   ###
    ##########################################################################################
    py_source = f"""# -*- coding: utf-8 -*-
# {py_run_ext}
import os
import sys
import argparse as ap
from pathlib import Path
import subprocess
import datetime as py_dt

import firebird.driver
from firebird.driver import *

os.environ["ISC_USER"] = '{act.db.user}'
os.environ["ISC_PASSWORD"] = '{act.db.password}'
driver_config.fb_client_library.value = r"{act.vars['fbclient']}"

bin_isql = r"{act.vars['isql']}"
bin_gfix = r"{act.vars['gfix']}"
bin_gstat = r"{act.vars['gstat']}"
db_conn = r'{act.db.dsn}'
db_file = r'{act.db.db_path}'
tmp_sql = r'{str(tmp_sql_py)}'

with connect(db_conn) as con:
    #print(f"Trying to run gfix -shut full -force 0 db_conn")
    subprocess.run( [bin_gfix, '-shut', 'full', '-force', '0', db_conn] )
    subprocess.run( [bin_gstat, '-h', db_file] )
    #print(f"Trying to run gfix -online db_conn")
    subprocess.run( [bin_gfix, '-online', db_conn] )
    subprocess.run( [bin_gstat, '-h', db_file] )

chk_sql='''
    set list on;
    set count on;
    select att_cnt from logger;
'''
with open(tmp_sql, 'w') as f:
    f.write(chk_sql)

subprocess.run( [bin_isql, '-q', '-i', tmp_sql, db_conn] )
"""
    ########################################################
    ###   END OF GENERATION OF TEMPORARY PYTHON SCRIPT   ###
    ########################################################

    tmp_run_py.write_text(py_source)
    with open(tmp_log_py, 'w') as f:
        subprocess.run( [ sys.executable, '-u', f'{str(tmp_run_py)}'], stdout = f, stderr = subprocess.STDOUT )
    
    with open(tmp_log_py, 'r') as f:
        print(f.read())
    act.expected_stdout = """
	Attributes full shutdown
	Attributes
	ATT_CNT 1
	Records affected: 1
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
