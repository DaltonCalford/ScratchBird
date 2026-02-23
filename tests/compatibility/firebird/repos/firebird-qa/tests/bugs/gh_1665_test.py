#coding:utf-8

"""
ID:          issue-1665
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1665
TITLE:       TempDirectories parameter in firebird.conf ignored by global temporary tables
DESCRIPTION:
  Source discussions:
    1) https://github.com/FirebirdSQL/firebird/pull/311
       ("Introduce new setting TempTableDirectory as discussed in fb-devel, see also CORE-1241")
    2) https://github.com/FirebirdSQL/firebird/issues/1665
       ("TempDirectories parameter in firebird.conf ignored by global temporary tables [CORE1241]")
        Old: CORE-1241; discussion resumed 14.12.2020.
  Commits (20.04.2021 14:17; 17.05.2021 15:46):
    1) https://github.com/FirebirdSQL/firebird/commit/f2805020a6f34d253c93b8edac6068c1b35f9b89
       "New setting TempTableDirectory.
       Used to set directory where engine should put data of temporary tables and temporary blobs."
    2) https://github.com/FirebirdSQL/firebird/commit/fd0fa8a3a58fbfe7fdc0641b4e48258643d72127
       "Let include file name into error message when creation of temp file failed."

  Test verifies that:
      * we can use 'TempTableDirectory' as per-database parameter;
      * we will able to see in firebird.log messages about failure of fb_table* file creation

  Section in databases.conf related to test DB must contain definition of 'TempTableDirectory' with value
  that for sure will be *incorrect* both on Windows and Linux. It was decided to use following value:
  '|DEFINITELY|INACCESSIBLE|' (no such folder can exist in either OS).

  Then we establish connection to the test DB and run SQL which creates GTT and adds several rows in it.
  NO exception must be raised in this case: GTT must be fulfilled w/o problems and FB must create temporary
  file (fb_table_*) in some existing folder (defined by FIREBIRD_TMP variable; if it is undefined then such
  file will be created in C:\\TEMP or  /tmp - depending on OS).

  But firebird.log must contain message about problem with creating file (fb_table_*) in the directory which
  could not be accessed. We check old and new content of firebird.log with expecting to see message that
  did appear about this problem.

  Initially checked on 5.0.0.40, 4.0.0.2436.

JIRA:        CORE-1241
FBTEST:      bugs.gh_1665
NOTES:
    [17.08.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias defvined by variable REQUIRED_ALIAS.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Checked on 5.0.0.623, 4.0.1.2692 - both on Windows and Linux.
"""

import locale
import re
from difflib import unified_diff
import time

import pytest
from firebird.qa import *

REQUIRED_ALIAS = 'tmp_gh_1665_alias'
db = db_factory(filename = '#' + REQUIRED_ALIAS)

act = python_act('db', substitutions=[('[ \t]+', ' '), ('"\\|DEFINITELY\\|INACCESSIBLE\\|"', '')])

FLD_WIDTH=100
NUM_ROWS=100

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    fblog_1 = act.get_firebird_log()

    init_sql = f'''
        set bail on;
        connect '{REQUIRED_ALIAS}' user {act.db.user};
        set list on;
        select rdb$config_name, rdb$config_value from rdb$config g where upper(g.rdb$config_name) = upper('TempTableDirectory');
        recreate global temporary table gtt_test(s varchar({FLD_WIDTH}) unique) on commit preserve rows;
        commit;
        set count on;
        insert into gtt_test(s) select lpad('',{FLD_WIDTH},uuid_to_char(gen_uuid())) from rdb$types rows {NUM_ROWS};
        commit;
    '''

    ###############################################################################################################
    # POINT-1: check that ISQL does NOT raise any error (related to invalid path for TempTableDirectory):
    #
    act.expected_stdout = f"""
        RDB$CONFIG_NAME   TempTableDirectory
        RDB$CONFIG_VALUE  |DEFINITELY|INACCESSIBLE|
        Records affected: {NUM_ROWS}
    """
    
    act.isql(switches = ['-q'], input = init_sql, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #-------------------------------------------------------------------

    time.sleep(1) # Allow content of firebird log be fully flushed on disk.
    fblog_2 = act.get_firebird_log()


    ###############################################################################################################
    # POINT-2: check that diff between firebird.log initial and current content has phrases:
	# Error creating file in TempTableDirectory "|DEFINITELY|INACCESSIBLE|"
	# I/O error during "CreateFile (create)" operation for file "|DEFINITELY|INACCESSIBLE|\fb_table_mwyjb8"
	# Error while trying to create file
    #
    diff_patterns = [
        "\\+\\s+Error creating file in TempTableDirectory",
        "\\+\\s+Error while trying to create file"
    ]
    diff_patterns = [re.compile(s) for s in diff_patterns]
    
    for line in unified_diff(fblog_1, fblog_2):
        if line.startswith('+'):
            if act.match_any(line, diff_patterns):
                print(line)


    expected_stdout_log_diff = """
        + Error creating file in TempTableDirectory
        + Error while trying to create file
    """
    act.expected_stdout = expected_stdout_log_diff
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
