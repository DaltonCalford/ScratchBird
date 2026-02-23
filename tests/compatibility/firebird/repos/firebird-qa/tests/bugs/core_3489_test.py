#coding:utf-8

"""
ID:          issue-3848
ISSUE:       3848
TITLE:       Blob transliteration may not happen inside the union
DESCRIPTION:
JIRA:        CORE-3489
FBTEST:      bugs.core_3489
NOTES:
    [30.10.2024] pzotov
    Bug was fixed for too old FB (3.0 Alpha 1), firebird-driver and/or QA-plugin
    will not able to run on this version in order to reproduce problem.
    Source for this test was taken from ticket almost w/o changes. Only aux view has been added ('v_conn_cset') for
    showing current connection protocol and character set - we make query to this view two twice: one for TCP and then
    for local protocol.

    Checked on 6.0.0.511 (Windows/Linux); 5.0.2.1550;  4.0.6.3165; 3.0.2.32670, 3,0,1,32609
"""

import locale
from pathlib import Path
import pytest
from firebird.qa import *

db = db_factory(charset='WIN1251')

act = python_act('db', substitutions=[('MSG_BLOB_ID.*', ''), ('TCPv(4|6)', 'TCP')])

expected_stdout = """
    CONN_PROTOCOL                   TCP
    CONNECTION_CSET                 WIN1251
    DB_DEFAULT_CSET                 WIN1251
    Records affected: 1
    Это проверка на вывод строки "Йцукёнг"
    Это проверка на вывод строки "Йцукёнг"
    Records affected: 2

    CONN_PROTOCOL                   <null>
    CONNECTION_CSET                 WIN1251
    DB_DEFAULT_CSET                 WIN1251
    Records affected: 1
    Это проверка на вывод строки "Йцукёнг"
    Это проверка на вывод строки "Йцукёнг"
    Records affected: 2
"""

tmp_sql = temp_file('tmp_core_3489.sql')

@pytest.mark.intl
@pytest.mark.version('>=3.0.0')
def test_1(act: Action, tmp_sql: Path):
    tmp_sql.write_text(
    f"""
        set bail on;
        set list on;
        set blob all;
        set count on;
        set names win1251;
        connect '{act.db.dsn}';
        create view v_conn_cset as
        select
             rdb$get_context('SYSTEM', 'NETWORK_PROTOCOL') as conn_protocol
            ,c.rdb$character_set_name as connection_cset
            ,r.rdb$character_set_name as db_default_cset
        from mon$attachments a
        join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
        cross join rdb$database r where a.mon$attachment_id=current_connection;

        set term ^;
        create or alter procedure sp_test
        returns (
                msg_blob_id blob sub_type 1 segment size 80 character set unicode_fss)
        AS
        begin
                msg_blob_id= 'Это проверка на вывод строки "Йцукёнг"'; -- text in cyrillic
                suspend;
        end
        ^
        set term ;^
        commit;
        --------------------------
        connect '{act.db.dsn}';     -- check TCP protocol
        select * from v_conn_cset;
        select msg_blob_id
        from sp_test
        union
        select msg_blob_id
        from sp_test;
        commit;
        --------------------------
        connect '{act.db.db_path}'; -- check local protocol
        select * from v_conn_cset;
        select msg_blob_id
        from sp_test
        union
        select msg_blob_id
        from sp_test;
    """
    ,encoding='cp1251')
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input_file = tmp_sql, charset = 'WIN1251', combine_output = True, connect_db = False)
    assert act.clean_stdout == act.clean_expected_stdout
