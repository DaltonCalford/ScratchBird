#coding:utf-8

"""
ID:          issue-3348
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/3348
TITLE:       Wrong results or unexpected errors while sorting a large data set
DESCRIPTION:
JIRA:        CORE-2966
FBTEST:      bugs.core_2966
NOTES:
    [07.02.2025] pzotov
    1. Test runs for ~55-60s (checked on Win-10, Intel(R) Core(TM) i3 CPU 540 3.07GHz, FIREBIRD_TMP pointed to RAM drive.
    2. Test assumes that FIREBIRD_TMP points to the resource with enough space!
       Otherwise it fails with one of following outcomes:
       case-1 (if there is no folder that is specified in FIREBIRD_TMP):
           INTERNALERROR> firebird.driver.types.DatabaseError: I/O error during "CreateFile (create)" operation for file ""
           INTERNALERROR> -Error while trying to create file
           INTERNALERROR> <the system cannot find the specified path> -- LOCALIZED message here
       case-2 (if folder specified by FIREBIRD_TMP *exists* but no free space encountered during sort):    
           Statement failed, SQLSTATE = HY000
           sort error
           -No free space found in temporary directories
           -operating system directive WriteFile failed
           -<not enough disk space> -- LOCALIZED message here
       Because of presence of localized messages, we have to use 'io_enc = locale.getpreferredencoding()' in act.execute().
       Also, 'combine_output = True' must be used in order to see both STDOUT and STDERR in the same log.
"""

import locale
import pytest
from firebird.qa import *

init_script = """
    create table t (col varchar(32000));
    commit;
    set term ^;
    execute block
    as
      declare variable i integer;
    begin
      i=0;
      while (i < 200000) do begin
        insert into t (col) values(mod(:i, 10));
        i= i+1;
      end
    end^
    set term ;^
    commit;
"""

db = db_factory(init = init_script)

test_script = """
    set list on;
    select col as col_as_txt from t group by 1;
    select cast(col as integer) as col_as_int from t group by 1;
"""

substitutions = [('[ \t]+', ' '), ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COL_AS_TXT 0
    COL_AS_TXT 1
    COL_AS_TXT 2
    COL_AS_TXT 3
    COL_AS_TXT 4
    COL_AS_TXT 5
    COL_AS_TXT 6
    COL_AS_TXT 7
    COL_AS_TXT 8
    COL_AS_TXT 9
    COL_AS_INT 0
    COL_AS_INT 1
    COL_AS_INT 2
    COL_AS_INT 3
    COL_AS_INT 4
    COL_AS_INT 5
    COL_AS_INT 6
    COL_AS_INT 7
    COL_AS_INT 8
    COL_AS_INT 9
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout

