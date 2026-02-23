#coding:utf-8

"""
ID:          issue-7772
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7772
TITLE:       Blob corruption in FB4.0.3 (embedded)
DESCRIPTION: 
    Problem appears when we use big streamed blobs with size 64K ... 128K.
    Test creates table and fills it using BLOB_APPEND() function.
    Then we do b/backup and make restore using embedded (local) mode.
    Values of octet_length and hash for blob in restored DB must be unchanged.
NOTES:
    [01.10.2023] pzotov
    Thanks to Vlad for provided example for test.
    Confirmed bug on 4.0.4.2997: restored database contains blob with size = 4464 which is less than initial size.

    Checked on 4.0.4.2998, 5.0.0.1235 (intermediate build), 6.0.0.65 (intermediate build) -- all fine.
"""
from io import BytesIO
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

BLOB_DATA_LEN = 70000
init_sql = f"""
    set term ^;
    create table t_blb
    (
        id int
       ,blb blob sub_type text
    ) ^
    commit ^
    create or alter function gen_blb(asize int)
       returns blob
    as
        declare b blob;
        declare str varchar(36);
    begin
        str = uuid_to_char(gen_uuid());
        while (asize > 0) do
        begin
            if (asize < 36) then
                begin
                    str = substring(str from 1 for asize);
                    asize = 0;
                end
            else
                asize = asize - 36;
            
            b = blob_append(b, str);
        end
        return b;
    end ^
    commit ^
    insert into t_blb values (1, gen_blb({BLOB_DATA_LEN})) ^
    commit ^
"""

db = db_factory(init = init_sql)
act = python_act('db')

#-------------------------------------------------------------------------------------

def get_blob_octets_hash(act: Action):
    blob_octets_len, blob_hash = -1, -1
    with act.db.connect() as con:
        cur = con.cursor()
        #cur.execute('select octet_length(blb) from t_blb')
        cur.execute('select octet_length(blb), crypt_hash(blb using sha512) from t_blb')
        for r in cur:
            blob_octets_len = r[0]
            blob_hash = r[1]
            # Convert binary to more readable view:
            blob_hash = format(int.from_bytes(blob_hash,  'big'), 'x')
            blob_hash = blob_hash.upper().rjust(128, '0')
    
    return blob_octets_len, blob_hash

#-------------------------------------------------------------------------------------

@pytest.mark.version('>=4.0.4')
def test_1(act: Action, capsys):

    init_len, init_hash = get_blob_octets_hash(act)

    # backup + restore without creating .fbk:
    #
    bkp_data = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database = act.db.db_path, backup_stream = bkp_data)
        bkp_data.seek(0)
        srv.database.local_restore(backup_stream = bkp_data, database = act.db.db_path, flags = SrvRestoreFlag.REPLACE)

    curr_len, curr_hash = get_blob_octets_hash(act)

    expected_stdout = "EXPECTED: blob data did not changed after backup-restore"

    if init_len == curr_len and init_hash == curr_hash:
        print(expected_stdout)
    else:
        print('UNEXPECTED: blob data CHANGED after backup-restore!')
        print(f'Initial len: {init_len}')
        print(f'Initial hash: {init_hash}')
        print(f'Current len: {curr_len}')
        print(f'Current hash: {curr_hash}')

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
