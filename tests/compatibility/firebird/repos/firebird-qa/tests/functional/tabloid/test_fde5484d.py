#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/fde5484d8f505d6d317f0e93860f3cc2765fe4e0
TITLE:       Increase isql max password length
DESCRIPTION:
    Test creates user with random password with length = maximal possible length minus 1 byte.
    Then we try to run isql and use such password as value for '-pass' command switch.
    Before fix, this failed with SQLSTATE = 28000 / Your user name and password are not defined.
    See letter from dimitr, 04.02.2026 10:57.
NOTES:
    [04.02.2026] pzotov
    Commits (all of them changed src/isql/isql.h: const int PASSWORD_LENGTH = 8192'):
        3.x: fde5484d8f505d6d317f0e93860f3cc2765fe4e0 // 04-feb-26 0336
        4.x: ea1c15c54491fc2eaecd2fb0cf221d35d5e99210 // 04-feb-26 0325
	    5.x: 51d9df9196a624949917b03eb9e1ad16558eb535 // 01-feb-26 1551
	    6.x: 761a49defddcee77d4cc2f1c7548163ed8a082dd // 04-feb-26 0324
	
	Currently only 6.x can pass this test for all generated passwords.
	Previous FB versions fail when password contains apostrophes or double quotes.

	One need to duplicate each occurrence of apostroph (ascii_char=39) when spicyfing it in SQL script.
	Double quotes (ascii_char=34) must be specified 'as is', i.e. must not be escaped or duplicated:
	    create or alter user foo password 'o''hara';
	    create or alter user bar password '"o""hara';
	If we run ISQL from COMMAND LINE to connect (rather from Python!) then '-passord' value:
	    1) must be enclosed in double quotes if password contains apostroph:
               disk:\\path> isql localhost:employee -user foo -pas "o'hara"
	    2) must have backslash as prefix for each double quote if password contains double quotes:
               disk:\\path> isql localhost:employee -user bar -pas \"o\"\"hara
           The need of usage backslash before each double quote was explained by Dm. Sibiryakov, see
           letter: 18.01.2026 12:54, subj: isql -user "john_smith" ...: SQLSTATE = 28000 ...
           =======
           command-line interpreter removes double quotes from parameters so ISQL gets name/password
           without this character. Possible workaround: -user \"john_must_be_in_lowercase\"
           =======

    But if we run ISQL from Pythons (using suprocess.run) then password should not be changed at all.
    Checked on 6.0.0.1405-761a49d.
"""
import os
import random
import string
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory()

# see src/isql/isql.h:
# const int PASSWORD_LENGTH = ...
#
MAX_PASSWORD_LEN = 8191
ITER_COUNT = 5

# string.punctuation
# '!"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~'

CHECKED_CHARS_LST = list(string.punctuation)

tmp_user = user_factory('db', name = 'tmp$fde5484d', password = '123')
tmp_pswf = temp_file('test_fde5484d.password')

substitutions = [('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)

# two special cases must be checked always: apostrophe and double quotes, duplicated <MAX_PASSWORD_LEN> times
ITER_COUNT = ITER_COUNT + 2

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_user: User, tmp_pswf: Path, capsys):

    check_sql = 'set list on;select current_user as whoami from rdb$database;'
    for iter in range(ITER_COUNT):
        if iter == 0:
            checked_pswd = "'" * MAX_PASSWORD_LEN
        elif iter == 1:
            checked_pswd = '"' * MAX_PASSWORD_LEN
        else:
            checked_pswd = ''.join(random.choices(CHECKED_CHARS_LST, k = MAX_PASSWORD_LEN))

        pswd_in_script = checked_pswd.replace("'", "''")
        pswd_in_cmdarg = checked_pswd
        # ::: NB ::: THIS IS NOT NEEDED WHEN CALL ISQL FROM PYTHON: pswd_in_cmdarg = checked_pswd.replace('"', '\\"')
        with act.db.connect() as con:
            # alter user foo password 'o''hara'
            con.execute_immediate(f"alter user /* trace_me iter={iter+1} */ {tmp_user.name} password '{pswd_in_script}'")
            con.commit()
        tmp_pswf.write_text(checked_pswd)
 
        for pswd_transfer in ('cmd_param', 'fetch_from'):
            protocols_lst = ('inet', 'xnet') if os.name =='nt' else ('inet',)
            for prot_prefix in protocols_lst:
                dsn = prot_prefix + '://' + str(act.db.db_path)
                isql_args = [dsn, '-q', '-user', tmp_user.name]
                isql_args.extend( ['-pas', pswd_in_cmdarg] if pswd_transfer == 'cmd_param' else ['-fe', str(tmp_pswf)] )
                act.isql(switches = isql_args, input = check_sql, connect_db = False, credentials = False, combine_output = True)

                expected_stdout = f"""
                    WHOAMI {tmp_user.name.upper()}
                """
                act.expected_stdout = expected_stdout
                if act.return_code ==0 and act.clean_stdout == act.clean_expected_stdout:
                    pass
                else:
                    print(f'\n\niter={iter+1} of total {ITER_COUNT}')
                    print('Problem with transferring password detected.')
                    print(f'Specified via: {pswd_transfer}')
                    print(f'ISQL returned code: {act.return_code}')
                    if pswd_transfer == 'cmd_param':
                        print('ISQL command params:')
                        print( ' '.join(isql_args) )
                    print(f'Protocol used: {prot_prefix}')
                    print('ISQL output:\n' + act.clean_stdout)
                    print('Password: ' + checked_pswd)
                act.reset()
    
    assert capsys.readouterr().out == ''
