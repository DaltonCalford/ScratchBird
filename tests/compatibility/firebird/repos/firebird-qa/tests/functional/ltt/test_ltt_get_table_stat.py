#coding:utf-8

"""
ID:          n/a
TITLE:       LOCAL TEMPORARY TABLE - get table access statistics using API
DESCRIPTION:
    Test creates LTT with all known datatypes (except blobs), create indices for every column and adds {ROWS_TO_ADD}
    rows into it (all columns have NULLs in appropriate INSERT statement).
    Then it runs two EXECUTE BLOCKs which query just created LTT (using ES mechanism): first ES must involve all indices
    (its query refers to each of indexed column) while second must cause natural reads.
    Before doing these actions, we query mon$local_temporary_tables in order to get mon$table_id for created LTT.
    For each ES, we obtain data from con.info.get_table_access_stats() related to currently checked LTT, and store two
    fields from it: '.sequential' and '.indexed'.
    Difference between new and old '.indexed' and '.sequential' values must be equal to the number of added rows in 
    execute blocks NN 1 and 2 respectively (see 'qry_map').
NOTES:
    [06.02.2026] pzotov
    Checked on 6.0.0.1405-761a49d.
"""
import locale
from pathlib import Path
import time

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError


db = db_factory(charset = 'utf8')
act = python_act('db')

ROWS_TO_ADD = 200

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    types_lst = [
        'smallint'
        ,'int'
        ,'bigint'
        ,'int128'
        ,'float'
        ,'real'
        ,'double precision'
        ,'long float'
        ,'numeric(3,2)'
        ,'decimal(3,2)'
        ,'numeric(9,2)'
        ,'decimal(9,2)'
        ,'numeric(18,2)'
        ,'decimal(18,2)'
        ,'decfloat(16)'
        ,'decfloat(34)'
        ,'date'
        ,'time'
        ,'timestamp'
        ,'time with time zone'
        ,'timestamp with time zone'
        ,'boolean'
        ,'char(1)'
        ,'nchar(1)'
        ,'varchar(1)'
    ]    

    ddl_list = [ 'create local temporary table ltt_test (', ]

    for i,ftype in enumerate(types_lst):
        f_suffix = '_'.join( ftype.split(' ')[:4] )
        ddl_list.append( (',' if i > 0 else '') + f'"fld_{i:02d}_{f_suffix}" {ftype}' )
    
    ddl_list.append(') on commit preserve rows')
    ddl_list.append('^')

    for i,ftype in enumerate(types_lst):
        if ftype.startswith('blob '):
            pass
        else:
            f_suffix = '_'.join( ftype.split(' ')[:4] )
            ddl_list.append(f'create index "ltt_idx_{i:02d}_{f_suffix}" on ltt_test ("fld_{i:02d}_{f_suffix}")')
            ddl_list.append('^')

    ddl_text = '\n'.join(ddl_list)

    dml_list = []
    dml_list.append('select count(*) from ltt_test where')
    for i,ftype in enumerate(types_lst):
        if ftype.startswith('blob '):
            pass
        else:
            f_suffix = '_'.join( ftype.split(' ')[:4] )
            dml_list.append(  f'"fld_{i:02d}_{f_suffix}" is null and' )

    dml_list[-1] = ' '.join(dml_list[-1].split(' ')[:-1]) # remove last 'and'
    dml_list = '\n'.join(dml_list)

    dml_list_1000 = f"""
        execute block as
            declare c int;
        begin
            execute statement q'#\n{dml_list}\n#' into c;
        end
    """

    dml_list_2000 = f"""
        execute block as
            declare c int;
        begin
            execute statement q'#\nselect count(*) from ltt_test\n#' into c;
        end
    """

    qry_map = {
        'CHK_IDX' : dml_list_1000
       ,'CHK_NAT' : dml_list_2000
    }

    with act.db.connect() as con:
        cur = con.cursor()

        for x in ddl_text.split('^'):
            if (s := x.strip()):
                if s.lower() == 'commit':
                    con.commit()
                else:
                    con.execute_immediate(s)
        con.commit()
        con.execute_immediate('insert into ltt_test select ' + ','.join( (['null'] * len(types_lst)) ) + f' from rdb$types rows {ROWS_TO_ADD}' )
        con.commit()
        
        cur.execute("select t.mon$table_id from mon$local_temporary_tables t where t.mon$table_name = upper('ltt_test')")
        ltt_rel_id = cur.fetchone()[0]

        nat_reads = {}
        idx_reads = {}
        for k, v in qry_map.items():
            for x_table in con.info.get_table_access_stats():
                if x_table.table_id == ltt_rel_id:
                    nat_reads[k] = -x_table.sequential if x_table.sequential is not None else 0
                    idx_reads[k] = -x_table.indexed if x_table.indexed is not None else 0

            cur.execute(v)

            for x_table in con.info.get_table_access_stats():
                if x_table.table_id == ltt_rel_id:
                    nat_reads[k] += x_table.sequential if x_table.sequential is not None else 0
                    idx_reads[k] += x_table.indexed if x_table.indexed is not None else 0

            print(k, nat_reads[k], idx_reads[k])

    expected_stdout = f"""
        CHK_IDX 0 {ROWS_TO_ADD}
        CHK_NAT {ROWS_TO_ADD} 0
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
