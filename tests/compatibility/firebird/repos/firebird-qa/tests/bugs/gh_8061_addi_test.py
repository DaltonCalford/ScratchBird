#coding:utf-8

"""
ID:          issue-8061
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8061
TITLE:       UNNEST subqueries invalidation. Examples when unnesting can NOT be used.
DESCRIPTION:
    Test uses DDL and data from employee DB but they have been extracted to .sql and stored in 
    files/standard_sample_databases.zip (file: "sample-DB_-_firebird.sql").
    Default names of all constraints were replaced in order to easy find appropriate table.

    Examples for this test based on 
    1) https://blogs.oracle.com/optimizer/post/optimizer-transformations-subquery-unesting-part-2
       (paragraph "Validity of Unnesting")
    2) https://jonathanlewis.wordpress.com/2007/02/26/subquery-with-or/
NOTES:
        1. One need to change config parameter SubQueryConversion to 'true' when check FB 5.x.
        2. Commits:
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
        
        Checked on 6.0.0.735,  5.0.3.1647
    [06.07.2025] pzotov
        Script 'sample-DB_-_firebird.sql' in filed/standard_sample_databases.zip has been adjusted
        for applying in FB 6.x: 'ALTER CHARACTER SET ... SET DEFAULT COLLATION <CUSTOM_COLLATION>'
        requires explicitly specified `PUBLIC.` prefix. Execute block with if/else is used now there.

        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.914; 5.0.3.1668.
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
                              x.job_country = c3.country
                      )
                  )
              """
             ,"""
                  Subqueries that are correlated to non-parent; for example,
                  subquery SQ3 is contained by SQ2 (parent of SQ3) and SQ2 in turn is contained
                  by SQ1 and SQ3 is correlated to tables defined in SQ1.
              """
           )
   ,2000 : (
              """
                  select c3.cust_no
                  from customer c3
                  where exists (
                      select s3.cust_no
                      from sales s3
                      where s3.cust_no = c3.cust_no
                      group by s3.cust_no
                  )
              """
             ,"""
                 A group-by subquery is correlated; in this case, unnesting implies doing join
                 after group-by. Changing the given order of the two operations may not be always legal.
              """
           )
   ,3000 : (
              """
                  select s1.cust_no
                  from sales s1
                  where exists (
                      select 1 from customer c1 where s1.cust_no = c1.cust_no
                      union all
                      select 1 from employee x1 where s1.sales_rep = x1.emp_no
                  )
              """
             ,"""
                 For disjunctive subqueries, the outer columns in the connecting
                 or correlating conditions are not the same.
              """
           )
   ,4000 : (
              """
                 select x1.emp_no
                 from employee x1
                 where
                 (
                     x1.job_country = 'USA' or
                     exists (
                         select 1
                         from sales s1
                         where s1.sales_rep = x1.emp_no
                     )
                 )
              """
             ,'An `OR` condition in compound WHERE expression, see https://jonathanlewis.wordpress.com/2007/02/26/subquery-with-or/'
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

        srv_cfg = driver_config.register_server(name = f'srv_cfg_8061_addi', config = '')
        db_cfg_name = f'db_cfg_8061_addi'
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
        Sub-query
        ....-> Filter
        ........-> Table "EMPLOYEE" as "X" Full Scan
        Sub-query
        ....-> Filter (preliminary)
        ........-> Filter
        ............-> Table "SALES" as "S3" Access By ID
        ................-> Bitmap
        ....................-> Index "SALES_CUSTOMER_FK_CUST_NO" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Table "CUSTOMER" as "C3" Full Scan

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Sub-query
        ....-> Aggregate
        ........-> Filter
        ............-> Table "SALES" as "S3" Access By ID
        ................-> Index "SALES_CUSTOMER_FK_CUST_NO" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Table "CUSTOMER" as "C3" Full Scan

        3000
        {query_map[3000][0]}
        {query_map[3000][1]}
        Sub-query
        ....-> Union
        ........-> Filter
        ............-> Table "CUSTOMER" as "C1" Access By ID
        ................-> Bitmap
        ....................-> Index "CUSTOMER_PK" Unique Scan
        ........-> Filter
        ............-> Table "EMPLOYEE" as "X1" Access By ID
        ................-> Bitmap
        ....................-> Index "EMPLOYEE_PK" Unique Scan
        Select Expression
        ....-> Filter
        ........-> Table "SALES" as "S1" Full Scan

        4000
        {query_map[4000][0]}
        {query_map[4000][1]}
        Sub-query
        ....-> Filter
        ........-> Table "SALES" as "S1" Access By ID
        ............-> Bitmap
        ................-> Index "SALES_EMPLOYEE_FK_SALES_REP" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Table "EMPLOYEE" as "X1" Full Scan
    """

    expected_stdout_6x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Sub-query
        ....-> Filter
        ........-> Table "PUBLIC"."EMPLOYEE" as "X" Full Scan
        Sub-query
        ....-> Filter (preliminary)
        ........-> Filter
        ............-> Table "PUBLIC"."SALES" as "S3" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."SALES_CUSTOMER_FK_CUST_NO" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."CUSTOMER" as "C3" Full Scan

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Sub-query
        ....-> Aggregate
        ........-> Filter
        ............-> Table "PUBLIC"."SALES" as "S3" Access By ID
        ................-> Index "PUBLIC"."SALES_CUSTOMER_FK_CUST_NO" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."CUSTOMER" as "C3" Full Scan

        3000
        {query_map[3000][0]}
        {query_map[3000][1]}
        Sub-query
        ....-> Union
        ........-> Filter
        ............-> Table "PUBLIC"."CUSTOMER" as "C1" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."CUSTOMER_PK" Unique Scan
        ........-> Filter
        ............-> Table "PUBLIC"."EMPLOYEE" as "X1" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."EMPLOYEE_PK" Unique Scan
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."SALES" as "S1" Full Scan

        4000
        {query_map[4000][0]}
        {query_map[4000][1]}
        Sub-query
        ....-> Filter
        ........-> Table "PUBLIC"."SALES" as "S1" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."SALES_EMPLOYEE_FK_SALES_REP" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."EMPLOYEE" as "X1" Full Scan
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
