#coding:utf-8

"""
ID:          issue-7388
ISSUE:       7388
TITLE:       Different invariants optimization between views and CTEs
DESCRIPTION:
    We run two queries as described in the ticket (see variables 'query1' and 'query2').
    For each of them we gather table statistics (sequential and indexed reads) and explained plan.
    If any value in the pair (seq, idx) differ between two statistics then we print all info about that plus explained plans.
    Otherwise we can consider test passed (without printing any concrete data from statistics or explained plans).
NOTES:
    [20.01.2024] pzotov
    Confirmed problem on 5.0.0.871.
    Checked on 6.0.0.218, 5.0.1.1318.

    [18.01.2025] pzotov
    Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
    in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
    Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
    This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
    The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
"""

from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_sql = """
    create view v1
    as select r.rdb$relation_id as id, r.rdb$relation_name as name
    from rdb$relations r
    inner join rdb$relation_fields rf on r.rdb$relation_name = rf.rdb$relation_name
    left join rdb$security_classes sc on r.rdb$security_class = sc.rdb$security_class
    ;
    commit;
"""
db = db_factory(init = init_sql)

act = python_act('db')

SUCCESS_MSG = "Expected: table statistics are identical."


#----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#----------------------------------------------------------

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):
    t_map = { 'rdb$relation_fields' : -1, 'rdb$relations' : -1, 'rdb$security_classes' : -1 }

    query1 = """
        select 1
        from v1
        where id = (select max(id) from v1)
    """

    query2 = """
        with sub as (
        select r.rdb$relation_id as id, r.rdb$relation_name as name
        from rdb$relations r
          inner join rdb$relation_fields rf on r.rdb$relation_name = rf.rdb$relation_name
          left join rdb$security_classes sc on r.rdb$security_class = sc.rdb$security_class
        )
        select * from sub
        where sub.id = (select max(id) from sub)
    """
    q_map = {query1 : '', query2 : ''}
    
    with act.db.connect() as con:
        cur = con.cursor()
        for k in t_map.keys():
            cur.execute(f"select rdb$relation_id from rdb$relations where rdb$relation_name = upper('{k}')")
            test_rel_id = None
            for r in cur:
                test_rel_id = r[0]
            assert test_rel_id, f"Could not find ID for relation '{k}'. Check its name!"
            t_map[ k ] = test_rel_id

        result_map = {}

        for qry_txt in q_map.keys():
            ps, rs = None, None
            try:
                ps = cur.prepare(qry_txt)
                q_map[qry_txt] = ps.detailed_plan
                for tab_nm,tab_id in t_map.items():
                    tabstat1 = [ p for p in con.info.get_table_access_stats() if p.table_id == tab_id ]

                    # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                    # We have to store result of cur.execute(<psInstance>) in order to
                    # close it explicitly.
                    # Otherwise AV can occur during Python garbage collection and this
                    # causes pytest to hang on its final point.
                    # Explained by hvlad, email 26.10.24 17:42
                    rs = cur.execute(ps)
                    for r in rs:
                        pass
                    tabstat2 = [ p for p in con.info.get_table_access_stats() if p.table_id == tab_id ]

                    result_map[qry_txt, tab_nm] = \
                        (
                           tabstat2[0].sequential if tabstat2[0].sequential else 0
                          ,tabstat2[0].indexed if tabstat2[0].indexed else 0
                        )
                    if tabstat1:
                        seq, idx = result_map[qry_txt, tab_nm]
                        seq -= (tabstat1[0].sequential if tabstat1[0].sequential else 0)
                        idx -= (tabstat1[0].indexed if tabstat1[0].indexed else 0)
                        result_map[qry_txt, tab_nm] = (seq, idx)

            except DatabaseError as e:
                print( e.__str__() )
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()

        '''
        print('q_map.items():')
        for k,v in q_map.items():
            print('k=',k)
            print('v=',v)
            print('')

        print('')
        print('result_map.items():')
        for k,v in result_map.items():
            print('(query,tab_nm)=',k)
            print('v=',v)
            print('')
        '''

        mism_found = 0
        for tab_nm in t_map.keys():
            if result_map[query1, tab_nm] == result_map[query2, tab_nm]:
                pass
            else:
                print(f"Mismatch detected in the statistics for table '{tab_nm}'.")
                print('Query-1:')
                print('(seq,idx) =',result_map[query1, tab_nm])
                print('Query-2:')
                print('(seq,idx) =',result_map[query2, tab_nm])
                mism_found += 1

        if mism_found:
            print('Check execution plans:')
            for i,qry_txt in enumerate(q_map.keys()):
                print('-' * 22)
                print(f'Query-{i+1}:')
                print(qry_txt)
                print('-' * 22)
                print('Plan:')
                # Show explained plan, with preserving indents by replacing leading spaces with '.':
                print( '\n'.join([replace_leading(s) for s in q_map[qry_txt].split('\n')]) )
                print('')
        else:
            print(SUCCESS_MSG)


    act.expected_stdout = SUCCESS_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
