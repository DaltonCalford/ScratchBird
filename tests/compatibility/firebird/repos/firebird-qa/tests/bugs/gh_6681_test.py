#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6681
TITLE:       Support for WHEN NOT MATCHED BY SOURCE for MERGE statement
DESCRIPTION:
NOTES:
    [20.02.2026] pzotov
    Re-implemented.
    Statement 'MERGE ... RETURNING' produces cursor which may contain several rows
    (see https://github.com/FirebirdSQL/firebird/issues/6815 ; 25-aug-2021, 5.0 Beta1).

    Such rows can be delivered to client in unpredictable order, in particular when
    execution plan has changed (this occurred in 6.x since 19.02.2026 6.0.0.1458,
    commit: "6a76c1da Better index usage in full outer joins...").

    Unfortunately, current FB versions do not allow to handle multiple rows returning
    by DML in PSQL cursor ("multiple rows in singleton select" is raised), see:
    https://www.firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref50/fblangref50-dml-merge.html#fblangref50-dml-merge-returning

    Because of that, we have to use *firebird-driver* cursor and its ability to handle
    such rows and accumulate output in the list with further SORTING (see 'merge_output').
    
    Checked on 6.0.0.1461-5e98812; 6.0.0.1454-b45aa4e; 5.0.4.1767-52823f5.
"""

from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_script = """
    recreate table ts(id int primary key, x int);
    recreate table tt(id int primary key, x int);

    insert into ts(id, x) values(5, 500);
    insert into ts(id, x) values(4, 400);
    insert into ts(id, x) values(1, 100);
    insert into ts(id, x) values(3, 300);
    insert into ts(id, x) values(2, 200);

    insert into tt(id, x) values(3, 30);
    insert into tt(id, x) values(7, 70);
    insert into tt(id, x) values(6, 60);
    insert into tt(id, x) values(4, 40);
    insert into tt(id, x) values(5, 50);

    commit;
"""

db = db_factory(init = init_script)

query_map = {

    1000 : (
                """
                    merge into tt t using ts s on s.id = t.id
                    when matched then update set t.x = t.x + s.x
                    when NOT matched BY TARGET then insert values(-10 * s.id, -10 * s.x)
                    returning t.id as tt_id_3, t.x as tt_x_3
                """
               ,''
           )
    ,
    2000 : (
                """
                    merge into tt t using ts s on s.id = t.id
                    when matched then update set t.x = t.x + s.x
                    when NOT matched BY SOURCE then update set t.id = -100 * t.id, t.x = -100 * t.x
                    returning t.id as tt_id_4, t.x as tt_x_4
                """
               ,''
           )
    ,
    3000 : (
               """
                   merge into tt t using ts s on s.id = t.id
                   when matched then update set t.x = t.x + s.x
                   when NOT matched BY SOURCE then delete
                   when NOT matched BY TARGET then insert values(-10 * s.id, -10 * s.x)
                   returning t.id as tt_id_5, t.x as tt_x_5
               """
              ,''
           )
}

substitutions = [('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    with act.db.connect() as con:
        cur = con.cursor()
        for q_idx, q_tuple in query_map.items():
            test_sql, qry_comment = q_tuple[:2]
            ps, rs = None, None
            merge_output = []
            try:
                print(q_idx)
                print(test_sql)
                print(qry_comment)
                ps = cur.prepare(test_sql)
                #print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                rs = cur.execute(ps)
                cur_cols = cur.description
                for r in cur:
                    for i in range(0,len(cur_cols)):
                        # DO NOT print(...)! Order of obtained records can change!
                        ##############################################################
                        ###   A C C U M U L A T E    D A T A    I N     L I S T   ###
                        ##############################################################
                        merge_output.append( (cur_cols[i][0].ljust(32), r[i]) )
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close()
                if ps:
                    ps.free()
                con.rollback()

            # Mandatory: *sort* content of accumulated data before print!
            for x in sorted(merge_output):
                print(x[0],x[1])

    expected_stdout = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        TT_ID_3                          -20
        TT_ID_3                          -10
        TT_ID_3                          3
        TT_ID_3                          4
        TT_ID_3                          5
        TT_X_3                           -2000
        TT_X_3                           -1000
        TT_X_3                           330
        TT_X_3                           440
        TT_X_3                           550

        2000
        {query_map[2000][0]}
        {query_map[2000][1]}
        TT_ID_4                          -700
        TT_ID_4                          -600
        TT_ID_4                          3
        TT_ID_4                          4
        TT_ID_4                          5
        TT_X_4                           -7000
        TT_X_4                           -6000
        TT_X_4                           330
        TT_X_4                           440
        TT_X_4                           550

        3000
        {query_map[3000][0]}
        {query_map[3000][1]}
        TT_ID_5                          -20
        TT_ID_5                          -10
        TT_ID_5                          3
        TT_ID_5                          4
        TT_ID_5                          5
        TT_ID_5                          6
        TT_ID_5                          7
        TT_X_5                           -2000
        TT_X_5                           -1000
        TT_X_5                           60
        TT_X_5                           70
        TT_X_5                           330
        TT_X_5                           440
        TT_X_5                           550
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
