#coding:utf-8

"""
ID:          issue-8252
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8252
TITLE:       Incorrect subquery unnesting with complex dependencies (SubQueryConversion = true)
DESCRIPTION:
NOTES:
    [14.09.2024] pzotov
        1. Parameter 'SubQueryConversion' currently presents only in FB 5.x and _NOT_ in FB 6.x.
           Because of that, testing version are limited only for 5.0.2. FB 6.x currently is NOT tested.
        2. Custom driver config object is created here for using 'SubQueryConversion = true'.
        3. Additional test was made for this issue: tests/functional/tabloid/test_aae2ae32.py
    [18.01.2025] pzotov
        Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
        in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
        Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
        This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
        The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
        Confirmed bug on 5.0.2.1497. Checked on 5.0.2.1499-5fa4ae6.
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
from firebird.driver import driver_config, connect, NetProtocol

db = db_factory()

act = python_act('db', substitutions = [ ('[ \t]+',' ') ])

test_sql = """
    select /* TRACE_ME */ first 5 1 x
    from sales s
    where exists (
        select 1 from customer c
        where
            s.cust_no = c.cust_no
            and ( s.cust_no = c.cust_no
                  or
                  s.cust_no = c.cust_no
                )
    );
"""

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5.0.2')
def test_1(act: Action, capsys):

    srv_cfg = driver_config.register_server(name = 'test_srv_gh_8252', config = '')
    db_cfg_name = f'db_cfg_8252'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.database.value = 'employee'
    if act.is_version('<6'):
        db_cfg_object.config.value = f"""
            SubQueryConversion = true
    """

    with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
        cur = con.cursor()
        ps = cur.prepare(test_sql)
        # Show explained plan:
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

        rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
        ps.free()
        
        con.rollback()

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    act.expected_stdout = f"""
        Sub-query
        ....-> Filter
        ........-> Table {SQL_SCHEMA_PREFIX}"CUSTOMER" as "C" Access By ID
        ............-> Bitmap
        ................-> Index {SQL_SCHEMA_PREFIX}"RDB$PRIMARY22" Unique Scan
        Select Expression
        ....-> First N Records
        ........-> Filter
        ............-> Table {SQL_SCHEMA_PREFIX}"SALES" as "S" Full Scan
        1
        1
        1
        1
        1
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
