#coding:utf-8

"""
ID:          issue-8252
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/aae2ae3291855f51ff587d0da055aed270137e8f
TITLE:       Better fix for #8252: incorrect subquery unnesting with complex dependencies, it re-allows converting nested IN/EXISTS into multiple semi-join
DESCRIPTION:
    Test verifies additional commit related to https://github.com/FirebirdSQL/firebird/issues/8252
    Example #1 from https://github.com/FirebirdSQL/firebird/issues/8265 is used for check
    (suggested by dimitr, letter 25.09.2024 13:33)
NOTES:
    [26.09.2024] pzotov
    1. No ticket has been created for this test.
    2. Confirmed problem on 5.0.2.1516-fe6ba50 (23.09.2024), got for SubQueryConversion = true plan with subquery:
        # Sub-query
        #     -> Filter
        #         -> Filter
        #             -> Hash Join (semi)
        #                 ... <TEST2> and <TEST3> ...
        # Select Expression
        #     -> Aggregate
        #         -> Filter
        #             -> Table "TEST1" as "A" Full Scan
    3. Parameter 'SubQueryConversion' currently presents only in FB 5.x and _NOT_ in FB 6.x.
       Because of that, testing version are limited only for 5.0.2. FB 6.x currently is NOT tested.
    4. Custom driver config objects are created here, one with SubQueryConversion = true and second with false.
    
    [18.01.2025] pzotov
    Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
    in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
    Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
    This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
    The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").

    Checked on 5.0.2.1516-92316F0 -- all ok: two hash joins instead of subquery.
    Thanks to dimitr for the advice on implementing the test.

    [16.04.2025] pzotov
    Re-implemented in order to check FB 5.x with set 'SubQueryConversion = true' and FB 6.x w/o any changes in its config.
    Checked on 6.0.0.687-730aa8f, 5.0.3.1647-8993a57
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

    -- alter table test1 add constraint test1_pk primary key(id);
    -- alter table test2 add constraint test2_pk primary key(id);
    -- alter table test3 add constraint test3_pk primary key(id);
    -- alter table test2 add constraint test2_fk foreign key(pid) references test1;
    -- alter table test3 add constraint test3_fk foreign key(pid) references test2;
"""

db = db_factory(init=init_script)

# Substitusions are needed here in order to ignore concrete numbers in explained plan parts, e.g.:
# Hash Join (semi) (keys: 1, total key length: 4)
# Sort (record length: 28, key length: 8)
# Record Buffer (record length: 25)
substitutions = [
     (r'Hash Join \(semi\) \(keys: \d+, total key length: \d+\)','Hash Join (semi)')
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
        select count(*) from test1 a
        where
            a.id in (
                select b.pid from test2 b
                where
                    b.id in (
                        select c.pid from test3 c
                        where name like '%ABC%'
                    )
            );
    """

    srv_cfg = driver_config.register_server(name = f'srv_cfg_aae2ae32', config = '')
    db_cfg_name = f'db_cfg_aae2ae32'
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
                print(r[0])
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
        ....-> Aggregate
        ........-> Filter
        ............-> Hash Join (semi)
        ................-> Table "TEST1" as "A" Full Scan
        ................-> Record Buffer (record length: 82)
        ....................-> Filter
        ........................-> Hash Join (semi)
        ............................-> Table "TEST2" as "B" Full Scan
        ............................-> Record Buffer (record length: 57)
        ................................-> Filter
        ....................................-> Table "TEST3" as "C" Full Scan
        10
    """

    expected_stdout_6x = f"""
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Hash Join (semi)
        ................-> Table "PUBLIC"."TEST1" as "A" Full Scan
        ................-> Record Buffer (record length: NN)
        ....................-> Filter
        ........................-> Hash Join (semi)
        ............................-> Table "PUBLIC"."TEST2" as "B" Full Scan
        ............................-> Record Buffer (record length: NN)
        ................................-> Filter
        ....................................-> Table "PUBLIC"."TEST3" as "C" Full Scan
        10
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
