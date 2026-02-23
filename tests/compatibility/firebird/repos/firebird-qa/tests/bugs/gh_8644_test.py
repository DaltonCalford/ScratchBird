#coding:utf-8

"""
ID:          issue-8644
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8644
TITLE:       Connection error via Loopback provider if it's the first in the Providers parameter
DESCRIPTION:
    Test uses pre-created databases.conf which has alias (see variable REQUIRED_ALIAS) with Providers = Loopback,Remote,Engine14
    Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
NOTES:
    [26.06.2025] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    4. ::: NB :::
       TEST CHECKS *ONLY* FB 6.x! Other major versions currently are not checked because of need to change 'NN' suffix in EngineNN value
       of Providers parameter ('NN' it must be '13' for 4.x and 5.x; '12' for 3.x).
       Despite the fact that 'Providers' property is mentioned in DPB class (see core.py), one can *not* to specify it in custom DB config
       because this property actually is not initialized anywhere in the source code of firebird-driver.

    [03.02.2026] pzotov
       Added 'ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL' in .sql.
       Otherwise this test fails when is checked in loop ("Windows 32" error raises).

    Confirmed bug  on 6.0.0.949, got:
        Statement failed, SQLSTATE = 42000
        Execute statement error at attach :
        335545060 : Missing security context for ...
        Data source : Firebird::tmp_gh_...
    Checked on 6.0.0.1061.
"""

import locale
import re
import os
from pathlib import Path

import pytest
from firebird.qa import *

substitutions = [('[ \t]+', ' '), ]

REQUIRED_ALIAS = 'tmp_gh_8644_alias_6x'

db = db_factory(filename = '#' + REQUIRED_ALIAS, do_not_create = True, do_not_drop = True)
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):
    
    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder.
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_8644_alias = $(dir_sampleDb)/qa/tmp_qa_8644.fdb 
                # - then we extract filename: 'tmp_qa_8644.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )

    test_sql = f"""
        set list on;
        create database '{tmp_fdb}' user {act.db.user} password '{act.db.password}';
        select trim(replace(g.rdb$config_value,' ','')) as db_conf_providers from rdb$config g where upper(g.rdb$config_name) = upper('providers');
        commit;
        set term ^;
        execute block returns(out_arg bigint) as
        begin
            execute statement 'select 1 from rdb$database' 
            on external data source '{REQUIRED_ALIAS}' 
            as user '{act.db.user}' password '{act.db.password}' into :out_arg;
            suspend;
        end
        ^
        set term ;^
        commit;
        -- 03.02.2026: we have to clear external connection pool!
        -- Otherwise this test fails when is checked in loop ("Windows 32" error raises).
        ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL;
        commit;
    """

    act.isql(switches=['-q'], input = test_sql, connect_db = False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    for line in act.stdout.splitlines():
        print(line)
    tmp_fdb.unlink() # < fails if external connection pool was not cleared before!

    act.expected_stdout = """
        DB_CONF_PROVIDERS Loopback,Remote,Engine14
        OUT_ARG 1
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
