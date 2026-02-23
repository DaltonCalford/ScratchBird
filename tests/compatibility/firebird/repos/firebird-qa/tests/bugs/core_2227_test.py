#coding:utf-8

"""
ID:          issue-2655
ISSUE:       2655
TITLE:       Problem with column names with Accents and triggers
DESCRIPTION:
JIRA:        CORE-2227
FBTEST:      bugs.core_2227
NOTES:
    [31.10.2024] pzotov
    Bug was fixed for too old FB (2.1.2; 2.5 Beta1) so firebird-driver and/or QA-plugin
    will not able to run on this version in order to reproduce problem.
    Source for this test was taken from ticket almost w/o changes. Only aux view has been added ('v_conn_cset') for
    showing current connection protocol and character set - we make query to this view two twice: one for TCP and then
    for local protocol.

    Checked on 6.0.0.511 (Windows/Linux); 5.0.2.1550;  4.0.6.3165; 3.0.2.32670, 3,0,1,32609
"""
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory(charset='ISO8859_1')

act = isql_act('db', substitutions = [ ('[ \\t]+', ' '), ('TCPv(4|6)', 'TCP') ])

tmp_sql = temp_file('tmp_core_2227.sql')

@pytest.mark.intl
@pytest.mark.version('>=3.0.0')
def test_1(act: Action, tmp_sql: Path):
    test_script = f"""
        set bail on;
        set list on;
        recreate table testing (
            "CÓDIGO" integer
        );
        commit;
        set term ^;
        create trigger testing_i for testing active before insert position 0 as
        begin
            new."CÓDIGO" = 1;
        end
        ^
        set term ;^
        commit;

        create view v_conn_cset as
        select
             rdb$get_context('SYSTEM', 'NETWORK_PROTOCOL') as conn_protocol
            ,c.rdb$character_set_name as connection_cset
            ,r.rdb$character_set_name as db_default_cset
        from mon$attachments a
        join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
        cross join rdb$database r where a.mon$attachment_id=current_connection;
        commit;

        connect '{act.db.dsn}';
        select * from v_conn_cset;
        insert into testing default values returning "CÓDIGO";
        rollback;

        connect '{act.db.db_path}';
        select * from v_conn_cset;
        insert into testing default values returning "CÓDIGO";
    """

    tmp_sql.write_text(test_script, encoding='iso8859_1')
    act.expected_stdout = """
        CONN_PROTOCOL TCPv4
        CONNECTION_CSET ISO8859_1
        DB_DEFAULT_CSET ISO8859_1
        CÓDIGO 1

        CONN_PROTOCOL <null>
        CONNECTION_CSET ISO8859_1
        DB_DEFAULT_CSET ISO8859_1
        CÓDIGO 1
    """
    act.isql(switches = ['-q'], input_file = tmp_sql, charset = 'iso8859_1', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
