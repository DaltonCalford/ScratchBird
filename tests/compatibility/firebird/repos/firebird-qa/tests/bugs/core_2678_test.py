#coding:utf-8

"""
ID:          issue-3081
ISSUE:       3081
TITLE:       Full outer join cannot use available indices (very slow execution)
DESCRIPTION:
JIRA:        CORE-2678
FBTEST:      bugs.core_2678
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.

    [20.02.2026] pzotov
    Re-implemented: explained plan is shown now (it has changed in 6.x 19.02.2026, snapshot 6.0.0.1458,
    commit: "6a76c1da Better index usage in full outer joins...").
    Min-version increased to 5.0 (no changes expected in FB 3.x/4.x related to FULL JOIN).
    Checked on 6.0.0.1461-5e98812; 5.0.4.1767-52823f5.
"""

import pytest
from firebird.qa import *

init_script = """
    create table td_data1 (
      c1 varchar(20) character set win1251 not null collate win1251,
      c2 integer not null,
      c3 date not null,
      d1 float not null
    );
    create index idx_td_data1 on td_data1(c1,c2,c3);
    commit;

    create table td_data2 (
      c1 varchar(20) character set win1251 not null collate win1251,
      c2 integer not null,
      c3 date not null,
      d2 float not null
    );
    create index idx_td_data2 on td_data2(c1,c2,c3);
    commit;
"""

db = db_factory(init = init_script)

query_map = {
    1000 : (
                """
                    select
                        d1.c1, d2.c1,
                        d1.c2, d2.c2,
                        d1.c3, d2.c3,
                        coalesce(sum(d1.d1), 0) t1,
                        coalesce(sum(d2.d2), 0) t2
                    from td_data1 d1
                    full join td_data2 d2
                        on
                            d2.c1 = d1.c1
                            and d2.c2 = d1.c2
                            and d2.c3 = d1.c3
                    group by
                        d1.c1, d2.c1,
                        d1.c2, d2.c2,
                        d1.c3, d2.c3
                """
               ,''
           )
}

substitutions = [('[ \t]+', ' '), ('(record|key)\\s+length(:)?\\s+\\d+', 'record/key length: NNN')]
act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=5')
def test_1(act: Action, capsys):

    expected_stdout_5x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Sort (record length: 180, key length: 80)
        ............-> Full Outer Join
        ................-> Nested Loop Join (outer)
        ....................-> Table "TD_DATA2" as "D2" Full Scan
        ....................-> Filter
        ........................-> Table "TD_DATA1" as "D1" Access By ID
        ............................-> Bitmap
        ................................-> Index "IDX_TD_DATA1" Range Scan (full match)
        ................-> Nested Loop Join (outer)
        ....................-> Table "TD_DATA1" as "D1" Full Scan
        ....................-> Filter
        ........................-> Table "TD_DATA2" as "D2" Access By ID
        ............................-> Bitmap
        ................................-> Index "IDX_TD_DATA2" Range Scan (full match)
    """

    expected_stdout_6x = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Aggregate
        ........-> Sort (record length: 180, key length: 80)
        ............-> Full Outer Join
        ................-> Nested Loop Join (outer)
        ....................-> Table "PUBLIC"."TD_DATA1" as "D1" Full Scan
        ....................-> Filter
        ........................-> Table "PUBLIC"."TD_DATA2" as "D2" Access By ID
        ............................-> Bitmap
        ................................-> Index "PUBLIC"."IDX_TD_DATA2" Range Scan (full match)
        ................-> Nested Loop Join (outer)
        ....................-> Table "PUBLIC"."TD_DATA2" as "D2" Full Scan
        ....................-> Filter
        ........................-> Table "PUBLIC"."TD_DATA1" as "D1" Access By ID
        ............................-> Bitmap
        ................................-> Index "PUBLIC"."IDX_TD_DATA1" Range Scan (full match)
    """

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
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close()
                if ps:
                    ps.free()

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
