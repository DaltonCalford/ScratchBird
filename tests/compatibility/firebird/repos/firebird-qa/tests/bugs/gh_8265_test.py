#coding:utf-8

"""
ID:          issue-8265
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8265
TITLE:       Nested IN/EXISTS subqueries should not be converted into semi-joins if the outer context is a sub-query which wasn't unnested
DESCRIPTION:
NOTES:
    [26.09.2024] pzotov
        0. Commits:
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
        1. Parameter 'SubQueryConversion' currently presents only in FB 5.x and _NOT_ in FB 6.x.
        2. Custom driver config objects are created here, one with SubQueryConversion = true and second with false.
        3. First example of this test is also used in tests/functional/tabloid/test_aae2ae32.py
           Confirmed problem on 5.0.2.1516-fe6ba50 (23.09.2024). Checked on 5.0.2.1516-92316F0 (25.09.2024).
    [16.04.2025] pzotov
        Re-implemented in order to check FB 5.x with set 'SubQueryConversion = true' and FB 6.x w/o any changes in its config.
        Checked on 6.0.0.687-730aa8f, 5.0.3.1647-8993a57
    [06.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.914; 5.0.3.1668.
"""

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, DatabaseError

init_script = """
    create table test1(id int not null);
    create table test2(id int not null, pid int not null);
    create table test3(id int not null, pid int not null, name varchar(30) not null);
    commit;

    insert into test1(id) select row_number()over()-1 from rdb$types rows 10;
    insert into test2(id, pid) select row_number()over()-1, mod(row_number()over()-1, 10) from rdb$types rows 100;
    insert into test3(id, pid, name) select row_number()over()-1, mod(row_number()over()-1, 100), 'QWEABCRTY' from rdb$types, rdb$types rows 1000;
    commit;
"""

db = db_factory(init=init_script)

# Hash Join (semi) (keys: 1, total key length: 4)
substitutions = [(r'Hash Join \(semi\) \(keys: \d+, total key length: \d+\)', 'Hash Join (semi)'), (r'record length: \d+', 'record length: NN')]

act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

query_map = {
    1000 : (
                """
                   select count(*) from test1 q1_a
                   where
                       q1_a.id in (
                           select q1_b.pid from test2 q1_b
                           where
                               q1_b.id in (
                                   select q1_c.pid from test3 q1_c
                                   where q1_c.name like '%ABC%'
                               )
                       )
                """
               ,'Both sub-queries can (and should) be unnested.'
           )
   ,2000 : (
                """
                    select count(*) from test1 q2_a
                    where
                        q2_a.id in (
                            select q2_b.pid from test2 q2_b
                            where
                                1=1 or q2_b.id in (
                                    select q2_c.pid from test3 q2_c
                                    where q2_c.name like '%ABC%'
                                )
                        )
                """
               ,'Inner sub-query can NOT be unnested due to `OR` condition present, but the outer sub-query can'
           )
   ,3000 : (
                """
                    select count(*) from test1 q3_a
                    where
                        1=1 or q3_a.id in (
                            select q3_b.pid from test2 q3_b
                            where q3_b.id in (
                                select id from test3 q3_c
                                where q3_c.name like '%ABC%'
                            )
                        )
                """
               ,'Outer sub-query can NOT be unnested due to `OR` condition present, so the inner sub-query should NOT be unnested too'
           )
   ,4000 : (
                """
                    select count(*) from test1 q4_a
                    where
                        1=1 or q4_a.id in (
                            select id from test2 q4_b
                            where
                                1=1 or q4_b.id in (
                                    select id from test3 q4_c
                                    where q4_c.name like '%ABC%'
                                )
                            )
                """
               ,'Both sub-queries can NOT be unnested due to OR conditions present'
           )
}

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.2')
def test_1(act: Action, capsys):

    srv_cfg = driver_config.register_server(name = f'srv_cfg_8265', config = '')
    db_cfg_name = f'db_cfg_8265'
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
            ps,rs = None, None
            try:
                ps = cur.prepare(test_sql)
                print(q_idx)
                print(test_sql)
                print(qry_comment)

                # Print explained plan with padding eash line by dots in order to see indentations:
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                rs = cur.execute(ps)
                # Print data:
                for r in rs:
                    print(r[0])
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                # explained by hvlad, 26.10.24 17:42
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
        ............-> Hash Join (semi) (keys: 1, total key length: 4)
        ................-> Table "TEST1" as "Q1_A" Full Scan
        ................-> Record Buffer (record length: 82)
        ....................-> Filter
        ........................-> Hash Join (semi) (keys: 1, total key length: 4)
        ............................-> Table "TEST2" as "Q1_B" Full Scan
        ............................-> Record Buffer (record length: 57)
        ................................-> Filter
        ....................................-> Table "TEST3" as "Q1_C" Full Scan
        10

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST3" as "Q2_C" Full Scan
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Hash Join (semi) (keys: 1, total key length: 4)
        ................-> Table "TEST1" as "Q2_A" Full Scan
        ................-> Record Buffer (record length: 33)
        ....................-> Filter
        ........................-> Table "TEST2" as "Q2_B" Full Scan
        10

        3000
        {query_map[3000][0]}
        {query_map[3000][1]}
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST3" as "Q3_C" Full Scan
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST2" as "Q3_B" Full Scan
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "TEST1" as "Q3_A" Full Scan
        10

        4000
        {query_map[4000][0]}
        {query_map[4000][1]}
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST3" as "Q4_C" Full Scan
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "TEST2" as "Q4_B" Full Scan
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "TEST1" as "Q4_A" Full Scan
        10
    """

    expected_stdout_6x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Hash Join (semi)
        ................-> Table "PUBLIC"."TEST1" as "Q1_A" Full Scan
        ................-> Record Buffer (record length: NN)
        ....................-> Filter
        ........................-> Hash Join (semi)
        ............................-> Table "PUBLIC"."TEST2" as "Q1_B" Full Scan
        ............................-> Record Buffer (record length: NN)
        ................................-> Filter
        ....................................-> Table "PUBLIC"."TEST3" as "Q1_C" Full Scan
        10

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "PUBLIC"."TEST3" as "Q2_C" Full Scan
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Hash Join (semi)
        ................-> Table "PUBLIC"."TEST1" as "Q2_A" Full Scan
        ................-> Record Buffer (record length: NN)
        ....................-> Filter
        ........................-> Table "PUBLIC"."TEST2" as "Q2_B" Full Scan
        10

        3000
        {query_map[3000][0]}
        {query_map[3000][1]}
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "PUBLIC"."TEST3" as "Q3_C" Full Scan
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "Q3_B" Full Scan
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "PUBLIC"."TEST1" as "Q3_A" Full Scan
        10

        4000
        {query_map[4000][0]}
        {query_map[4000][1]}
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "PUBLIC"."TEST3" as "Q4_C" Full Scan
        Sub-query
        ....-> Filter
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "Q4_B" Full Scan
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "PUBLIC"."TEST1" as "Q4_A" Full Scan
        10
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
