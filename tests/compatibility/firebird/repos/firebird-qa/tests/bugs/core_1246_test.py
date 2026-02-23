#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1670
TITLE:       Incorrect column values with outer joins and derived tables
DESCRIPTION:
NOTES:
    [20.02.2026] pzotov
    Re-implemented: FULL JOIN execution plan has changed since 19.02.2026 6.0.0.1458, commit:
    "6a76c1da Better index usage in full outer joins...".  This changed the order of rows in resultset.
    The fix is simple: we can add 'ORDER BY' to the query but this must not change execution plan
    (i.e. optimizer still has to use FULL JOIN in this case! Discussed with dimitr, 20.02.2026 1345).
    Because of this, explained plan is shown for additional check and it must contain 'Full Outer Join'.
    Min-version increased to 5.0 (no changes expected in FB 3.x/4.x related to FULL JOIN).

    Checked on 6.0.0.1461-5e98812; 5.0.4.1767-52823f5.
"""

import pytest
from firebird.qa import *

init_script = """
    create table t1 (n int);
    create table t2 (n int);

    insert into t1 values (1);
    insert into t1 values (2);
    insert into t2 values (2);
    commit;
"""

db = db_factory(init=init_script)

substitutions = [('[ \t]+', ' '), ('(record|key)\\s+length(:)?\\s+\\d+', 'record/key length: NNN')]
act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5')
def test_1(act: Action, capsys):


    query_map = {
        1000 : (
                    f"""
                        select t1.n as t1_n, t2.n as t2_n
                        from (
                            select 1 n from rdb$database
                        ) t1
                        full join (
                            select 2 n from rdb$database
                        ) t2 on t2.n = t1.n
                        order by 1,2 -- <<< added 20.02.2026
                    """
                   ,''
               )
    }

    with act.db.connect() as con:
        cur = con.cursor()
        for q_idx, q_tuple in query_map.items():
            test_sql, qry_comment = q_tuple[:2]
            ps, rs = None, None
            try:
                print(q_idx)
                print(test_sql)
                print(qry_comment)
                ps = cur.prepare(test_sql)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                rs = cur.execute(ps)
                cur_cols = cur.description
                for r in cur:
                    for i in range(0,len(cur_cols)):
                        print( cur_cols[i][0], ':', r[i] )
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close()
                if ps:
                    ps.free()

    expected_stdout_5x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Sort (record length: 52, key length: 16)
        ........-> Full Outer Join
        ............-> Nested Loop Join (outer)
        ................-> Table "RDB$DATABASE" as "T2 RDB$DATABASE" Full Scan
        ................-> Filter
        ....................-> Table "RDB$DATABASE" as "T1 RDB$DATABASE" Full Scan
        ............-> Nested Loop Join (outer)
        ................-> Table "RDB$DATABASE" as "T1 RDB$DATABASE" Full Scan
        ................-> Filter
        ....................-> Table "RDB$DATABASE" as "T2 RDB$DATABASE" Full Scan
        T1_N : None
        T2_N : 2
        T1_N : 1
        T2_N : None
    """

    expected_stdout_6x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Sort (record length: 52, key length: 16)
        ........-> Full Outer Join
        ............-> Nested Loop Join (outer)
        ................-> Table "SYSTEM"."RDB$DATABASE" as "T1" "SYSTEM"."RDB$DATABASE" Full Scan
        ................-> Filter
        ....................-> Table "SYSTEM"."RDB$DATABASE" as "T2" "SYSTEM"."RDB$DATABASE" Full Scan
        ............-> Nested Loop Join (outer)
        ................-> Table "SYSTEM"."RDB$DATABASE" as "T2" "SYSTEM"."RDB$DATABASE" Full Scan
        ................-> Filter
        ....................-> Table "SYSTEM"."RDB$DATABASE" as "T1" "SYSTEM"."RDB$DATABASE" Full Scan
        T1_N : None
        T2_N : 2
        T1_N : 1
        T2_N : None
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
