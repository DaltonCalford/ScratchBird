#coding:utf-8

"""
ID:          issue-4492
ISSUE:       4492
TITLE:       Replace the hierarchical union execution with the plain one
DESCRIPTION:
JIRA:        CORE-4165
FBTEST:      bugs.core_4165
NOTES:
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_script = """
    recreate table t1(id int);
    recreate table t2(id int);
    recreate table t3(id int);
    commit;
    insert into t1 select rand()*100 from rdb$types;
    commit;
    insert into t2 select * from t1;
    insert into t3 select * from t1;
    commit;
"""

db = db_factory(init=init_script)
act = python_act('db', substitutions=[('record length.*', ''), ('key length.*', '')])

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    qry_map = {
        1000 :
        """
            select 0 i from t1
            union all
            select 1 from t1
            union all
            select 2 from t1
        """
        ,
        2000 :
        """
            select 0 i from t2
            union
            select 1 from t2
            union
            select 2 from t2
        """
        ,
        3000 :
        """
            select 0 i from t3
            union distinct
            select 1 from t3
            union all
            select 2 from t3
        """
    }

    with act.db.connect() as con:
        cur = con.cursor()

        for k, v in qry_map.items():
            ps, rs = None, None
            try:
                ps = cur.prepare(v)

                print(v)
                # Print explained plan with padding eash line by dots in order to see indentations:
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                print('')

                # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                # We have to store result of cur.execute(<psInstance>) in order to
                # close it explicitly.
                # Otherwise AV can occur during Python garbage collection and this
                # causes pytest to hang on its final point.
                # Explained by hvlad, email 26.10.24 17:42
                #rs = cur.execute(ps)
                #for r in rs:
                #    print(r[0], r[1])
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()
        
    expected_stdout_5x = f"""
        {qry_map[1000]}
        Select Expression
        ....-> Union
        ........-> Table "T1" Full Scan
        ........-> Table "T1" Full Scan
        ........-> Table "T1" Full Scan

        {qry_map[2000]}
        Select Expression
        ....-> Unique Sort (
        ........-> Union
        ............-> Table "T2" Full Scan
        ............-> Table "T2" Full Scan
        ............-> Table "T2" Full Scan

        {qry_map[3000]}
        Select Expression
        ....-> Union
        ........-> Unique Sort (
        ............-> Union
        ................-> Table "T3" Full Scan
        ................-> Table "T3" Full Scan
        ........-> Table "T3" Full Scan
    """

    expected_stdout_6x = f"""
        {qry_map[1000]}
        Select Expression
        ....-> Union
        ........-> Table "PUBLIC"."T1" Full Scan
        ........-> Table "PUBLIC"."T1" Full Scan
        ........-> Table "PUBLIC"."T1" Full Scan

        {qry_map[2000]}
        Select Expression
        ....-> Unique Sort (
        ........-> Union
        ............-> Table "PUBLIC"."T2" Full Scan
        ............-> Table "PUBLIC"."T2" Full Scan
        ............-> Table "PUBLIC"."T2" Full Scan

        {qry_map[3000]}
        Select Expression
        ....-> Union
        ........-> Unique Sort (
        ............-> Union
        ................-> Table "PUBLIC"."T3" Full Scan
        ................-> Table "PUBLIC"."T3" Full Scan
        ........-> Table "PUBLIC"."T3" Full Scan
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
