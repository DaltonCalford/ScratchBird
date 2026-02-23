#coding:utf-8

"""
ID:          n/a
ISSUE:       n/a
TITLE:       Allow to create DB with owner containing a character from string.punctuation set, with duplicating it up to max allowed length
DESCRIPTION:
    Test verifies ability to create DB with assigning to its OWNER values like  '!' * 63 ; '[' * 63 ; '<' * 63 etc.
    After such DB will be created (in empbedded mode), we check ability to create USER in it (via Services API).
    It is better to use self-security DB because later this test can be extented by some code that operates with another cases
    of extremely 'exotic' user names (i.e. such users must not remain in the default security.db if this test fails).
NOTES:
    [22.01.2026] pzotov
        There are several special characters for which logic of this test is changed:  backslash ; qouble quotes ; apostrophe.
        1. For backslash: we have to duplicate every TRAILING backslash because it is used as python escaping character.
           Because in this test OWNER contains only one character, we have to duplicate the whole OWNER name
           (all backslashes can be considered as 'trailing')
        2. For double quotes and apostrophe: currently one can not query SEC$USERS table when there is stored such owners.
           Attempt to do that (using cursor) will fail with:
               > raise self.__report(DatabaseError, self.status.get_errors())
               E firebird.driver.types.DatabaseError: Missing terminating quote <'> in the end of quoted string
               E -Secondary attachment - config data from DPB ignored
           As of double quotes, sec$users.sec$user_name contains 63 space characters after we add user = owner.
           (although SHOW DATABASE command issues `Owner: <63 double quotes here>`)
        3. Additionally:
            3.1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
            3.2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
                 (for LINUX this equality is case-sensitive, even when aliases are compared!)
            3.3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf
                 (one need to replace it before every test session).
                 Discussed with pcisar, letters since 30-may-2022 13:48.
    Checked on 6.0.0.1394 5.0.4.1746 4.0.7.3243.
"""
import os
import random
import re
import string
import locale
from pathlib import Path
from firebird.driver import driver_config, DatabaseError, create_database, core as fb_core

import pytest
from firebird.qa import *

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

##############
REQUIRED_ALIAS = 'tmp_self_sec_alias'
CHECKED_CHARS_LST = list(string.punctuation)
##############

# Double quote and apostrophe can not be used: con.commit() fails on create_database() and
# attempt to run: create or alter user "{db_owner}" password '{db_passwd}' using plugin Srp
# Error;
#     > raise self.__report(DatabaseError, self.status.get_errors())
#     E firebird.driver.types.DatabaseError: Missing terminating quote <'> in the end of quoted string
#     E -Secondary attachment - config data from DPB ignored
# gdscode = 335545279
CHECKED_CHARS_LST.remove('"')
CHECKED_CHARS_LST.remove("'")

db = db_factory()
substitutions = []

act = python_act('db')

#--------------------------------------------------

def duplicate_trailing_backslashes_re(s):
    # Regex explanation:
    # (\\+)   - Captures one or more backslashes at the end of the string
    # $       - Asserts position at the end of the string
    # r'\1\1' - The replacement string, which doubles the captured group (group 1)
    return re.sub(r'(\\+)$', r'\1\1', s)

