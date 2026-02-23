#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6288
TITLE:       Incorrect handling of non-ASCII object names in CREATE MAPPING statement
TITLE:       Srp user manager sporadically creates users which can not attach
DESCRIPTION:
    Explanation of bug nature was provided by Alex, see letter 05-jun-19 13:51.
    Some iteration failed with probability equal to occurence of 0 (zero) in the
    highest BYTE of some number. Byte is 8 bit ==> this probability is 1/256.
    Given 'USERS_LIMIT' is number of iterations, probability of success for ALL of
    them is 7.5%, and when USERS_LIMIT is 1000 then p = 0.004%.
    Because of time (speed) it was decided to run only 256 iterations. If bug
    will be 'raised' somewhere then this number is enough to catch it after 2-3
    times of test run.
NOTES:
    [05.06.2019] pzotov
        Reproduced on WI-V3.0.5.33118, date: 11-apr-19 (got fails not late than on 250th iteration).
        Works fine on WI-V3.0.5.33139, date: 04-apr-19.
        A new bug was found during this test implementation, affected 4.0 Classic only: CORE-6080.
    [16.01.2026] pzotov
        Deep refactoring:
        * use self-security DB as test one in order to not to worry about remained users;
        * self-security DB is created by file-level copy of test database to the file with predefined
          name: $(dir_sampleDb)/qa/tmp_core_6038.fdb (databases.conf must have appropriate ALIAS for that).
          The speed up is provided by FW = OFF because any test DB by default has such attribute value;
        * linger set to positive value in the test (self-security) DB. This will keep this DB opened
          between subsequent connection;
        * list of really strong (random) passwords is created. SQL script is generated to create users
          with such passwords using Srp plugin. This SQL will be applied to DB using isql utility.
          Log of its execution must not contain 'SQLSTATE =' string (otherwise we show entire log).
        * check difference in firebird.log after all work is done (must remain empty).
        Test duration reduced from ~145 to ~25 seconds.
        Checked on:
            3.0.5.33118 (confirmed fail after random number of created users); 3.0.5.33139 (all fine);
            6.0.0.1393; 5.0.4.1746; 4.0.7.3243; 3.0.14.33829.
"""
import random
import string
import locale
import shutil
import re
from difflib import unified_diff
from pathlib import Path
from firebird.driver import ShutdownMethod, ShutdownMode

import pytest
from firebird.qa import *

############################
REQUIRED_ALIAS = 'tmp_core_6038_alias'
NAME_PREFIX = 'tmp$c6038_srp'
USERS_LIMIT = 256
############################

db = db_factory()
act = python_act('db', substitutions=[('[ \t]+', ' ')])
tmp_sql = temp_file('tmp_core_6038.sql')

#-------------------------------------------------------------------

def generate_random_string(length):
    characters = string.ascii_letters + string.digits + string.punctuation
    random_string = ''.join(random.choices(characters, k=length))
    return random_string

#-------------------------------------------------------------------

@pytest.mark.version('>=3.0.5')
def test_1(act: Action, tmp_sql: Path, capsys):

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_core_6288_alias = $(dir_sampleDb)/qa/tmp_core_6038.fdb
                # - then we extract filename: 'tmp_qa_8253.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf


    tmp_db = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )
    shutil.copy2(act.db.db_path, tmp_db)

    pwd_list = [generate_random_string(20).replace("'", "_") for x in range(USERS_LIMIT)]

    make_one_user_sql = f"""
        connect 'localhost:{REQUIRED_ALIAS}' user '{act.db.user}' password '{act.db.password}';
        create user {NAME_PREFIX}_%d password '%s' using plugin Srp;
        commit;
        connect 'localhost:{REQUIRED_ALIAS}' user '{NAME_PREFIX}_%d' password '%s';
        commit;
    """

    make_all_users_sql = '\n'.join( [ make_one_user_sql % (i, p, i, p) for i,p in enumerate(pwd_list) ] )

    check_sql = f"""
        set bail on;
        set list on;
        set echo on;
        connect '{REQUIRED_ALIAS}' user {act.db.user} password '{act.db.password}'; 
        create user {act.db.user} password '{act.db.password}';
        select mon$sec_database, mon$forced_writes from mon$database; -- must be: 'Self'
        commit;
        connect 'localhost:{REQUIRED_ALIAS}' user '{act.db.user}' password '{act.db.password}';
        alter database set linger to 30;
        commit;
        
        {make_all_users_sql}

        quit;
    """
    tmp_sql.write_text(check_sql, encoding='utf8')

    # Get content of firebird.log BEFORE test
    log_before = act.get_firebird_log()

    act.isql(switches = ['-q'], input_file = tmp_sql, credentials = False, connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())

    if 'SQLSTATE =' in act.clean_stdout:
        print('XXX NOT ALL USERS COULD BE CREATED. CHECK LOG: XXX')
        print('-' * 40)
        for line in act.clean_stdout.splitlines():
            print(line)
        print('-' * 40)

    # Normally no output must be at this point.
    # Otherwise it will contain entire script and outcome of every statements:
    #
    actual_stdout = capsys.readouterr().out
    act.reset()

    ###  achtung ###
    # We have to be sure that ISQL does not keep database file opened and thus it can be removed (LINGER > 0!).
    # Apply additional SQL to the test DB: restore linger to 0 and kill all remained attachments.
    ################
    cleanup_sql = f"""
        set list on;
        connect 'localhost:{REQUIRED_ALIAS}' user '{act.db.user}' password '{act.db.password}';
        alter database set linger to 0;
        commit;
        delete from mon$attachments where mon$attachment_id <> current_connection;
        select current_user, count(*) existing_users from sec$users where sec$user_name like '{NAME_PREFIX.upper()}%';
        exit;
    """
    act.isql(switches = ['-q'], input = cleanup_sql, credentials = False, connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())
    actual_stdout += '\n' + act.clean_stdout

    tmp_db.unlink(missing_ok = True)

    # Get content of firebird.log AFTER test (diff must remain empty)
    log_after = act.get_firebird_log()
    fb_log_diff = list(unified_diff(log_before, log_after))
    if fb_log_diff:
        actual_stdout += '\n\nCheck new lines in firbird.log:'
        for line in fb_log_diff:
            if line.startswith('+') and line[2:].strip():
                actual_stdout += '\n' + line.rstrip()

    actual_stdout = act.clean_string(actual_stdout, act.substitutions)

    act.expected_stdout = f"""
        USER {act.db.user.upper()}
        EXISTING_USERS {USERS_LIMIT}
    """
    assert actual_stdout == act.clean_expected_stdout
