#coding:utf-8

"""
ID:          issue-7647
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7647
TITLE:       Regression: Error in isc_array_lookup_bounds
DESCRIPTION:
NOTES:
    [21.07.2024] pzotov
    Checked on 3.0.12.33735, 4.0.5.3077 - works fine.
    Problem with 6.x has been fixed in 6.0.0.346, commit date 07.05.2024:
    https://github.com/FirebirdSQL/firebird/commit/17b007d14f8ccc6cfba0d63a3b2f21622ced20d0
    Removed upper limit restriction for major version.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()

expected_stdout = """
    [[1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12]]
"""

act = python_act('db')

@pytest.mark.version('>=3.0.12')
def test_1(act: Action, capsys):
    arrayIn = [
        [1, 2, 3, 4],
        [5, 6, 7, 8],
        [9,10,11,12]
      ]

    with act.db.connect() as con:
        
        try:
            con.execute_immediate('create table array_table (array_column_3x4 int[3,4])')
            con.commit()
        except DatabaseError as e:
            print(f'Failed to create a table with array field :')
            print(e.__str__())
            print(e.gds_codes)

        cur = con.cursor()
        try:
            cur.execute("insert into array_table values (?)", (arrayIn,))
            cur.execute("select array_column_3x4 from array_table")
            arrayOut = cur.fetchone()[0]
            print(f"{arrayOut}")
        except DatabaseError as e:
            print(f'Failed to insert array:')
            print(e.__str__())
            print(e.gds_codes)
        except Exception as x:
            print('Other exc:')
            print(x)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