#--------------------------------------------------

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    MAX_USER_LENGTH = -1
    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute("select f.rdb$character_length from rdb$fields f where f.rdb$field_name = 'RDB$USER'")
        MAX_USER_LENGTH = cur.fetchone()[0] # currently this is 63 on 4.x ... 6.x
    assert MAX_USER_LENGTH > 0

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

    srv_cfg = """
        [local]
        host = localhost
        user = SYSDBA
        password = masterkey
    """
    srv_cfg = driver_config.register_server(name = 'test_db_owner_punct_chars', config = '')

    db_cfg_name = 'tmp_db_owner_punct_chars'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.protocol.value = None
    db_cfg_object.database.value = REQUIRED_ALIAS

    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )
    self_db = str(tmp_fdb)

    chk_sql = """
        select
             lower(m.mon$database_name) as db_name
            ,m.mon$sec_database as sec_db
            ,trim(m.mon$owner) as db_owner_text
            ,octet_length(trim(m.mon$owner)) as db_owner_octets
            ,trim(a.mon$user) as whoami_text
            ,octet_length(trim(a.mon$user)) as whoami_octets
            ,a.mon$attachment_name as att_name
            ,s.sec$user_name as sec_user_text
            ,octet_length(trim(s.sec$user_name)) as sec_user_octets
            ,left(a.mon$remote_protocol,3) as mon_protocol
        from mon$database m
        join mon$attachments a on a.mon$attachment_id = current_connection
        left join sec$users s on upper(trim(s.sec$user_name)) = upper(trim(m.mon$owner))
    """
    
    expected_out_lines = []

    for k in range(len(CHECKED_CHARS_LST)):

        Path(tmp_fdb).unlink(missing_ok = True)

        db_owner = CHECKED_CHARS_LST[k] * MAX_USER_LENGTH
        db_owner = duplicate_trailing_backslashes_re(db_owner)

        # code from 'create-db-with-arbitrary-characters-in-its-owner.py'
        # DO NOT REMOVE IT!
        while len(db_owner) > MAX_USER_LENGTH:
            lst = list(db_owner)
            for i in range(len(lst)):
                if lst[i] == '\\':
                    pass
                else:
                    lst[i] = ''
                    break
            db_owner = ''.join(lst)
            if len(db_owner) > MAX_USER_LENGTH:
                db_owner = db_owner[:-1]

        db_owner = db_owner.replace('"', '""')

        db_passwd = '123'

        user_created_ok = 0
        user_updated_ok = 0
        with create_database(db_cfg_name, user= f'"{db_owner}"', password = db_passwd) as con:
            try:
                con.execute_immediate(f'''create or alter user "{db_owner}" password '{db_passwd}' using plugin Srp ''')
                con.commit() # < fails when db_owner is sequence of double quote or apostrophe
                user_created_ok = 1
            except DatabaseError as e:
                print(f'Problem with CREATE USER >{db_owner}< in {self_db=}')
                print(e.__str__())
                print(e.gds_codes)

            # svc.update() raises:
            # >           self._srv()._svc.start(spb.get_buffer()) // core.py:5452
            # >       self._check()
            # ...
            # > firebird.driver.types.DatabaseError: invalid switch specified // interfaces.py:141 // gdscode=336724008

            #if user_created_ok:
            #    with fb_core.connect_server(server = srv_cfg.name, user = f'"{db_owner}"', password = db_passwd, expected_db = self_db) as srv:
            #        svc = fb_core.ServerUserServices(srv)
            #        try:
            #            svc.update( user_name = f'"{db_owner}"', password = db_passwd, database = self_db)
            #            #svc.update( user_name = db_owner, password = db_passwd, database = self_db)
            #            user_updated_ok = 1
            #        except DatabaseError as e:
            #            print(f'Problem with adding user >{db_owner}< in {self_db=}')
            #            #print(e)
            #            print(e.__str__())
            #            print(e.gds_codes)
            #            #print('List of all users:')
            #            #for u in svc.get_all(database = self_db):
            #            #    print(f'{u.user_name=}')
        # < with create_database

        if user_created_ok:
            with fb_core.connect('inet://' + REQUIRED_ALIAS, user= f'"{db_owner}"', password = db_passwd) as con:
                cur = con.cursor()
                try:
                    cur.execute(chk_sql)
                    ccol=cur.description
                    for r in cur:
                        for i in range(0,len(ccol)):
                            print( ccol[i][0],':', r[i])
                    add_block_to_expected_out = f"""
                        DB_NAME : {self_db.lower()}
                        SEC_DB : Self
                        DB_OWNER_TEXT : {db_owner}
                        DB_OWNER_OCTETS : {MAX_USER_LENGTH}
                        WHOAMI_TEXT : {db_owner}
                        WHOAMI_OCTETS : {MAX_USER_LENGTH}
                        ATT_NAME : {REQUIRED_ALIAS}
                        SEC_USER_TEXT : {db_owner}
                        SEC_USER_OCTETS : {MAX_USER_LENGTH}
                        MON_PROTOCOL : TCP
                    """
                    expected_out_lines.append(add_block_to_expected_out)
                except DatabaseError as e:
                    # find/display record error
                    # -Install incomplete. To complete security database initialization please CREATE USER.
                    print(f'Problem with obtaining data from mon$ and sec$ tables for user >"{db_owner}"< in {self_db=}')
                    # print(e)
                    print(e.__str__())
                    print(e.gds_codes)
        
            act.gfix( switches = ['-user', f'"{db_owner}"', '-pas', db_passwd, '-shutdown', 'full', '-force', '0', 'inet://' + REQUIRED_ALIAS], credentials = False, combine_output = True, io_enc = locale.getpreferredencoding() )

    # < for k in range(len(CHECKED_CHARS_LST))
    act.expected_stdout = '\n'.join(expected_out_lines)
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
