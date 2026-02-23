#coding:utf-8

"""
ID:          issue-5357
ISSUE:       5357
TITLE:       Compound index cannot be used for filtering in some ORDER/GROUP BY queries
DESCRIPTION:
JIRA:        CORE-5070
FBTEST:      bugs.core_5070
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    recreate table test1 (
        ia integer not null,
        id integer not null,
        it integer not null,
        dt date not null,
        constraint test1_pk_ia_id primary key (ia,id)
    );
"""

db = db_factory(init = init_script)
substitutions = [] # [('record length.*', ''), ('key length.*', '')]
act = python_act('db', substitutions = substitutions)

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
            select *
            from test1
            where
                ia=1 and dt='01/01/2015' and it=1
            order by id
        """
        ,
        2000 :
        """
            select id
            from test1
            where
                ia=1 and dt='01/01/2015' and it=1
            group by id
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
        ....-> Filter
        ........-> Table "TEST1" Access By ID
        ............-> Index "TEST1_PK_IA_ID" Range Scan (partial match: 1/2)

        {qry_map[2000]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "TEST1" Access By ID
        ................-> Index "TEST1_PK_IA_ID" Range Scan (partial match: 1/2)
    """

    expected_stdout_6x = f"""
        {qry_map[1000]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST1" Access By ID
        ............-> Index "PUBLIC"."TEST1_PK_IA_ID" Range Scan (partial match: 1/2)

        {qry_map[2000]}
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table "PUBLIC"."TEST1" Access By ID
        ................-> Index "PUBLIC"."TEST1_PK_IA_ID" Range Scan (partial match: 1/2)
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
