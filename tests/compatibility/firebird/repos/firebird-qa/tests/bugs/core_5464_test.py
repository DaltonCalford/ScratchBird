#coding:utf-8

"""
ID:          issue-5734
ISSUE:       5734
TITLE:       AV in fbclient when reading blob stored in incompatible encoding
DESCRIPTION:
    Domain description contains non-ascii text in Latvian
    and is created using charset = win1257.
    Subsequent connect which tries to get this description uses cp1253 (Greek).
    Commit that fixed ticket: 0fab1a85597baa5054a34cae437f5da6096580b0 (20.01.2017 00:43)
JIRA:        CORE-5464
FBTEST:      bugs.core_5464
NOTES:
    [30.10.2024] pzotov
    Crash *not* occurs but one may note different behaviour of snapshots before and after fix.

    Snapshot before fix (e.g. 90a46fa3, 06-jan-2017) for query to rdb$fields (see view v_domain_descr)
    behave differently depending on connection protocol:
        * for TCP is does not return any record for query to view 'v_conn_cset';
        * for LOCAL protocol its returns weird 'RDB$SYSTEM_FLAG 18775' and error 'SQLSTATE = 42000 / invalid BLOB ID'.

    Also, error message for query to view 'v_domain_descr' (before fix) was:
        Statement failed, SQLSTATE = HY000
        Cannot transliterate character between character sets
        request synchronization error

    Discussed with Vlad, letters date: 29-oct-2024.
    Checked on 6.0.0.511 (Windows/Linux); 5.0.2.1550;  4.0.6.3165; 3.0.13.33793; 3.0.2.32670-0fab1a8.
"""
import locale
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory(charset='win1257')
act = isql_act('db', substitutions = [('TCPv(4|6)', 'TCP')])

tmp_sql = temp_file('tmp_core_5464.sql')

@pytest.mark.intl
@pytest.mark.version('>=3.0.1')
def test_1(act: Action, tmp_sql: Path, capsys):

    non_ascii_txt = """
        Oblonsku mājā viss bija sajaukts.
        Sieva uzzināja, ka viņas vīram ir attiecības ar franču guvernanti,
        kas atradās viņu mājā, un paziņoja vīram, ka nevar dzīvot ar viņu vienā mājā.
    """

    init_script = f"""
        create domain dm_test int;
        comment on domain dm_test is '{non_ascii_txt}';
        commit;
        create view v_conn_cset as
        select
             rdb$get_context('SYSTEM', 'NETWORK_PROTOCOL') as conn_protocol
            ,c.rdb$character_set_name as connection_cset
            ,r.rdb$character_set_name as db_default_cset
        from mon$attachments a
        join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
        cross join rdb$database r where a.mon$attachment_id=current_connection;

        create view v_domain_descr as
        select f.rdb$field_name, f.rdb$system_flag, f.rdb$description
        from rdb$database d
        left join rdb$fields f on f.rdb$description is not null;
        commit;
    """
    tmp_sql.write_bytes(init_script.encode('cp1257'))
    act.isql(switches=['-q'], input_file = tmp_sql, charset='win1257', combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.return_code == 0

    test_sql = f"""
        set blob all;
        set list on;
        set count on;
        connect '{act.db.dsn}';
        select v1.* from v_conn_cset as v1;
        select v2.* from v_domain_descr as v2;
        commit;

        connect '{act.db.db_path}';
        select v3.* from v_conn_cset as v3;
        select v4.* from v_domain_descr as v4;
        commit;
    """
    act.isql(switches=['-q'], connect_db = False, input = test_sql, charset='win1253', combine_output = True, io_enc = locale.getpreferredencoding())

    act.expected_stdout = """
        CONN_PROTOCOL                   TCP
        CONNECTION_CSET                 WIN1253
        DB_DEFAULT_CSET                 WIN1257
        Records affected: 1

        Statement failed, SQLSTATE = 22018
        Cannot transliterate character between character sets
        Records affected: 0


        CONN_PROTOCOL                   <null>
        CONNECTION_CSET                 WIN1253
        DB_DEFAULT_CSET                 WIN1257
        Records affected: 1

        Statement failed, SQLSTATE = 22018
        Cannot transliterate character between character sets
        Records affected: 0
    """
    assert act.clean_stdout == act.clean_expected_stdout
