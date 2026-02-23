#coding:utf-8

"""
ID:          issue-8061
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8061
TITLE:       Unnest IN/ANY/EXISTS subqueries and optimize them using semi-join algorithm
DESCRIPTION:
    Test uses DDL and data from employee DB but they have been extracted to .sql and stored in 
    files/standard_sample_databases.zip (file: "sample-DB_-_firebird.sql").
    Default names of all constraints were replaced in order to easy find appropriate table.

    Some examples for this test were taken from:
    https://blogs.oracle.com/optimizer/post/optimizer-transformations-subquery-unnesting-part-1
NOTES:
        1. One need to change config parameter SubQueryConversion to 'true' when check FB 5.x.
        2. Explained plan in FB 5.x has no details about keys and total key length, so we have to apply
           substitution in order to ignore these data when make comparison with expected output.
        3. Commits:
           6.x:
               22.03.2025 10:47
               https://github.com/FirebirdSQL/firebird/commit/fc12c0ef392fec9c83d41bc17da3dc233491498c
               (Unnest IN/ANY/EXISTS subqueries and optimize them using semi-join algorithm (#8061))
           5.x
               31.07.2024 09:46
               https://github.com/FirebirdSQL/firebird/commit/4943b3faece209caa93cc9573803677019582f1c
               (Added support for semi/anti and outer joins to hash join algorithm ...)
               Also:
               14.09.2024 09:24
               https://github.com/FirebirdSQL/firebird/commit/5fa4ae611d18fd4ce9aac1c8dbc79e5fea2bc1f2
               (Fix bug #8252: Incorrect subquery unnesting with complex dependencies)

        4. Following tests also relate to unnesting but they check only FB 5.x (and not FB 6.x):
               bugs/gh_8265_test.py; // additional examples related to ability of subquery unnesting;
               bugs/gh_8252_test.py; // example when unnesting must NOT be performed;
               bugs/gh_8233_test.py;
               bugs/gh_8231_test.py;
               bugs/gh_8225_test.py;
               bugs/gh_8223_test.py;
           All these tests will be reimplemented soon in order to check FB 6.x also.
        
        Confirmed old execution plan in 6.0.0.680 (19.03.2025), it had no 'hash join (semi)' in any explanied plan.
        Checked on 6.0.0.687-730aa8f (22-mar-2025),  5.0.1.1464-d1033cc (01-aug-2024).
    [06.07.2025] pzotov
        Script 'sample-DB_-_firebird.sql' in filed/standard_sample_databases.zip has been adjusted
        for applying in FB 6.x: 'ALTER CHARACTER SET ... SET DEFAULT COLLATION <CUSTOM_COLLATION>'
        requires explicitly specified `PUBLIC.` prefix. Execute block with if/else is used now there.

        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.914; 5.0.3.1668.

    [16.01.2026] pzotov
        Script 'sample-DB_-_firebird.sql' in filed/standard_sample_databases.zip has been supplemented with
        index statistics re-calculation, including PK/FK indices. This is needed to get correct execution plan
        for query marked as 'Check unnesting of two nested EXISTS' (explained by dimitr, 15.01.2026 09:12).
        If value of select_count_from_<outer> * index_selectivity_for_<inner> greater than 1 then HJ is used.
        Otherwise Nested Loops are in use.
        Execution plan for this query has changed in 6.0.0.1394-c8d9d10d (2026.01.14) after
        commit #893488b "(Simplified but still) cost-based choice between LOOP and HASH semi-joins".
"""

import pytest
import zipfile
from pathlib import Path
from firebird.qa import *
from firebird.driver import driver_config, connect

db = db_factory()
# Hash Join (semi) (keys: 1, total key length: 4)
substitutions = [(r'Hash Join \(semi\) \(keys: \d+, total key length: \d+\)', 'Hash Join (semi)'), (r'record length: \d+', 'record length: NN')]

act = python_act('db', substitutions = substitutions)

tmp_sql = temp_file('gh_8061.tmp.sql')

