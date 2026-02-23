#coding:utf-8
"""
ID:          issue-7747
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7747
TITLE:       garbage collection in indexes and blobs is not performed in VIO_backout
DESCRIPTION:
NOTES:
    [14.09.2023] pzotov
    Confirmed problem on 5.0.0.1209, 4.0.4.2986, 3.0.12.33707
    Checked on 5.0.0.1211, 4.0.4.2988 (intermediate snapshots), SS/CS.

    [17.02.2024] pzotov
    Added call to sweep(): test sometimes can fail if background garbage collection does not complete
    its job after 'select * from t1_blob' and before get_statistics() on iter #2
    (detected several times on Linux).
    Checked again on Windows, builds 5.0.0.1209 and 5.0.0.1211 (confirmed problem and fix).
"""
import re

import pytest
from firebird.qa import *
from firebird.driver import TPB, SrvStatFlag
import time

init_sql = """
    create table t1_blob (id integer, str1 blob);
    create index t1_blob_idx1 on t1_blob (id);
    insert into t1_blob values (0, 'abc');
    commit;
"""
db = db_factory(init = init_sql)
act = python_act('db') # , substitutions = substitutions)

CHECK_SQL = 'select id from test order by id with lock skip locked'

expected_stdout = f"""
    Table statistics, iter #1:
    total_records 1
    total_versions 1
    blobs 2
    nodes 2

    Table statistics, iter #2:
    total_records 1
    total_versions 0
    blobs 1
    nodes 1
"""

#------------------------------

def parse_db_stat(act, stats):
    stat_data_map = {}
    allowed_patterns = (
        'Average record length: \\d+\\.\\d+, total records: \\d+'
       ,'Average version length: \\d+\\.\\d+, total versions: \\d+, max versions: \\d+'
       ,'Blobs: \\d+, total length: \\d+, blob pages: \\d+'
       ,'Root page: \\d+, depth: \\d+, leaf buckets: \\d+, nodes: \\d+'
    )
    allowed_patterns = [ re.compile(r, re.IGNORECASE) for r in  allowed_patterns]
    for r in stats:
        if act.match_any(r, allowed_patterns):
            if allowed_patterns[0].search(r):
                stat_data_map[ 'total_records' ] = int(r.split()[6].replace(',',''))
            if allowed_patterns[1].search(r):
                stat_data_map[ 'total_versions' ] = int(r.split()[6].replace(',',''))
            if allowed_patterns[2].search(r):
                stat_data_map[ 'blobs' ] = int(r.split()[1].replace(',',''))
            if allowed_patterns[3].search(r):
                stat_data_map[ 'nodes' ] = int(r.split()[9].replace(',',''))
                
    return stat_data_map

#------------------------------

@pytest.mark.version('>=3.0.12')
def test_1(act: Action, capsys):

    with act.db.connect() as con:
        custom_tpb = TPB(no_auto_undo = True)
        tx = con.transaction_manager(default_tpb=custom_tpb.get_buffer())
        tx.begin()
        cur = tx.cursor()
        cur.execute("update t1_blob set id = 1, str1 = '123' where id = 0")
        tx.rollback()

    with act.connect_server() as srv:
        srv.database.get_statistics(database=act.db.db_path, flags=SrvStatFlag.RECORD_VERSIONS)
        stats = srv.readlines()

    # Average record  length: 16.00,  total  records:   1
    # Average version length: 9.00,   total  versions:  1,       max  versions:   1
    # Blobs:    2,     total  length:   6,     blob     pages:     0
    # Root    page:     227,  depth:    1,     leaf     buckets:   1,   nodes:    2
    # -----------------------------------------------------------------------------
    #  0        1        2      3       4       5       6          7       8      9


    stat_data_map = parse_db_stat(act, stats)
    print('Table statistics, iter #1:')
    for k,v in stat_data_map.items():
        print(k,v)


    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute('select * from t1_blob')
        for r in cur:
            pass

    with act.connect_server() as srv:
        srv.database.sweep(database=act.db.db_path) # <<< 17.02.2024. Force GC.
        srv.database.get_statistics(database=act.db.db_path, flags=SrvStatFlag.RECORD_VERSIONS)
        stats = srv.readlines()

    print('Table statistics, iter #2:')
    stat_data_map = parse_db_stat(act, stats)
    for k,v in stat_data_map.items():
        print(k,v)
    
    
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
