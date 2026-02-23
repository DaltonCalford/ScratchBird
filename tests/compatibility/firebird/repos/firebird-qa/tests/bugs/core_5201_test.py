#coding:utf-8

"""
ID:          issue-5482
ISSUE:       5482
TITLE:       Return nonzero result code when restore fails on activating and creating deferred user index
DESCRIPTION:
    According to Alex response on letter 25-apr-2016 19:15, zero retcode returned ONLY when restore
    was done WITH '-verbose' switch, and this was fixed.
    When restoring is performed without additional switches then retcode is 1.

    We create table with UNIQUE computed-by index which expression refers to other table (Firebird allows this!).
    Because other table (test_2) initially is empty, index _can_ be created. But after this we insert record into
    this table and do commit. Since that moment backup of this database will have table test_1 but its index will
    NOT be able to restore (unless '-i' switch specified).
    We will use this inability of restore index by checking 'gbak -rep -v ...' return code: it should be NON zero.
    If code will skip exception then this will mean FAIL of test.
JIRA:        CORE-5201
FBTEST:      bugs.core_5201
NOTES:
    [07.02.2023] pzotov
    Adjusted tail of restore log: added messages
        gbak: ERROR:Database is not online due to failure to activate one or more indices.
        gbak: ERROR:    Run gfix -online to bring database online without active indices.
   (actual since 5.0.0.932; will be soon also for FB 3.x and 4.x - see letter from Alex, 07.02.2023 11:53).

    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    
    Checked on 6.0.0.884; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from pathlib import Path
from firebird.qa import *
import re

init_script = """
    create table test_1(x int);
    create table test_2(x int);
    insert into test_1 values(1);
    insert into test_1 values(2);
    insert into test_1 values(3);
    commit;
    create unique index test_1_unq on test_1 computed by( iif( exists(select * from test_2), 1, x ) );
    commit;
    insert into test_2 values(1000);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

fbk_file = temp_file('core_5201.fbk')
tmp_db_file = temp_file('tmp_core_5201.fdb')

@pytest.mark.version('>=3.0')
def test_1(act: Action, fbk_file: Path, tmp_db_file: Path):

    with act.connect_server() as srv:
        srv.database.backup(database=act.db.db_path, backup=fbk_file)
        assert srv.readlines() == []
    #

    gbak_expected_out_5x = """
        gbak: ERROR:attempt to store duplicate value (visible to active transactions) in unique index "TEST_1_UNQ"
        gbak: ERROR:    Problematic key value is (<expression> = 1)
        gbak: ERROR:Database is not online due to failure to activate one or more indices.
        gbak: ERROR:    Run gfix -online to bring database online without active indices.
    """

    gbak_expected_out_6x = """
        gbak: ERROR:attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."TEST_1_UNQ"
        gbak: ERROR:    Problematic key value is (<expression> = 1)
        gbak: ERROR:Database is not online due to failure to activate one or more indices.
        gbak: ERROR:    Run gfix -online to bring database online without active indices.
    """

    act.expected_stdout = gbak_expected_out_5x if act.is_version('<6') else gbak_expected_out_6x

    act.gbak(switches=['-rep', '-v', str(fbk_file), str(tmp_db_file)], combine_output = True)
    p_gbak_err = re.compile('^gbak:\s?ERROR:', re.IGNORECASE)

    # filter stdout
    act.stdout = '\n'.join([line for line in act.stdout.splitlines() if p_gbak_err.search(line)])
    assert act.return_code == 2
    assert act.clean_stdout == act.clean_expected_stdout
