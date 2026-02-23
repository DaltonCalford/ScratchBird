#coding:utf-8

"""
ID:          issue-1734
ISSUE:       1734
TITLE:       Data type unknown
DESCRIPTION:
JIRA:        CORE-1315
NOTES:
    [18.01.2025] pzotov
    Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
    in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
    Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
    This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
    The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
"""

from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    COALESCE
    -----------
    2

    COALESCE
    -----------
    1
"""

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        cur = con.cursor()
        ps, rs = None, None
        try:
            ps = cur.prepare('select coalesce(?,1) from RDB$DATABASE')

            # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
            # We have to store result of cur.execute(<psInstance>) in order to
            # close it explicitly.
            # Otherwise AV can occur during Python garbage collection and this
            # causes pytest to hang on its final point.
            # Explained by hvlad, email 26.10.24 17:42
            rs = cur.execute(ps,[2])
            act.print_data(rs)

            rs = cur.execute(ps,[None])
            act.print_data(rs)

        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if rs:
                rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if ps:
                ps.free()
        
        act.stdout = capsys.readouterr().out
        act.expected_stdout = expected_stdout
        assert act.clean_stdout == act.clean_expected_stdout
