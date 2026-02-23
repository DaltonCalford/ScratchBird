#coding:utf-8

"""
ID:          issue-8233
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8233
TITLE:       SubQueryConversion = true - multiple rows in singleton select
DESCRIPTION:
NOTES:
    [27.08.2024] pzotov
        1. Confirmed bug on 5.0.1.1485-274af35 (26.08.2024), got for SubQueryConversion=true:
           "multiple rows in singleton select", gdscodes: (335544652, 335544842)
        2. Parameter 'SubQueryConversion' currently presents only in FB 5.x and _NOT_ in FB 6.x.
           Because of that, testing version are limited only for 5.0.2. FB 6.x currently is NOT tested.
        3. Table 't1' must have more than one row for bug reproducing. Query must be enclosed in execute block.
        4. Custom driver config objects are created here, one with SubQueryConversion = true and second with false.
    [18.01.2025] pzotov
        Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
        in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
        Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
        This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
        The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
        Checked on 5.0.2.1487-6934878 -- all ok.
        Thanks to dimitr for the advice on implementing the test.

    [16.04.2025] pzotov
        Re-implemented in order to check FB 5.x with set 'SubQueryConversion = true' and FB 6.x w/o any changes in its config.
        Checked on 6.0.0.687-730aa8f, 5.0.3.1647-8993a57
    [06.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.914; 5.0.3.1668.
"""

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, DatabaseError

init_script = """
create table t1(id int, fld int);
create table t2(id int, fld int);

insert into t1(id, fld) values(1, 111);
insert into t1(id, fld) values(2, 222);
insert into t1(id, fld) values(3, 333);
insert into t2(id, fld) values(3, 999);
commit;
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
        execute block returns (res int)
        as
        begin
           select first 1 id from t1
             where exists (select 1 from t2 where t1.id = t2.id)
             order by t1.id
             into :res;
           suspend;
        end
    """

    srv_cfg = driver_config.register_server(name = f'srv_cfg_8233', config = '')
    db_cfg_name = f'db_cfg_8233'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.database.value = str(act.db.db_path)
    if act.is_version('<6'):
        db_cfg_object.config.value = f"""
            SubQueryConversion = true
        """

    with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
        ps, rs =  None, None
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

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    act.expected_stdout = f"""
        Select Expression (line 5, column 12)
        ....-> Singularity Check
        ........-> First N Records
        ............-> Filter
        ................-> Hash Join (semi)
        ....................-> Sort (record length: 28, key length: 8)
        ........................-> Table {SQL_SCHEMA_PREFIX}"T1" Full Scan
        ....................-> Record Buffer (record length: 25)
        ........................-> Table {SQL_SCHEMA_PREFIX}"T2" Full Scan
        3
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