query_map = {
    1000 : (
              """
                  select c1.cust_no
                  from customer c1
                  where exists (
                      select 1 from sales s1 where s1.cust_no = c1.cust_no and s1.qty_ordered > 10
                  )
              """
             ,'Check unnesting of single EXISTS'
           )
   ,2000 : (
              """
                  select c2.cust_no
                  from customer c2
                  where c2.cust_no = any (
                      select s2.cust_no
                      from sales s2
                      where s2.qty_ordered > 10
                  )
              """
             ,'Check unnesting of ANY'
           )
   ,3000 : (
              """
                  select c3.cust_no
                  from customer c3
                  where exists (
                      select  s3.cust_no
                      from sales s3
                      where s3.cust_no = c3.cust_no and
                      exists (
                          select x.emp_no
                          from employee x
                          where
                              x.emp_no = s3.sales_rep
                              and (
                                  x.dept_no > 0
                                  or
                                  x.job_code > ''
                              )
                      )
                  )
              """
             ,'Check unnesting of two nested EXISTS'
           )
   ,4000 : (
              """
                  select c4.cust_no
                  from customer c4
                  where c4.cust_no in
                  (
                      select s4.cust_no
                      from sales s4
                      where
                          s4.paid > ''
                          or
                          s4.sales_rep in (
                              select x.emp_no
                              from employee x
                              where
                                  x.dept_no > 0
                                  or
                                  x.job_code > ''
                          )
                  )
              """
             ,'Check unnesting of IN (NOTE: inner sub-query cannot be unnested due to OR condition present, but the outer sub-query *can*; see also bugs/gh_8265_test.py)'
           )
}

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.1')
def test_1(act: Action, tmp_sql: Path, capsys):
    employee_data_sql = zipfile.Path(act.files_dir / 'standard_sample_databases.zip', at='sample-DB_-_firebird.sql')
    tmp_sql.write_bytes(employee_data_sql.read_bytes())

    act.isql(switches = ['-q'], charset='utf8', input_file = tmp_sql, combine_output = True)

    if act.return_code == 0:

        srv_cfg = driver_config.register_server(name = f'srv_cfg_8061', config = '')
        db_cfg_name = f'db_cfg_8061'
        db_cfg_object = driver_config.register_database(name = db_cfg_name)
        db_cfg_object.server.value = srv_cfg.name
        db_cfg_object.database.value = str(act.db.db_path)
        if act.is_version('<6'):
            db_cfg_object.config.value = f"""
                SubQueryConversion = true
            """

        with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
            cur = con.cursor()
            for q_idx, q_tuple in query_map.items():
                test_sql, qry_comment = q_tuple[:2]
                ps = cur.prepare(test_sql)
                print(q_idx)
                print(test_sql)
                print(qry_comment)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                ps.free()

    else:
        # If retcode !=0 then we can print the whole output of failed gbak:
        print('Initial script failed, check output:')
        for line in act.clean_stdout.splitlines():
            print(line)
    act.reset()

    expected_stdout_5x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi) (keys: 1, total key length: 4)
        ............-> Table "CUSTOMER" as "C1" Full Scan
        ............-> Record Buffer (record length: 33)
        ................-> Filter
        ....................-> Table "SALES" as "S1" Full Scan

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi) (keys: 1, total key length: 4)
        ............-> Table "CUSTOMER" as "C2" Full Scan
        ............-> Record Buffer (record length: 33)
        ................-> Filter
        ....................-> Table "SALES" as "S2" Full Scan

        3000
        {query_map[3000][0]}
        {query_map[3000][1]}
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi) (keys: 1, total key length: 4)
        ............-> Table "CUSTOMER" as "C3" Full Scan
        ............-> Record Buffer (record length: 58)
        ................-> Filter
        ....................-> Hash Join (semi) (keys: 1, total key length: 2)
        ........................-> Table "SALES" as "S3" Full Scan
        ........................-> Record Buffer (record length: 41)
        ............................-> Filter
        ................................-> Table "EMPLOYEE" as "X" Full Scan

        4000
        {query_map[4000][0]}
        {query_map[4000][1]}
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "EMPLOYEE" as "X" Access By ID
        ................-> Bitmap
        ....................-> Index "EMPLOYEE_PK" Unique Scan
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi) (keys: 1, total key length: 4)
        ............-> Table "CUSTOMER" as "C4" Full Scan
        ............-> Record Buffer (record length: 33)
        ................-> Filter
        ....................-> Table "SALES" as "S4" Full Scan
    """

    expected_stdout_6x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi)
        ............-> Table "PUBLIC"."CUSTOMER" as "C1" Full Scan
        ............-> Record Buffer (record length: NN)
        ................-> Filter
        ....................-> Table "PUBLIC"."SALES" as "S1" Full Scan

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi)
        ............-> Table "PUBLIC"."CUSTOMER" as "C2" Full Scan
        ............-> Record Buffer (record length: NN)
        ................-> Filter
        ....................-> Table "PUBLIC"."SALES" as "S2" Full Scan

        3000
        {query_map[3000][0]}
        {query_map[3000][1]}
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi)
        ............-> Table "PUBLIC"."CUSTOMER" as "C3" Full Scan
        ............-> Record Buffer (record length: NN)
        ................-> Nested Loop Join (semi)
        ....................-> Table "PUBLIC"."SALES" as "S3" Full Scan
        ....................-> Filter
        ........................-> Table "PUBLIC"."EMPLOYEE" as "X" Access By ID
        ............................-> Bitmap
        ................................-> Index "PUBLIC"."EMPLOYEE_PK" Unique Scan

        4000
        {query_map[4000][0]}
        {query_map[4000][1]}
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "PUBLIC"."EMPLOYEE" as "X" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."EMPLOYEE_PK" Unique Scan
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi)
        ............-> Table "PUBLIC"."CUSTOMER" as "C4" Full Scan
        ............-> Record Buffer (record length: NN)
        ................-> Filter
        ....................-> Table "PUBLIC"."SALES" as "S4" Full Scan
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
