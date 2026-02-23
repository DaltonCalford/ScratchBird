#coding:utf-8

"""
ID:          issue-8427
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8427
TITLE:       Allow specifying exactly what type of database string is expected in the database section for trace configuration
DESCRIPTION:
    We create five temporary files which will be overwritten by test DB.
    Two of them ('tmp_fdb1a' and 'tmp_fdb1b') have curvy braces in name in order to check that parsing properly handles such names.
    Another two files ('tmp_fdb2a' and 'tmp_fdb2b') have digital suffix containing thee digits in order to check that they will be
    properly taken in account by '[[:DIGIT:]]{3,}' that will be used in 'databaseRegex' section.
    And 5th file ('tmp_fdb2c') has name that must NOT occur in the trace log.

    Trace config will have following structure:
    ============
        database
        {
            enabled = false
        }

        # FIRST occurence of 'databaseName':
        databaseName = <tmp_fdb1a>
        {
            enabled = true
            log_initfini = false
            log_connections = true
        }

        # SECOND occurence of 'databaseName':
        databaseName = <tmp_fdb1b>
        {
            enabled = true
            log_initfini = false
            log_connections = true
        }

        databaseRegex = <pattern_for tmp_fdb2a and tmp_fdb2b>
        {
            enabled = true
            log_initfini = false
            log_connections = true
        }
    ============

    Then we make .sql script that only has 'CONNECT' command to each of these databases.
    If we launch trace and execute this .sql then:
        * events 'ATTACH_DATABASE' and 'DETACH_DATABASE' must be met one time in the trace log for every databases EXCEPT <tmp_fdb2c>;
        * for <tmp_fdb2c> no events must be in the trace log.
    We can consider test as passed if and only if above mentioned conditions are met.
NOTES:
    [03.04.2025] pzotov
    Checked on 6.0.0.715-08cb3f9 SS/CS (Windows).
"""

import locale
import re
from pathlib import Path
import shutil

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

tmp_fdb1a = temp_file('tmp_8427_{1}.fdb')
tmp_fdb1b = temp_file('tmp_8427_{2}.fdb')
tmp_fdb2a = temp_file('tmp_8427_301.fdb')
tmp_fdb2b = temp_file('tmp_8427_302.fdb')
tmp_fdb2c = temp_file('tmp_8427_qwe.fdb') # <<< this must NOT be traced, see blow
tmp_conf = temp_file('tmp_8427_trace.conf')
tmp_sql = temp_file('tmp_8427_check.sql')

@pytest.mark.trace
@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_fdb1a: Path, tmp_fdb1b: Path, tmp_fdb2a: Path, tmp_fdb2b: Path, tmp_fdb2c: Path, tmp_conf: Path, tmp_sql: Path, capsys):

    # NOTE. We have to replace each '{' and '}' with duplicated one:
    db_name1 = str(tmp_fdb1a).replace('{', '{{').replace('}', '}}')
    db_name2 = str(tmp_fdb1b).replace('{', '{{').replace('}', '}}')
    
    db_patt = '(%[\\\\/](tmp_8427_[[:DIGIT:]]{{3,}}).fdb)'

    trace_conf = f"""
        database
        {{
            enabled = false
        }}
        
        databaseName = {db_name1}
        {{
            enabled = true
            log_initfini = false
            log_connections = true
        }}

        databaseName = {db_name2}
        {{
            enabled = true
            log_initfini = false
            log_connections = true
        }}

        databaseRegex = {db_patt}
        {{
            enabled = true
            log_initfini = false
            log_connections = true
        }}
    """

    # for debug only:
    # tmp_conf.write_text(trace_conf)

    chk_sql_lines = []
    for db_i in (tmp_fdb1a, tmp_fdb1b, tmp_fdb2a, tmp_fdb2b, tmp_fdb2c):
        shutil.copy2(act.db.db_path, db_i)
        chk_sql_lines.append(f"connect 'inet://{db_i}' user {act.db.user} password {act.db.password};")
        chk_sql_lines.append('commit;')
    
    tmp_sql.write_text('\n'.join(chk_sql_lines))

    with act.trace(config = [x for x in trace_conf.splitlines() if x.strip()], encoding='utf8', encoding_errors='utf8'):
        act.isql(switches = ['-q'], input_file = tmp_sql, connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())
        isql_retcode = act.return_code
        isql_output = act.clean_stdout

    assert isql_retcode == 0 and isql_output == '', 'Script to check trace output FAILED, check its output.'
    act.reset()

    traced_db_count = {} # K = db_name; V = (attach_count, detach_count)
    trace_out = []
    for line in act.trace_log:
        if (x := line.rstrip()):
            trace_out.append(x)

    attach_detach_ptn = re.compile( r'\)\s+(ATTACH_|DETACH_)DATABASE' )
    attach_id_pattern = re.compile( r'\s+\(ATT_\d+,\s+' + act.db.user.upper() )

    for i,line in enumerate(trace_out):
        if attach_detach_ptn.search(line):
            next_line = trace_out[i+1]
            if (p := attach_id_pattern.search(next_line)):
                handled_db = Path(next_line[ : p.span()[0] ].strip().lower())
                attach_cnt, detach_cnt = traced_db_count.get(handled_db, [0,0])
                if 'ATTACH_DATABASE' in line:
                    attach_cnt += 1
                elif 'DETACH_DATABASE' in line:
                    detach_cnt += 1
                traced_db_count[handled_db] = (attach_cnt, detach_cnt)
    
    # Expected content of traced_db_count:
    # <path/to/temp>/tmp_8427_{1}.fdb (1, 1)
    # <path/to/temp>/tmp_8427_{2}.fdb (1, 1)
    # <path/to/temp>/tmp_8427_301.fdb (1, 1)
    # <path/to/temp>/tmp_8427_302.fdb (1, 1)

    EXPECTED_MSG = 'Expected: found *exact* set of ATTACH and DETACH events.'

    if list(traced_db_count.keys()) == [tmp_fdb1a, tmp_fdb1b, tmp_fdb2a, tmp_fdb2b] and set(traced_db_count.values()) == set([(1,1)]):
        print(EXPECTED_MSG)
    else:
        print('Trace either has no events for some DB or excessive databases / events present:')
        for k,v in traced_db_count.items():
            print(str(k), f'attach_count: {v[0]}, detach_count: {v[1]}')

    act.expected_stdout = f"""
        {EXPECTED_MSG}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
