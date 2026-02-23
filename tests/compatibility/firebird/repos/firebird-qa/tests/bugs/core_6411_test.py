#coding:utf-8

"""
ID:          issue-6649
ISSUE:       6649
TITLE:       FB crashes on attempt to create table with number of fields greater than 5460
DESCRIPTION:
    It was found that maximal number of fields with type = BIGINT that could fit in a table DDL is 8066.
    If this limit is exeeded then FB raises "new record size of N bytes is too big" (where N >= 65536).
    We use for-loop with two iterations, each of them does following:
      1. Create table with total number of fields = <N> (one for 'ID primary key' plus 8064 for
         'user-data' fields with names 'F1', 'F2', ..., 'F'<N>-1). All of them have type = BIGINT.
      2. DO RECONNECT // mandatory! otherwise crash can not be reproduced.
      3. Run UPDATE OR INSERT statement that is specified in the ticker(insert single record with ID=1).
      4. Run SELECT statement which calculates total sum on all 'user-data' fields.
    When N = 8065 then these actions must complete successfully and result of final SELECT must be displayed.
    When N = 8066 then we have to get exception:
      Statement failed, SQLSTATE = 54000
      unsuccessful metadata update
      -new record size of 65536 bytes is too big

    Confirmed bug on 4.0.0.2204: got crash when N=8065 (but still "new record size of 65536 bytes is too big" when N=8066).
    Checked on 3.0.7.33368, 4.0.0.2214 - all OK.
JIRA:        CORE-6411
FBTEST:      bugs.core_6411
NOTES:
    [24.03.2022] pzotov
        On WINDOWS excessive lines in STDERR (after first error) appear because ISQL ignores 'SET BAIL' command or '-bail' switch
        in case when it executing commands that come via PIPE mechanism.
        Discussed with FB-team, letter with subj "[new-qa] ISQL "SET BAIL ON" problem on Windows when commands come from PIPE", 12-mar-2022 16:04.
        It looks strange but such behaviour was considered as DESIRED feature and requested long ago (in 2011).
        This means that it is very unlikely that it will be fixed and ISQL will behave like on POSIX (where no such effect occurs).
        Because of this, it was decided to change test: we create temporary SQL file and provide this file as input script for launching ISQL,
        see:
            act.isql(..., input_file=isql_script, ...)
    [03.07.2025] pzotov
    ::: NB ::: test duration time is ~6 minutes.
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.884; 5.0.3.1668; 4.0.6.3214.

    [06.10.2025] pzotov
    Disabled execution on 6.x: since #7332 this test not actual because rowsize can achieve 1Mb (i.e. 16x larger than in previous versions).
    Besides, execution time will be unacceptable.
"""

import pytest
import platform
import subprocess
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('.*(-)?After line \\d+.*', ''), ('[ \t]+', ' ')])

isql_script = temp_file('test-script-6411.sql')

@pytest.mark.version('>=3.0.7,<6')
def test_1(act: Action, isql_script: Path, capsys):
    
    FLD_COUNT_THRESHOLD = 8064 if act.is_version('<6') else 50000 # ?

    for step in range(0,2):
        fields_count = FLD_COUNT_THRESHOLD + step
        ddl_init = 'recreate table tdata (id bigint primary key'
        ddl_addi = '\n'.join([f',f{i} bigint' for i in range(1,fields_count)])
        ddl_expr = ''.join([ddl_init, ddl_addi, ')'])
        upd_init = 'update or insert into tdata values(1'
        upd_addi = '\n'.join( [f',{i}' for i in range(1,fields_count)])
        upd_expr = ''.join([upd_init, upd_addi, ') matching(id)'])
        sel_init = 'select '
        sel_addi = '+'.join([str(i) for i in range(0,fields_count)])
        sel_expr = ''.join([sel_init, sel_addi, ' as fields_total from tdata'])
        sql_expr=  f"""
            set bail on ;
            {ddl_expr} ;
            commit;
            connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}' ;
            {upd_expr} ;
            set list on ;
            {sel_expr} ;
            quit ;
        """
        isql_script.write_text(sql_expr)

        act.reset()
        act.isql(switches=[], input_file=isql_script, combine_output=True)

        for line in act.clean_stdout.splitlines():
            if line.strip():
                print(f'step: {step}, fields_count: {fields_count}, result: {line}')

    act.reset()

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    TABLE_NAME = 'TDATA' if act.is_version('<6') else  '"TDATA"'
    expected_stdout = f"""
        step: 0, fields_count: {FLD_COUNT_THRESHOLD}, result: FIELDS_TOTAL 32510016
        step: 1, fields_count: {FLD_COUNT_THRESHOLD+1}, result: Statement failed, SQLSTATE = 54000
        step: 1, fields_count: {FLD_COUNT_THRESHOLD+1}, result: unsuccessful metadata update
        step: 1, fields_count: {FLD_COUNT_THRESHOLD+1}, result: -new record size of 65536 bytes is too big
        step: 1, fields_count: {FLD_COUNT_THRESHOLD+1}, result: -TABLE {SQL_SCHEMA_PREFIX}{TABLE_NAME}
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out

    assert act.clean_stdout == act.clean_expected_stdout
