#coding:utf-8

"""
ID:          issue-5978
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5978
TITLE:       Access to the name of DB encryption key [CORE5712]
DESCRIPTION:
    Test uses Firebird built-in encryption plugin wich actually does encryption using trivial algorithm.
    Before running this test following prerequisites must be met:
       1. Files fbSampleKeyHolder.conf, fbSampleKeyHolder.dll, fbSampleDbCrypt.conf and fbSampleDbCrypt.dll 
          must be copied from $FB_HOME/examples/prebuilt/plugins/ to $FB_HOME/plugins/
          (on Linux name of binaries are: libfbSampleDbCrypt.so and libfbSampleKeyHolder.so)
       2. File fbSampleKeyHolder.conf must contain lines: Auto = true and KeyRed = <any number>
       3. File $QA_HOME/pytest.ini must contain line with 'encryption' marker declaration.

    We create temporary user with system privilege GET_DBCRYPT_INFO in order to allow him to obtain encryption info.
    Then we run following:
        1) encrypt DB using plugin 'fbSampleDbCrypt' provided in every FB 4.x+ snapshot;
        2) make connection as SYSDBA and ask DB-crypt info (DbInfoCode.CRYPT_PLUGIN and DbInfoCode.CRYPT_KEY)
        3) decrypt DB
    After this we repeat these actions, except that in "2)" we use temporary user ('tmp_senior') instead of SYSDBA
    (he must get same info as was obtained in previous step for SYSDBA).
NOTES:
    [08.05.2024] pzotov

    ### ACHTUNG ### TEST REQUIRES FIREBIRD-DRIVER VERSION 1.10.4+ (date: 07-may-2024).
    Thanks to pcisar for explanation of DbInfoCode usage.
    See letters with subj "fb_info_crypt_key: how it can be obtained using firebird-driver ? // GH-5978, 2018" (27.04.2024 14:55).

    Firebird 3.x can not be checked. Exception:
       raise NotSupportedError(f"Info code {info_code} not supported by engine version {self.__engine_version}")
       firebird.driver.types.NotSupportedError: Info code 138 not supported by engine version 3.0

    Checked on 4.0.5.3092, 5.0.1.1395, 6.0.0.346.
"""
import os
import locale
import re
import time
import datetime as py_dt

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError, DbInfoCode

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

db = db_factory()
act = python_act('db', substitutions = [('[ \t]+', ' ')])

# Create user to check ability to get info about crypt key name and plugin name
# by granting yto him system privilege 'GET_DBCRYPT_INFO'
# See: https://github.com/FirebirdSQL/firebird/issues/5978#issuecomment-826241686
# Full list of systyem privileges: src/jrd/SystemPrivileges.h
#
tmp_senior = user_factory('db', name='tmp$senior', password='456', plugin = 'Srp')

tmp_role = role_factory('db', name='tmp$role_get_dbcrypt_key')

#-----------------------------------------------------------------------

def run_encr_decr(act: Action, mode, max_wait_encr_thread_finish, capsys):
    
    assert mode in ('encrypt', 'decrypt')

    if mode == 'encrypt':
        alter_db_sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"'
        wait_for_state = 'Database encrypted'
    elif mode == 'decrypt':
        alter_db_sttm = 'alter database decrypt'
        wait_for_state = 'Database not encrypted'

    e_thread_finished = False

    # 0 = non crypted;
    # 1 = has been encrypted;
    # 2 = is DEcrypting;
    # 3 = is Encrypting;
    #
    REQUIRED_CRYPT_STATE = 1 if mode == 'encrypt' else 0
    current_crypt_state = -1
    d1 = py_dt.timedelta(0)
    with act.db.connect() as con:
        cur = con.cursor()

        ps = cur.prepare('select mon$crypt_state from mon$database')
        rs = None

        t1=py_dt.datetime.now()
        try:
            d1 = t1-t1
            con.execute_immediate(alter_db_sttm)
            con.commit()
            while True:
                t2=py_dt.datetime.now()
                d1=t2-t1
                if d1.seconds*1000 + d1.microseconds//1000 > max_wait_encr_thread_finish:
                    break
    
                ######################################################
                ###   C H E C K    M O N $ C R Y P T _ S T A T E   ###
                ######################################################
                rs = cur.execute(ps)
                current_crypt_state = cur.fetchone()[0]
                con.commit()
                if current_crypt_state == REQUIRED_CRYPT_STATE:
                    e_thread_finished = True
                    break
                else:
                    time.sleep(0.5)
        except DatabaseError as e:
            print(e.__str__())
            for x in e.gds_codes:
                print(x)
        finally:
            if rs:
                rs.close()
            if ps:
                ps.free()


    assert e_thread_finished, f'TIMEOUT EXPIRATION: {mode=} took {d1.seconds*1000 + d1.microseconds//1000} ms which {max_wait_encr_thread_finish=} ms'

#-----------------------------------------------------------------------

@pytest.mark.encryption
@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_senior: User, tmp_role: Role, capsys):

    # src/jrd/SystemPrivileges.h
    prepare_sql = f"""
        set bail on;
        set wng off;
        set list on;
        alter role {tmp_role.name}
            set system privileges to
                GET_DBCRYPT_INFO
        ;
        revoke all on all from {tmp_senior.name};
        grant default {tmp_role.name} to user {tmp_senior.name};
        commit;
    """
    
    # NB: "firebird.driver.types.InterfaceError: An error response was received" will raise if we
    # try to run as tmp_senior and miss 'grant default {tmp_role.name} to user {tmp_senior.name};'

    act.expected_stdout = ''
    act.isql(switches=['-q'], input = prepare_sql, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #......................................................

    try:
        run_encr_decr(act, 'encrypt', MAX_WAITING_ENCR_FINISH, capsys)

        for con_user in (act.db.user, tmp_senior.name):
            con_pswd = act.db.password if con_user == act.db.user else tmp_senior.password

            # ROLE not needed for tmp_senior because it will be granted as default, see above:
            with act.db.connect(user = con_user, password = con_pswd) as con:
                crypt_plugin = con.info.get_info(DbInfoCode.CRYPT_PLUGIN)
                crypt_key = con.info.get_info(DbInfoCode.CRYPT_KEY)
                print(f'{con_user=}')
                print(f'{crypt_plugin=}')
                print(f'{crypt_key=}')

        run_encr_decr(act, 'decrypt', MAX_WAITING_ENCR_FINISH, capsys)

    except DatabaseError as e:
        print(e.__str__())

    act.expected_stdout = f"""
        con_user='{act.db.user.upper()}'
        crypt_plugin='{ENCRYPTION_PLUGIN}'
        crypt_key='{ENCRYPTION_KEY}'

        con_user='{tmp_senior.name.upper()}'
        crypt_plugin='{ENCRYPTION_PLUGIN}'
        crypt_key='{ENCRYPTION_KEY}'
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
