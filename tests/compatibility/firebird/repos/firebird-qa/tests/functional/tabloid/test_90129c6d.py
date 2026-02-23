#coding:utf-8

"""
ID:          issue-90129c6d
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/90129c6d3ebd5d1c9a7b44d287a4791dffcd031e
TITLE:       Scrollable cursors. Fixed isBof/isEof when the prefetch is active
DESCRIPTION:
    is_bof()/is_eof() could became True much earlier than actual position of cursor did come to BOF/EOF.
    This occurred on fetch_prior() / fetch_next() calls and caused wrong result for counting number of
    processed rows.
    Discussed with dimitr, see letters with subj = "firebird-driver & scrollable cursors".
    Dates: 27.11.2021 21:20, 28.11.2021 19:00, 29.11.2021 07:40
NOTES:
    [19.07.2024] pzotov
    1. No ticket has been created for described problem.
       Problem was fixed 28.11.2021 at 16:09, commit #90129c6d.
       Confirmed bug on 5.0.0.321 (28.11.2021). Fixed in 5.0.0.324 (29.11.2021).
    2. Initial test contained too big values for check:
           N_ROWS = 10000
           N_WIDTH = 32765
           LOOP_COUNT = 1000
       They can be safely replaced with minimal possible values in order to see difference before and after fix.
    3. NOTE that argument passed to cur.stream_blobs.append() must be equal to the name of blob column as it is
       stored in RDB$ tables, i.e. in uppercase. Because of that, variable 'BLOB_FLD_NAME' is used instead of
       repeating blob column name in DDL and cur.stream_blobs.append().
    4. Custom driver-config object must be used for DPB because two values of WireCrypt parameter must be checked:
       Enabled and Disabled (see 'w_crypt').

    Checked on 6.0.0.396, 5.0.1.1440.
"""

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol
import time

db = db_factory()
act = python_act('db')

N_ROWS = 1
N_WIDTH = 1
LOOP_COUNT = 2
BLOB_FLD_NAME = 'BINARY_DATA'

@pytest.mark.scroll_cur
@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):
    
    srv_cfg = driver_config.register_server(name = 'test_90129c6d_srv', config = '')

    for w_crypt in ('Enabled', 'Disabled'):
        db_cfg_name = f'test_90129c6d_wcrypt_{w_crypt}'
        db_cfg_object = driver_config.register_database(name = db_cfg_name)
        db_cfg_object.server.value = srv_cfg.name
        db_cfg_object.protocol.value = NetProtocol.INET
        db_cfg_object.database.value = str(act.db.db_path)
        db_cfg_object.config.value = f"""
            WireCrypt = w_crypt
        """

        for suitable_for_compression in (0,1):
            if suitable_for_compression:
                data_dml = f"""
                    execute block as
                        declare n int = {N_ROWS};
                    begin
                        while ( n > 0 ) do
                        begin
                            insert into ts(id,{BLOB_FLD_NAME}) values(:n, lpad('',  {N_WIDTH}, 'A'));
                            n = n - 1;
                        end
                    end
                """
            else:
                data_dml = f"""
                    execute block as
                       declare n int = {N_ROWS};
                       declare n_wid int = {N_WIDTH};
                       declare encrypted_text varchar({N_WIDTH}) character set octets;
                       declare encr_addition varchar(16) character set octets;
                    begin
                       while (n > 0) do
                       begin
                           encrypted_text = '';
                           encr_addition = '';
                           while ( 1 = 1 ) do
                           begin
                               encr_addition = gen_uuid();
                               if ( octet_length(encrypted_text) < n_wid - octet_length(encr_addition) ) then
                                   encrypted_text = encrypted_text || trim(encr_addition);
                               else
                                   begin
                                       encrypted_text = encrypted_text || left(encr_addition, n_wid - octet_length(encrypted_text));
                                       leave;
                                   end
                           end
                           insert into ts(id,{BLOB_FLD_NAME}) values(:n, :encrypted_text);
                           n = n - 1;
                       end
                    end
                """

            init_sql = f"""
                recreate table ts(id int primary key, {BLOB_FLD_NAME} blob)
                ^
                commit
                ^
                {data_dml}
                ^
                commit
                ^
            """

            with act.db.connect() as con:
                cur = con.cursor()
                for x in init_sql.split("^"):
                    s = x.lower().strip()
                    if s == "commit":
                        con.commit()
                    elif s:
                        cur.execute(s)

            with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
                assert (True if w_crypt else False) == con.info.is_encrypted(), f'Value of {con.info.is_encrypted()=} not equals to required: {w_crypt=}'

                cur = con.cursor()
                cur.stream_blobs.append(BLOB_FLD_NAME.upper())
                cur.open(f'select id, {BLOB_FLD_NAME} from ts order by id')

                for iter_no in range(LOOP_COUNT):
                    cnt_fwrd=0
                    while True:
                        fetched_row_data = cur.fetch_next()
                        if cur.is_eof():
                            break
                        v_id, v_blob_data = fetched_row_data
                        with v_blob_data:
                            v_blob_data.read()
                        cnt_fwrd += 1

                    cnt_back=0
                    while True:
                        fetched_row_data = cur.fetch_prior()
                        if cur.is_bof():
                            break
                        v_id, v_blob_data = fetched_row_data
                        with v_blob_data:
                            v_blob_data.read()
                        cnt_back += 1

                    print(f'{w_crypt=}, {suitable_for_compression=}, {iter_no=}: {cnt_fwrd=}, {cnt_back=}')

    act.expected_stdout = """
        w_crypt='Enabled', suitable_for_compression=0, iter_no=0: cnt_fwrd=1, cnt_back=1
        w_crypt='Enabled', suitable_for_compression=0, iter_no=1: cnt_fwrd=1, cnt_back=1
        w_crypt='Enabled', suitable_for_compression=1, iter_no=0: cnt_fwrd=1, cnt_back=1
        w_crypt='Enabled', suitable_for_compression=1, iter_no=1: cnt_fwrd=1, cnt_back=1
        w_crypt='Disabled', suitable_for_compression=0, iter_no=0: cnt_fwrd=1, cnt_back=1
        w_crypt='Disabled', suitable_for_compression=0, iter_no=1: cnt_fwrd=1, cnt_back=1
        w_crypt='Disabled', suitable_for_compression=1, iter_no=0: cnt_fwrd=1, cnt_back=1
        w_crypt='Disabled', suitable_for_compression=1, iter_no=1: cnt_fwrd=1, cnt_back=1
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
