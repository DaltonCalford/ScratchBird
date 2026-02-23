#coding:utf-8

"""
ID:          issue-8774
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8774
TITLE:       Gstat -table will only report one matching table
DESCRIPTION:
    We create several schemas (see SCHEMAS_LST) and several tables within each of them.
    All table names for some schema are generated using names from SCHEMAS_LST + suffix '_tab'
    (thus, total number of tables will be len(SCHEMAS_LST)**2, but number of their unique
    names equals to the length of SCHEMAS_LST).
    Each unique table name <T> must be found in all schemas when obtaining statistics using
    services API and specifying this <T> as argument to the 'tables' list.
    For each <T> we have to find in the DB statistics output:
        * line of the form '"<schema_name>"."<table_name>" (<id>)' where ID must
          correspond to rdb$relations.relation_id value for this schema+table
          (for all created schemas)
        * line of the form 'Index "<schema_name>.<index_name>"'
    If we found such lines for every table then test is considered as passed and
    its output will remain empty.
    Otherwise names of missed items (tables) are printed.
NOTES:
    [24.10.2025] pzotov
    On Windows we can *not* use non-ascii table names if DB statistics is gathered with '-t' option.
    Otherwise exception 'table not found' raises.
    Sent report to FB-team, 24.10.2025 14:14.

    Confirmed bug on 6.0.0.1306
    Checked on 6.0.0.1313-8ee208b.
"""
import traceback
import locale
from pathlib import Path
from firebird.driver import SrvStatFlag

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')
act = python_act('db')

SCHEMAS_LST= (
    'SCHEMA',
    'system',
    'public',
)

TABLES_MAP = {}

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    stat_output = []
    with act.db.connect(charset = 'utf8') as con:
        
        for s in SCHEMAS_LST:
           cur = con.cursor()
           con.execute_immediate(f'create schema "{s}"')
           con.commit()
           for t in SCHEMAS_LST:
               tab_name = t + '_tab'
               idx_name = t + '_idx'
               con.execute_immediate(f'create table "{s}"."{tab_name}"(id int primary key using index "{idx_name}")')
               con.commit()
               cur.execute(f"select rdb$relation_id from rdb$relations where rdb$schema_name = '{s}' and rdb$relation_name = '{tab_name}'")
               TABLES_MAP[ (s, tab_name) ] = [cur.fetchone()[0], -1, -1, '', '']

    # ('SCHEMA', 'SCHEMA_tab') ::: [128, -1, -1]
    # ('SCHEMA', 'system_tab') ::: [129, -1, -1]
    # ('SCHEMA', 'public_tab') ::: [130, -1, -1]
    # ('system', 'SCHEMA_tab') ::: [131, -1, -1]
    # ('system', 'system_tab') ::: [132, -1, -1]
    # ('system', 'public_tab') ::: [133, -1, -1]
    # ('public', 'SCHEMA_tab') ::: [134, -1, -1]
    # ('public', 'system_tab') ::: [135, -1, -1]
    # ('public', 'public_tab') ::: [136, -1, -1]

    with act.connect_server(encoding = 'utf8', encoding_errors = locale.getpreferredencoding()) as srv:
        for t in set([x[1] for x in TABLES_MAP.keys()]):
            srv.database.get_statistics(
                database=act.db.db_path
               ,tables=[t]
               ,flags=SrvStatFlag.DATA_PAGES | SrvStatFlag.IDX_PAGES
            )
            try:
                # This failed before #8789 was fixed if schema/table/index required 4 bytes per char for encoding:
                stat_output = [x.rstrip() for x in srv.readlines() if x.strip()]
                for i,line in enumerate(stat_output):
                    # "SCHEMA"."SCHEMA_tab" (128)
                    #  Index "SCHEMA"."SCHEMA_idx" (0)
                    if line.startswith('"') or line.strip().startswith('Index'):
                        for k,v in TABLES_MAP.items():
                            sch_name, tab_name = k[:2]
                            idx_name = tab_name.replace('_tab', '_idx')
                            if line.startswith('"' + sch_name + '"."' + tab_name + '"'):
                                if line.endswith( f' ({v[0]})' ):
                                    v[1] = i
                                    v[3] = line # 4debug
                            if line.strip().startswith('Index "' + sch_name + '"."' + idx_name + '"'):
                                    #     Index "SCHEMA"."SCHEMA_idx" (0)
                                    v[2] = i
                                    v[4] = line # 4debug
                            TABLES_MAP[ k ] = v

            except Exception as e: # BufferError, UnicodeDecodeError
                print('Unexpected error occurs during attempt to get lines of DB statistics:')
                print(e.__class__) # <class 'firebird.driver.types.DatabaseError'>
                print(e.__str__()) # table "<non_ascii_name>" not found
                for p in traceback.format_exc().split('\n'):
                    print('...',p)

    missed_sch_tab = {k for k,v in TABLES_MAP.items() if v[1] <= 0 or v[2] <= 0}
    if missed_sch_tab:
        print(f'UNEXPECTED. Could not find lines for following schemas and/or tables:')
        for p in missed_sch_tab:
            print(f'Schema: {p[0]}, table: {p[1]}' )
        print('Check TABLES_MAP:')
        for k,v in TABLES_MAP.items():
            print(k,':::',v)


    expected_stdout = f"""
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
