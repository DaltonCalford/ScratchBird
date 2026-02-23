#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/893488bb9d592b37b8c79e2197d1db9f831d15b5
TITLE:       (Simplified but still) cost-based choice between LOOP and HASH semi-joins
DESCRIPTION:
    As explained by dimitr (letter 15.01.2026 09:12), this improvement relates to the case when semi-join
    is transformed to either HASH JOIN or NESTED LOOPS but the type of join depends on result of multiplication
    <OUTER_SOURCE_FILTERED_COUNT> * <INNER_SOURCE_SELECTIVITY> being compared with 1.
    If this value GREATER than 1 then query is converted to HJ, otherwise to NL.
    We create two tables with fixed (not random!) data so that:
        * outer table ('customer') contains 15 rows and 15 unique cities;
        * inner table ('sales') has FK to the customer (using 'cust_no' field) and 14 unique cust_no values
        * selectivity of index sales.cust_no is 0.07142857
    Then we obtain explained plan for two queries:
        * Q-1: select ... from customer where exists(select ... from sales <using cust_no>)
          -- i.e. WITHOUT any filtering of customer; this leads to HASH JOIN because 15 * 0.07142857 = ~1.07 (>1);
        * Q-2: select ... from customer where <ADDITIONAL_FILTER> and exists(select ... from sales <using cust_no>)
          -- where <ADDITIONAL_FILTER> limits number of rows so that 15 multiplied on 0.07142857 will be <= 1.
NOTES:
    Confirmed missed transformation on 6.0.0.1389 (explained plan for Q-2 remained HJ).
    Confirmed improvement on 6.0.0.1393.
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_sql = f"""
    recreate table customer(cust_no int not null, city varchar(30) not null);
    recreate table sales(cust_no int not null);

    insert into customer (cust_no, city) values (1001, 'san diego');
    insert into customer (cust_no, city) values (1002, 'dallas');
    insert into customer (cust_no, city) values (1003, 'boston');
    insert into customer (cust_no, city) values (1004, 'manchester');
    insert into customer (cust_no, city) values (1005, 'central hong kong');
    insert into customer (cust_no, city) values (1006, 'ottawa');
    insert into customer (cust_no, city) values (1007, 'pebble beach');
    insert into customer (cust_no, city) values (1008, 'lihue');
    insert into customer (cust_no, city) values (1009, 'turtle island');
    insert into customer (cust_no, city) values (1010, 'tokyo');
    insert into customer (cust_no, city) values (1011, 'zurich');
    insert into customer (cust_no, city) values (1012, 'paris');
    insert into customer (cust_no, city) values (1013, 'milan');
    insert into customer (cust_no, city) values (1014, 'brussels');
    insert into customer (cust_no, city) values (1015, 'den haag');
    commit;

    insert into sales (cust_no) values (1004);
    insert into sales (cust_no) values (1004);
    insert into sales (cust_no) values (1012);
    insert into sales (cust_no) values (1010);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1002);
    insert into sales (cust_no) values (1002);
    insert into sales (cust_no) values (1002);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1014);
    insert into sales (cust_no) values (1006);
    insert into sales (cust_no) values (1006);
    insert into sales (cust_no) values (1009);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1005);
    insert into sales (cust_no) values (1008);
    insert into sales (cust_no) values (1008);
    insert into sales (cust_no) values (1013);
    insert into sales (cust_no) values (1010);
    insert into sales (cust_no) values (1010);
    insert into sales (cust_no) values (1015);
    insert into sales (cust_no) values (1011);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1001);
    insert into sales (cust_no) values (1007);
    insert into sales (cust_no) values (1005);
    insert into sales (cust_no) values (1011);
    commit;

    alter table customer add constraint customer_pk primary key(cust_no);
    create index cust_city on customer(city);
    alter table sales add constraint sales_cust_fk foreign key(cust_no) references customer;
    commit;

    /******************
    -- leave for debug only:
    set list on;
    select
         iif(cust_count_all * sales_cust_idx_stat > 1, 'HJ', 'NL') as plan_case_1
        ,iif(cust_count_city * sales_cust_idx_stat > 1, 'HJ', 'NL') as plan_case_2
    from (
        select
             (select count(*) as cust_count_all from customer)
            ,(select count(*) as cust_count_city from customer x where x.city in ('central hong kong','dallas','den haag','lihue','manchester','milan','ottawa','paris','pebble beach','san diego','tokyo','turtle island','zurich') )
            ,cast((select ri.rdb$statistics from rdb$indices ri where ri.rdb$index_name = 'SALES_CUST_FK') as numeric(10,8)) as sales_cust_idx_stat
        from rdb$database
    ) t;
    *******************/
"""

db = db_factory(init = init_sql)

substitutions = []

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = python_act('db', substitutions = substitutions)

#---------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#---------------------------------------------------------

query_map = {
    1000 : (
              """
                select 1
                from customer c
                where
                    exists (
                        select  s.cust_no
                        from sales s
                        where s.cust_no = c.cust_no
                    )
              """
             ,'Check that plan uses HASH JOIN'
           )
   ,2000 : (
              """
                select 2
                from customer c
                where
                    c.city in ('central hong kong','dallas','den haag','lihue','manchester','milan','ottawa','paris','pebble beach','san diego','tokyo','turtle island','zurich') and
                    exists (
                        select  s.cust_no
                        from sales s
                        where s.cust_no = c.cust_no 
                    )
              """
             ,'Check that plan uses NESTED LOOPS'
           )
}

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    with act.db.connect() as con:
        cur = con.cursor()
        for q_idx, q_tuple in query_map.items():
            test_sql, qry_comment = q_tuple[:2]
            ps = None
            try:
                ps = cur.prepare(test_sql)
                print(q_idx)
                print(test_sql)
                print(qry_comment)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()
   
    expected_stdout = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi) (keys: 1, total key length: 4)
        ............-> Table CUSTOMER as C Full Scan
        ............-> Record Buffer (record length: 25)
        ................-> Table SALES as S Full Scan

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Select Expression
        ....-> Nested Loop Join (semi)
        ........-> Filter
        ............-> Table CUSTOMER as C Access By ID
        ................-> Bitmap
        ....................-> Index CUST_CITY List Scan (full match)
        ........-> Filter
        ............-> Table SALES as S Access By ID
        ................-> Bitmap
        ....................-> Index SALES_CUST_FK Range Scan (full match)
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
