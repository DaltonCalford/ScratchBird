#coding:utf-8

"""
ID:          issue-6092
ISSUE:       6092
TITLE:       Not user friendly output of gstat at encrypted database
DESCRIPTION:
    Test encrypts empty database: we run 'ALTER DATABASE ENCRYPT ...' and wait until 'SHOW DATABASE'
    will display text that database encrypted.
    Then we check that 'gstat -h' output contains (beside encryption state in 'Attributes' line):
        * crypt checksum
        * key hash
        * encryption key name
    Because of randomness in data that is displayed, we use substitutions list which must suppress it.

JIRA:        CORE-5831
FBTEST:      bugs.core_5831
NOTES:
    [13.06.2022] pzotov
    Checked on 4.0.1.2692, 3.0.8.33535 - both on Linux and Windows.
"""
import os
import re
import datetime as py_dt
from datetime import timedelta

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

###########################
###   S E T T I N G S   ###
###########################

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
enc_settings = QA_GLOBALS['encryption']

# ACHTUNG: this must be carefully tuned on every new host:
#
MAX_WAITING_ENCR_FINISH = int(enc_settings['MAX_WAIT_FOR_ENCR_FINISH_WIN' if os.name == 'nt' else 'MAX_WAIT_FOR_ENCR_FINISH_NIX'])
assert MAX_WAITING_ENCR_FINISH > 0

ENCRYPTION_PLUGIN = enc_settings['encryption_plugin'] # fbSampleDbCrypt
ENCRYPTION_KEY = enc_settings['encryption_key'] # Red


substitutions = [
                  ('ATTRIBUTES (FORCE WRITE,)? ENCRYPTED, PLUGIN.*', 'ATTRIBUTES ENCRYPTED'),
                  ('CRYPT CHECKSUM.*', 'CRYPT CHECKSUM'),
                  ('KEY HASH: .*', 'KEY HASH')
                ]

db = db_factory()

act = python_act('db', substitutions=substitutions)

expected_stdout_gstat = """
    ATTRIBUTES ENCRYPTED
    CRYPT CHECKSUM: EDU3BS2YNQ2V+8OKQARRH2UQEKW=
    KEY HASH: 9EG30XIHPMHNOZJGGMLLOD+NA9Y=
    ENCRYPTION KEY NAME: RED
"""

@pytest.mark.encryption
@pytest.mark.version('>=3.0.4')
def test_1(act: Action, capsys):

    encryption_started = False
    encryption_finished = False
    with act.db.connect() as con:

        t1=py_dt.datetime.now()
        d1 = t1-t1
        sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"'
        try:
            con.execute_immediate(sttm)
            con.commit()
            encryption_started = True
        except DatabaseError as e:
            # -ALTER DATABASE failed
            # -Crypt plugin fbSampleDbCrypt failed to load
            #  ==> no sense to do anything else, encryption_started remains False.
            print( e.__str__() )

        while encryption_started:
            t2=py_dt.datetime.now()
            d1=t2-t1
            if d1.seconds*1000 + d1.microseconds//1000 > MAX_WAITING_ENCR_FINISH:
                print(f'TIMEOUT EXPIRATION: encryption took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {MAX_WAITING_ENCR_FINISH} ms.')
                break

            # Possible output:
            #     Database not encrypted
            #     Database encrypted, crypt thread not complete
            act.isql(switches=['-q'], input = 'show database;', combine_output = True)
            if 'Database encrypted' in act.stdout:
                if 'not complete' in act.stdout:
                    pass
                else:
                    encryption_finished = True
                    break
            act.reset()

        act.expected_stdout = ''
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

    if encryption_finished:

        act.gstat(switches=['-h'])

        db_attr_check_patterns = [
              r"\s*Attributes\.*"
             ,r"crypt\s+checksum:\s+\S+"
             ,r"key\s+hash:\s+\S+"
             ,r"encryption\s+key\s+name:\s+\S+"
        ]
        db_attr_check_patterns = [ re.compile(s, re.IGNORECASE) for s in db_attr_check_patterns ]

        for line in act.stdout.splitlines():
            if act.match_any(line, db_attr_check_patterns):
                print(line.upper().replace('\t',' '))

        act.expected_stdout = expected_stdout_gstat
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
