#coding:utf-8

"""
ID:          issue-8223
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8223
TITLE:       SubQueryConversion = true: error "no current record for fetch operation" with complex joins
DESCRIPTION:
NOTES:
    [27.08.2024] pzotov
        1. Confirmed bug on 5.0.1.1469-1d792e4 (Release (15.08.2024), got for SubQueryConversion=true:
           no current record for fetch operation / gdscode = 335544348.
        2. Parameter 'SubQueryConversion' currently presents only in FB 5.x and _NOT_ in FB 6.x.
           Because of that, testing version are limited only for 5.0.2. FB 6.x currently is NOT tested.
        3. Custom driver config objects are created here, one with SubQueryConversion = true and second with false.
    [18.01.2025] pzotov
        Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
        in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
        Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
        This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
        The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
        
        Checked on 5.0.2.1483-0bf2de0 -- all ok.
        Thanks to dimitr for the advice on implementing the test.
    [16.04.2025] pzotov
        Re-implemented in order to check FB 5.x with set 'SubQueryConversion = true' and FB 6.x w/o any changes in its config.
        Checked on 6.0.0.687-730aa8f, 5.0.3.1647-8993a57
    [06.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.914; 5.0.3.1668.
    [15.01.2026] pzotov
        Execution plan has changed since 6.0.0.1393-f7068cd.
        Currently it is unknown whether we have to adjust actual output.
        Waiting for decision by dimitr, letter 14.01.2026 13:05.
        See  e8de18c2, "Some adjustments to the selectivity factors".
"""

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, DatabaseError

init_script = """
    create table t1(id int);
    create table t2(id int);
    create table t3(id int);
    create table t4(id int);
    create table t5(id int);

    insert into t1(id) values(1);
    insert into t2(id) values(1);
    insert into t3(id) values(1);
    insert into t4(id) values(1);
    insert into t5(id) values(1);
    commit;

    create view v as
    select a.id as a_id, b.id as b_id, c.id as c_id
    from t1 a
    left join t2 b on a.id = b.id
    left join t3 c on b.id = c.id;
    commit;
"""

db = db_factory(init=init_script)

# Substitusions are needed here in order to ignore concrete numbers in explained plan parts, e.g.:
# Hash Join (semi) (keys: 1, total key length: 4)
# Sort (record length: 28, key length: 8)
# Record Buffer (record length: 25)
substitutions = [
     (r'Hash Join \(semi\) \(keys: \d+, total key length: \d+\)','Hash Join (semi)')
    ,(r'Hash Join \(inner\) \(keys: \d+, total key length: \d+\)','Hash Join (inner)')
    ,(r'record length: \d+', 'record length: NN')
    ,(r'key length: \d+', 'key length: NN')
]

act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.2')
def test_1(act: Action, capsys):

    test_sql = """
        select v.a_id, v.b_id, v.c_id
        from v 
        join t4 d on v.c_id = d.id
        where exists (
            select 1
            from t5 e where e.id = d.id
        )
    """

    srv_cfg = driver_config.register_server(name = f'srv_cfg_8223', config = '')
    db_cfg_name = f'db_cfg_8223'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.database.value = str(act.db.db_path)
    if act.is_version('<6'):
        db_cfg_object.config.value = f"""
            SubQueryConversion = true
        """

    with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
        ps, rs = None, None
        try:
            cur = con.cursor()
            ps = cur.prepare(test_sql)

            # Print explained plan with padding eash line by dots in order to see indentations:
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )

            # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
            # We have to store result of cur.execute(<psInstance>) in order to
            # close it explicitly.
            # Otherwise AV can occur during Python garbage collection and this
            # causes pytest to hang on its final point.
            # Explained by hvlad, email 26.10.24 17:42
            rs = cur.execute(ps)
            for r in rs:
                print(r[0], r[1], r[2])
        except DatabaseError as e:
            print(e.__str__())
            print(e.gds_codes)
        finally:
            if rs:
                rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if ps:
                ps.free()


    expected_stdout_5x = f"""
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi)
        ............-> Filter
        ................-> Hash Join (inner)
        ....................-> Nested Loop Join (outer)
        ........................-> Nested Loop Join (outer)
        ............................-> Table "T1" as "V A" Full Scan
        ............................-> Filter
        ................................-> Table "T2" as "V B" Full Scan
        ........................-> Filter
        ............................-> Table "T3" as "V C" Full Scan
        ....................-> Record Buffer (record length: 25)
        ........................-> Table "T4" as "D" Full Scan
        ............-> Record Buffer (record length: 25)
        ................-> Table "T5" as "E" Full Scan
        1 1 1
    """

    expected_stdout_6x = f"""
        Select Expression
        ....-> Filter
        ........-> Hash Join (semi)
        ............-> Filter
        ................-> Hash Join (inner)
        ....................-> Nested Loop Join (outer)
        ........................-> Nested Loop Join (outer)
        ............................-> Table "PUBLIC"."T1" as "PUBLIC"."V" "A" Full Scan
        ............................-> Filter
        ................................-> Table "PUBLIC"."T2" as "PUBLIC"."V" "B" Full Scan
        ........................-> Filter
        ............................-> Table "PUBLIC"."T3" as "PUBLIC"."V" "C" Full Scan
        ....................-> Record Buffer (record length: NN)
        ........................-> Table "PUBLIC"."T4" as "D" Full Scan
        ............-> Record Buffer (record length: NN)
        ................-> Table "PUBLIC"."T5" as "E" Full Scan
        1 1 1
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
