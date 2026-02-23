#coding:utf-8

"""
ID:          issue-8773
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8773
TITLE:       Gstat output of tables does not include schema name
DESCRIPTION:
    We create several schemas and tables+indices within each of them.
    Name of table equals to the name of schema concatenated with '_tab' suffix.
    Name of index equals to the name of schema concatenated with '_pk' suffix.
    For each schema we have to find in the DB statistics output:
        * line of the form '"<schema_name>"."<table_name>" (<id>)' where ID must
          correspond to rdb$relations.relation_id value for this schema+table;
        * line of the form 'Index "<schema_name>.<index_name>"'
    If we found such lines for every schema test is considered as passed and
    its output will remain empty.
    Otherwise exception content or names of missed items (schemas) are printed.
NOTES:
    During test implementation a new bug was found related to truncated lines
    in the DB statistics caused by names which require 3 or 4 bytes per character,
    see: https://github.com/FirebirdSQL/firebird/issues/8789
    Such ('cuted') lines can not be properly handled and eceptions raise for them
    (BufferError, UnicodeDecodeError).

    [24.10.2025] pzotov
    Checked on 6.0.0.1313-8ee208b.
"""
import traceback
import locale
from pathlib import Path
from firebird.driver import SrvStatFlag

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

NAMES_MAP= {
    'SCHEMA' : [-1, -1, -1],
    'system' : [-1, -1, -1],
    'public' : [-1, -1, -1],

    # 2 bytes per character:
    'инструменты' : [-1, -1, -1],
    'nástroje' : [-1, -1, -1],
    'narzędzia' : [-1, -1, -1],
    'værktøjer' : [-1, -1, -1],
    'työkaluja' : [-1, -1, -1],
    'verktøy' : [-1, -1, -1],
    'eszközöket' : [-1, -1, -1],
    'МетодЗейделяДляЛинейныхГиперболическихИТрансцендентныхУр' : [-1, -1, -1],
    'έαισορροπίαθαείναικάτωαπότομηδέαισορροπίαθαείναικάτωαπτέ' : [-1, -1, -1],

    # all following names caused test FAIL before #8789 was fixed:
    # 3 bytes per character:
    '€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€' : [-1, -1, -1],
    'ლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლლ' : [-1, -1, -1],
    # 4 bytes per character:
    '𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞𝄞' : [-1, -1, -1],
    '🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉🂉' : [-1, -1, -1],
}

@pytest.mark.intl
@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    stat_output = []
    with act.db.connect(charset = 'utf8') as con:
        
        for n in NAMES_MAP.keys():
           cur = con.cursor()
           con.execute_immediate(f'create schema "{n}"')
           con.commit()
           con.execute_immediate(f'create table "{n}"."{n}_tab"(id int primary key using index "{n}_pk")')
           con.commit()
           cur.execute(f"select rdb$relation_id from rdb$relations where rdb$relation_name = '{n}_tab'")
           NAMES_MAP[ n ] = [cur.fetchone()[0], -1, -1]

    with act.connect_server(encoding = 'utf8', encoding_errors = locale.getpreferredencoding()) as srv:
        srv.database.get_statistics(
            database=act.db.db_path
           ,flags=SrvStatFlag.DATA_PAGES | SrvStatFlag.IDX_PAGES
        )

        try:
            # This failed before #8789 was fixed if schema/table/index required 4 bytes per char for encoding:
            stat_output = [x.rstrip() for x in srv.readlines() if x.strip()]
        except Exception as e: # BufferError, UnicodeDecodeError
            print('Unexpected error occurs during attempt to get lines of DB statistics:')
            print(e.__class__) # unknown error handler name 'cp1251'
            print(e.__str__()) # <class 'LookupError'>
            for p in traceback.format_exc().split('\n'):
                print('...',p)
    
    for i,line in enumerate(stat_output):
        if line.startswith('"') or line.strip().startswith('Index'):
            for k,v in NAMES_MAP.items():
                if line.startswith('"' + k + '"."' + k + '_tab"'):
                    # NB: we have to find not only name of relation but also its ID:
                    if line.endswith( f' ({v[0]})' ):
                        v[1] = i
                if line.strip().startswith('Index "' + k + '"."' + k + '_pk"'):
                        #     Index "SCHEMA"."SCHEMA_pk" (0)
                        v[2] = i
                NAMES_MAP[ k ] = v

    missed_schemas = {k for k,v in NAMES_MAP.items() if min(v) <= 0}
    if missed_schemas:
        print(f'UNEXPECTED. Could not find prefixed names of tables and/or indices for following schemas:')
        print('\n'.join(missed_schemas))

    expected_stdout = f"""
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
