#coding:utf-8

"""
ID:          issue-8513
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8513
TITLE:       Makes MON$COMPILED_STATEMENTS and MON$STATEMENTS share blobs with text and plan content of the same statement
DESCRIPTION:
    Test runs ISQL with query that joins mon$statements and mon$compiled_statements on mon$compiled_statement_id value.
    ISQL must produce same BLOB_ID value for both these tables (we parse ISQL output and filter only interesting lines of it).
NOTES:
    [14.04.2025] pzotov
    Thanks to Vlad for suggestion about this test implementation.

    Confirmed different BLOB_ID values on 6.0.0.722.
    Reduced min_version to 5.0.3 (17.04.2025).
    Checked on 6.0.0.734, 5.0.3.1647-475d999 (intermediate snapshot).
"""

import os
import re
import time

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

msg_prefix = 'Total unique BLOB_ID values:'
#-----------------------------------------------------------

@pytest.mark.version('>=5.0.3')
def test_1(act: Action, capsys):
    test_sql = f"""
        set list on;
        set blob all;
        select
             s.mon$sql_text as blob_id_mon_statements
            ,c.mon$sql_text as blob_id_mon_compiled_s
        from mon$statements s
        join mon$compiled_statements c using(mon$compiled_statement_id)
        where mon$attachment_id = current_connection and s.mon$sql_text is not null
        ;
    """

    act.isql(switches = ['-q'], input = test_sql, combine_output = True)

    blob_id_pattern = re.compile('^blob_id_mon_', re.IGNORECASE);

    blob_ids_map = {}
    if act.return_code == 0:
        # Print only interesting lines from ISQl output tail:
        for line in act.clean_stdout.splitlines():
            if (blob_id_pattern.search(line)):
                blob_ids_map[ line.split()[0] ] = line.split()[1]
        if len(set(blob_ids_map.values())) == 1:
            print(f'{msg_prefix} {len(set(blob_ids_map.values()))}')
        else:
            print('UNEXPECTED: number of unique BLOB_ID values is different from 1:')
            for k,v in blob_ids_map.items():
                print(k,v)
    else:
        # If retcode !=0 then we can print the whole output of failed gbak:
        print('ISQL failed, check output:')
        for line in act.clean_stdout.splitlines():
            print(line)
    act.reset()

    expected_stdout = f"""
        {msg_prefix} 1
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
