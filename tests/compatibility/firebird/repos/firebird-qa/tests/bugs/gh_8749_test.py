#coding:utf-8

"""
ID:          issue-8749
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8749
TITLE:       Computed index on RDB$RECORD_VERSION don't work as expected
DESCRIPTION:
    Test uses DDL and data from files/standard_sample_databases.zip (file: "sample-DB_-_firebird.sql").
NOTES:
    [19.02.2026] pzotov
    It is supposed that tables EMPLOYEE and PROJECT are filled within same transaction.
    Two additional indices are created here for tables EMPLOYEE and PROJECT, both on RDB$RECORD_VERSION field.
    We query both tables in order to obtain one (arbitrary) record with value of RDB$RECORD_VERSION
    that is same for both EMPLOYEE and PROJECT -- see variables 'EMP_RECVERS_SAMPLE' and 'PRJ_RECVERS_SAMPLE'

    Confirmed problem on 6.0.0.1458-0-6a76c1d, 5.0.4.1762: no rows when index on rdb$record_version is used (all FOUND_DATA_* = 0)
    Checked on 6.0.0.1458-c3778e0; 5.0.4.1767-52823f5.
"""
import time
import zipfile
from pathlib import Path
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

tmp_init_sql = temp_file('gh_8749.tmp.sql')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.4')
def test_1(act: Action, tmp_init_sql: Path, capsys):
    employee_data_sql = zipfile.Path(act.files_dir / 'standard_sample_databases.zip', at='sample-DB_-_firebird.sql')
    tmp_init_sql.write_bytes(employee_data_sql.read_bytes())

    act.isql(switches = ['-q'], charset='utf8', input_file = tmp_init_sql, combine_output = True)
    
    if act.return_code == 0:
        pass
    else:
        # If retcode !=0 then we can print the whole output of failed gbak:
        print('Initial script failed, check output:')
        for line in act.clean_stdout.splitlines():
            print(line)
    act.reset()

    with act.db.connect() as con:
        con.execute_immediate('create index emp_rdb_rec_ver on employee computed by ( rdb$record_version )')
        con.execute_immediate('create index prj_rdb_rec_ver on project computed by ( rdb$record_version )')
        con.commit()
        cur = con.cursor()

        # ::: NB :::
        # It is supposed that tables EMPLOYEE and PROJECT are filled within same transaction.
        # In case of assert one need to ensure that all DML statements against these tables 
        # belong to the same Tx. Check files/standard_sample_databases.zip (script: "sample-DB_-_firebird.sql").
        #
        cur.execute('select first 1 rdb$record_version from employee')
        EMP_RECVERS_SAMPLE = cur.fetchone()[0]
        cur.execute('select first 1 rdb$record_version from project p where exists(select 1 from employee e where e.emp_no = p.team_leader and e.rdb$record_version  = ?)', (EMP_RECVERS_SAMPLE,))
        PRJ_RECVERS_SAMPLE = cur.fetchone()[0]

        assert PRJ_RECVERS_SAMPLE, f'At least one record  must exist in PROJECT with team_leader = employee.emp_no for employee.rdb$record_version = {EMP_RECVERS_SAMPLE}'


    query_map = {
        1000 : (
                  f"""
                    select /* trace_me */ sign(count(*)) as found_data_1000 from employee e where e.rdb$record_version = {EMP_RECVERS_SAMPLE}
                  """
                 ,''
               )
        ,
        2000 : (
                  f"""
                    select /* trace_me */ sign(count(*)) as found_data_2000 from project p where p.rdb$record_version = {PRJ_RECVERS_SAMPLE}
                  """
                 ,''
               )
        ,
        3000 : (
                  f"""
                    select /* trace_me */ sign(count(*)) as found_data_3000
                    from employee e1
                    join project p1 on e1.emp_no  + 0 = p1.team_leader
                    where e1.rdb$record_version = {EMP_RECVERS_SAMPLE} and p1.rdb$record_version = {PRJ_RECVERS_SAMPLE}
                  """
                 ,''
               )
    }


    with act.db.connect() as con:
        cur = con.cursor()
        for q_idx, q_tuple in query_map.items():
            test_sql, qry_comment = q_tuple[:2]
            ps, rs = None, None
            try:
                print(q_idx)
                print(test_sql)
                print(qry_comment)
                ps = cur.prepare(test_sql)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                rs = cur.execute(ps)
                cur_cols = cur.description
                for r in cur:
                    for i in range(0,len(cur_cols)):
                        print( cur_cols[i][0], ':', r[i] )
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close()
                if ps:
                    ps.free()

    expected_stdout_5x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "EMPLOYEE" as "E" Access By ID
        ................-> Bitmap
        ....................-> Index "EMP_RDB_REC_VER" Range Scan (full match)
        FOUND_DATA_1000 : 1

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "PROJECT" as "P" Access By ID
        ................-> Bitmap
        ....................-> Index "PRJ_RDB_REC_VER" Range Scan (full match)
        FOUND_DATA_2000 : 1

        3000
        {query_map[3000][0]}
        {query_map[3000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Hash Join (inner)
        ................-> Filter
        ....................-> Table "EMPLOYEE" as "E1" Access By ID
        ........................-> Bitmap
        ............................-> Index "EMP_RDB_REC_VER" Range Scan (full match)
        ................-> Record Buffer (record length: 25)
        ....................-> Filter
        ........................-> Table "PROJECT" as "P1" Access By ID
        ............................-> Bitmap
        ................................-> Index "PRJ_RDB_REC_VER" Range Scan (full match)
        FOUND_DATA_3000 : 1
    """

    expected_stdout_6x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "PUBLIC"."EMPLOYEE" as "E" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."EMP_RDB_REC_VER" Range Scan (full match)
        FOUND_DATA_1000 : 1

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "PUBLIC"."PROJECT" as "P" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."PRJ_RDB_REC_VER" Range Scan (full match)
        FOUND_DATA_2000 : 1

        3000
        {query_map[3000][0]}
        {query_map[3000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Hash Join (inner) (keys: 1, total key length: 8)
        ................-> Filter
        ....................-> Table "PUBLIC"."EMPLOYEE" as "E1" Access By ID
        ........................-> Bitmap
        ............................-> Index "PUBLIC"."EMP_RDB_REC_VER" Range Scan (full match)
        ................-> Record Buffer (record length: 25)
        ....................-> Filter
        ........................-> Table "PUBLIC"."PROJECT" as "P1" Access By ID
        ............................-> Bitmap
        ................................-> Index "PUBLIC"."PRJ_RDB_REC_VER" Range Scan (full match)
        FOUND_DATA_3000 : 1
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
