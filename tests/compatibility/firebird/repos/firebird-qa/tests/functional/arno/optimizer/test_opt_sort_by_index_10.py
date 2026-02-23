#coding:utf-8

"""
ID:          optimizer.sort-by-index-10
TITLE:       ORDER BY ASC using index (multi)
DESCRIPTION:
  ORDER BY X, Y
  When more fields are given in ORDER BY clause try to use a compound index.
FBTEST:      functional.arno.optimizer.opt_sort_by_index_10
NOTES:
    [08.07.2025] pzotov
    Refactored: explained plan is used to be checked in expected_out.
    Added ability to use several queries and their datasets for check - see 'qry_list' and 'qry_data' tuples.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.930; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""

from firebird.driver import DatabaseError
import pytest
from firebird.qa import *

init_script = """
    recreate table test_idx (
      id1 integer,
      id2 integer
    );
    insert into test_idx(id1, id2)
    select (r/10)*10, r - (r/10)*10
    from (select row_number()over() r from rdb$types rows 50);
    insert into test_idx (id1, id2) values (0, null);
    insert into test_idx (id1, id2) values (null, 0);
    insert into test_idx (id1, id2) values (null, null);
    commit;

    create asc  index idx_id1_asc      on test_idx(id1);
    create desc index idx_id1_desc     on test_idx(id1);
    create asc  index idx_id2_asc      on test_idx(id2);
    create desc index idx_id2_desc     on test_idx(id2);
    create asc  index idx_id1_id2_asc  on test_idx(id1, id2);
    create desc index idx_id1_id2_desc on test_idx(id1, id2);
    create asc  index idx_id2_id1_asc  on test_idx(id2, id1);
    create desc index idx_id2_id1_desc on test_idx(id2, id1);
    commit;
"""

db = db_factory(init=init_script)

qry_list = (
    # Queries with RANGE index scan now have in the plan only "ORDER"
    # clause (index navigation) without bitmap building.
    # See: http://tracker.firebirdsql.org/browse/CORE-1550
    # ("the same index should never appear in both ORDER and INDEX parts of the same plan item")
    # must navigate through the leaf level of idx_id1_id2_asc, *without* bitmap!
    """
    select t.id1, t.id2
    from test_idx t
    where t.id1 = 40
    order by  t.id1 asc, t.id2 asc;
    """,
)
data_list = (
    """
    ID1 : 40
    ID2 : 0
    ID1 : 40
    ID2 : 1
    ID1 : 40
    ID2 : 2
    ID1 : 40
    ID2 : 3
    ID1 : 40
    ID2 : 4
    ID1 : 40
    ID2 : 5
    ID1 : 40
    ID2 : 6
    ID1 : 40
    ID2 : 7
    ID1 : 40
    ID2 : 8
    ID1 : 40
    ID2 : 9
    """,
)

substitutions = [ ( r'\(record length: \d+, key length: \d+\)', 'record length: N, key length: M' ) ]
act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
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

    expected_out_4x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST_IDX" as "T" Access By ID
        ............-> Index "IDX_ID1_ID2_ASC" Range Scan (partial match: 1/2)
        {data_list[0]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Filter
        ........-> Table "TEST_IDX" as "T" Access By ID
        ............-> Index "IDX_ID1_ID2_ASC" Range Scan (partial match: 1/2)
        {data_list[0]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST_IDX" as "T" Access By ID
        ............-> Index "PUBLIC"."IDX_ID1_ID2_ASC" Range Scan (partial match: 1/2)
        {data_list[0]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
