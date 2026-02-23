#coding:utf-8

#coding:utf-8

"""
ID:          issue-8194
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8194
TITLE:       Internal consistency check (page in use during flush) with small number of DefaultDbCachePages
DESCRIPTION:
    Test uses pre-created databases.conf which has alias (see variable REQUIRED_ALIAS) with DefaultDbCachePages = 128.
    Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
NOTES:
    [02.08.2024] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    4. Custom driver config object ('db_cfg_object') is used: we have to use DB with predefined alias instead of temporary one.
    5. It is enough to set number of iterations to small number about 10...20 (see 'LOOP_LIMIT'): bugcheck raised after first 2...3 iter.

    Confirmed bug on 6.0.0.403, got in firebird.log:
        internal Firebird consistency check (page in use during flush (210), file: cch.cpp line: 2827)
    Checked on 6.0.0.406 - all fine.

    [23.04.2025] pzotov
    Confirmed bug on 5.0.3.1648-ca2f3e7, got:
        internal Firebird consistency check (page in use during flush (210), file: cch.cpp line: 2828)
    Checked on 5.0.3.1649-afcfa1a (intermediate snapshot) - all OK. Reduced min_version to 5.0.3.

    [20.05.2024] pzotov
    Added temporary mark 'disabled_in_forks' to SKIP this test when QA runs agains *fork* of standard FB.
"""

import re
from pathlib import Path
import pytest
from firebird.qa import *
from firebird.driver import driver_config, create_database, NetProtocol

substitutions = [('[ \t]+', ' '), ]
REQUIRED_ALIAS = 'tmp_gh_8194_alias'
LOOP_LIMIT = 20
SUCCESS_MSG = 'OK.'

db = db_factory()
act = python_act('db')

@pytest.mark.disabled_in_forks
@pytest.mark.version('>=5.0.3')
def test_1(act: Action, capsys):
    
    if act.get_server_architecture() != 'SuperServer':
        pytest.skip('Applies only to SuperServer')

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_8194_alias = $(dir_sampleDb)/qa/tmp_qa_8194.fdb 
                # - then we extract filename: 'tmp_qa_8194.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf
    
    srv_cfg = """
        [local]
        host = localhost
        user = SYSDBA
        password = masterkey
    """
    srv_cfg = driver_config.register_server(name = 'test_srv_gh_8194', config = '')

    db_cfg_name = 'tmp_8194'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.protocol.value = NetProtocol.INET
    db_cfg_object.database.value = REQUIRED_ALIAS
    for i in range(LOOP_LIMIT):
        # ::: NB :::
        # charset must be 'utf8' otherwise problem can not be reproduced!
        # (see also note by the author of ticket in his starting message)
        with create_database(db_cfg_name, user = act.db.user, password = act.db.password, charset = 'utf8') as con:
            con.drop_database()
    
    print(SUCCESS_MSG)

    act.expected_stdout = SUCCESS_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
