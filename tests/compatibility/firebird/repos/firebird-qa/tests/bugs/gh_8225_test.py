#coding:utf-8

"""
ID:          issue-8225
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8225
TITLE:       Problematic queries when SubQueryConversion = true
DESCRIPTION:
NOTES:
    [03.09.2024] pzotov
        Parameter 'SubQueryConversion' currently presents only in FB 5.x and _NOT_ in FB 6.x.
        Because of that, testing version are limited only for 5.0.2. FB 6.x currently is NOT tested.
    [18.01.2025] pzotov
        Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
        in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
        Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
        This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
        The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
        
        Confirmed bug on 5.0.2.1479-adfe97a.
        Checked on 5.0.2.1482-604555f.
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
    create domain dm_emp_id smallint;
    create domain dm_dep_id smallint;
    create domain dm_name varchar(20);

    create table department (
         dept_no dm_dep_id not null
        ,dept_name dm_name not null
    );

    create table employee (
        emp_no dm_emp_id not null
        ,last_name dm_name not null
        ,dept_no dm_dep_id not null
        ,constraint emp_key primary key (emp_no)
    );
    commit;
    insert into department( dept_no, dept_name) values (1, 'd1');
    insert into department( dept_no, dept_name) values (2, 'd2');
    insert into department( dept_no, dept_name) values (3, 'd3');
    insert into employee( emp_no, last_name, dept_no) values (1, 'e1', 1);
    insert into employee( emp_no, last_name, dept_no) values (2, 'e2', 2);
    insert into employee( emp_no, last_name, dept_no) values (3, 'e3', 3);
    insert into employee( emp_no, last_name, dept_no) values (4, 'e4', 1);
    insert into employee( emp_no, last_name, dept_no) values (5, 'e5', 1);
    insert into employee( emp_no, last_name, dept_no) values (6, 'e6', 1);
    insert into employee( emp_no, last_name, dept_no) values (7, 'e7', 2);
    insert into employee( emp_no, last_name, dept_no) values (8, 'e8', 3);
    insert into employee( emp_no, last_name, dept_no) values (9, 'e9', 3);
    commit;

    update department d set dept_no = -dept_no where exists(select * from employee e where e.dept_no = d.dept_no) rows 1;
    insert into employee( emp_no, last_name, dept_no) values (12, 'e12', -(select max(dept_no)+1 from department) );
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
        select d.dept_no, d.dept_name from department d
        where exists(select * from employee e where e.dept_no = d.dept_no)
        order by dept_no rows 1
    """

    srv_cfg = driver_config.register_server(name = f'srv_cfg_8225', config = '')
    db_cfg_name = f'db_cfg_8225'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.database.value = str(act.db.db_path)
    if act.is_version('<6'):
        db_cfg_object.config.value = f"""
            SubQueryConversion = true
        """

    with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
        cur = con.cursor()
        ps, rs = None, None
        try:
            ps = cur.prepare(test_sql)
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )

            # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
            # We have to store result of cur.execute(<psInstance>) in order to
            # close it explicitly.
            # Otherwise AV can occur during Python garbage collection and this
            # causes pytest to hang on its final point.
            # Explained by hvlad, email 26.10.24 17:42
            rs = cur.execute(ps)
            for r in rs:
                print(r[0],r[1])

        except DatabaseError as e:
            print(e.__str__())
            print(e.gds_codes)
        finally:
            if rs:
                rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if ps:
                ps.free()

        con.rollback()

    expected_stdout_5x = f"""
        Select Expression
        ....-> First N Records
        ........-> Filter
        ............-> Hash Join (semi)
        ................-> Refetch
        ....................-> Sort (record length: 28, key length: 8)
        ........................-> Table "DEPARTMENT" as "D" Full Scan
        ................-> Record Buffer (record length: 25)
        ....................-> Table "EMPLOYEE" as "E" Full Scan
        2 d2
    """

    expected_stdout_6x = f"""
        Select Expression
        ....-> First N Records
        ........-> Filter
        ............-> Hash Join (semi)
        ................-> Refetch
        ....................-> Sort (record length: NN, key length: NN)
        ........................-> Table "PUBLIC"."DEPARTMENT" as "D" Full Scan
        ................-> Record Buffer (record length: NN)
        ....................-> Table "PUBLIC"."EMPLOYEE" as "E" Full Scan
        2 d2
    """
   
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

