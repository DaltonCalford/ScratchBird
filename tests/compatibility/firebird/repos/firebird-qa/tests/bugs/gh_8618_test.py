#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8618
TITLE:       Extra quotes in plan when using UNLIST function
DESCRIPTION:
NOTES:
    [17.07.2025] pzotov
    Confirmed problem on 6.0.0.845.
    Checked on 6.0.0.1020
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()

act = python_act('db')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=6')
def test_1(act: Action, capsys):

    qry_list = (
        "select count(*) from unlist('1,2,3') as system(n)"
        ,"select count(*) from unlist('1,2,3') as public(n)"
        ,'''select count(*) as """" from unlist('1,2,3') as """"(n)'''
    )

    with act.db.connect() as con:
        cur = con.cursor()
        for test_sql in qry_list:
            ps, rs =  None, None
            try:
                cur = con.cursor()
                ps = cur.prepare(test_sql)
                print(test_sql)
                # Print explained plan with padding eash line by dots in order to see indentations:
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )

                # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                # We have to store result of cur.execute(<psInstance>) in order to
                # close it explicitly.
                # Otherwise AV can occur during Python garbage collection and this
                # causes pytest to hang on its final point.
                # Explained by hvlad, email 26.10.24 17:42
                rs = cur.execute(ps)
                cur_cols = cur.description
                for r in rs:
                    for i in range(0,len(cur_cols)):
                        print( cur_cols[i][0], ':', r[i] )

            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()

    act.expected_stdout = f'''
        {qry_list[0]}
        Select Expression
        ....-> Aggregate
        ........-> Function "UNLIST" as "SYSTEM" Scan
        COUNT : 3

        {qry_list[1]}
        Select Expression
        ....-> Aggregate
        ........-> Function "UNLIST" as "PUBLIC" Scan
        COUNT : 3

        {qry_list[2]}
        Select Expression
        ....-> Aggregate
        ........-> Function "UNLIST" as """" Scan
        " : 3
    '''
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
