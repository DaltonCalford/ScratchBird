#coding:utf-8

"""
ID:          issue-2140
ISSUE:       2140
TITLE:       Incorrect "key size exceeds implementation restriction for index" error
DESCRIPTION:
JIRA:        CORE-1715
FBTEST:      bugs.core_1715
NOTES:
    [26.06.2025] pzotov
    Re-implemented: use max allowed values of key lengths for indices when 4 and 6 bytes are used per character.
    See:
        https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref50/firebird-50-language-reference.html#fblangref50-ddl-idx-limits
        ("Table 38. Maximum indexable (VAR)CHAR length")
    Bug existed on 6.x since '4fe307: Improvement #8406 - Increase MIN_PAGE_SIZE to 8192': only page_size = 8K was avaliable for usage.
    Fixed in https://github.com/FirebirdSQL/firebird/commit/6f6d16831919c4fa279189f02b93346c4d5ac1bf

    Checked on 6.0.0.876-6f6d168 ; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""
from pathlib import Path
import locale

import pytest
from firebird.qa import *

db = db_factory(charset='utf8')

substitutions = [('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)

tmp_fdb = temp_file('tmp_core_1715.fdb')

@pytest.mark.version('>=3')
def test_1(act: Action, tmp_fdb: Path, capsys):
    
    utf8_max_key_size_map = {4096 : (253, 169), 8192 : (509,339), 16384 : (1021,681)}
    if act.is_version('>=6'):
         del utf8_max_key_size_map[4096]
         utf8_max_key_size_map[32768] = (2045, 1363)

    expected_lst = []
    for pg_size, max_key_length_pair in utf8_max_key_size_map.items():

        tmp_fdb.unlink(missing_ok = True)
        
        max_key_4_bytes_per_char, max_key_6_bytes_per_char = max_key_length_pair[:2]
        passed_msg = f'Passed for {max_key_length_pair=}'

        test_script = f"""
            set bail on;
            set list on;
            create database 'localhost:{str(tmp_fdb)}'
                page_size {pg_size}
                default character set utf8
            ;
            select mon$page_size from mon$database;
            commit;
            create table test (
                vc_utf8_utf8 varchar({max_key_4_bytes_per_char}) character set utf8 collate utf8
               ,vc_utf8_unic varchar({max_key_6_bytes_per_char}) character set utf8 collate unicode
            );
            create index i_vc_utf8 on test (vc_utf8_utf8);
            create index i_vc_unic on test (vc_utf8_unic);
            commit;
            set list off;
            set heading off;
            select '{passed_msg}' from rdb$database;
            drop database;
        """

        act.isql(switches=['-q'], input = test_script, credentials = True, charset = 'utf8', connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())
        print(act.clean_stdout)
        act.reset()

        expected_lst.extend( [f'mon$page_size {pg_size}'.upper(), passed_msg] )

    act.expected_stdout = '\n'.join(expected_lst)
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
