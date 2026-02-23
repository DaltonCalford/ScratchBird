#coding:utf-8

"""
ID:          optimizer.sort-by-index-09
TITLE:       ORDER BY ASC using index (non-unique)
DESCRIPTION:
  ORDER BY X
  If WHERE clause is present it should also use index if possible.
FBTEST:      functional.arno.optimizer.opt_sort_by_index_09
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
    create table table_66 (id integer);
    commit;
    set term ^ ;
    create procedure pr_filltable_66 as
      declare fillid integer;
    begin
      fillid = 2147483647;
      while (fillid > 0) do
      begin
        insert into table_66 (id) values (:fillid);
        fillid = fillid / 2;
      end
      insert into table_66 (id) values (null);
      insert into table_66 (id) values (0);
      insert into table_66 (id) values (null);
      fillid = -2147483648;
      while (fillid < 0) do
      begin
        insert into table_66 (id) values (:fillid);
        fillid = fillid / 2;
      end
    end
    ^
    set term ; ^
    commit;

    execute procedure pr_filltable_66;
    commit;

    create asc index i_table_66_asc on table_66 (id);
    create desc index i_table_66_desc on table_66 (id);
    commit;
  """

db = db_factory(init=init_script)

qry_list = (
    # clause (index navigation) without bitmap building.
    # See: http://tracker.firebirdsql.org/browse/CORE-1550
    # ("the same index should never appear in both ORDER and INDEX parts of the same plan item")
    # Queries with RANGE index scan now have in the plan only "ORDER"
    """
    select id as id_asc
    from table_66 t66
    where t66.id between -20 and 20
    order by t66.id asc
    """
    ,
    """
    select id as id_desc
    from table_66 t66
    where t66.id between -20 and 20
    order by t66.id desc
    """,
)
data_list = (
    """
        ID_ASC : -16
        ID_ASC : -8
        ID_ASC : -4
        ID_ASC : -2
        ID_ASC : -1
        ID_ASC : 0
        ID_ASC : 1
        ID_ASC : 3
        ID_ASC : 7
        ID_ASC : 15
    """
    ,
    """
        ID_DESC : 15
        ID_DESC : 7
        ID_DESC : 3
        ID_DESC : 1
        ID_DESC : 0
        ID_DESC : -1
        ID_DESC : -2
        ID_DESC : -4
        ID_DESC : -8
        ID_DESC : -16
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
        ........-> Table "TABLE_66" as "T66" Access By ID
        ............-> Index "I_TABLE_66_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        {data_list[0]}

        {qry_list[1]}
        Select Expression
        ....-> Filter
        ........-> Table "TABLE_66" as "T66" Access By ID
        ............-> Index "I_TABLE_66_DESC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        {data_list[1]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Filter
        ........-> Table "TABLE_66" as "T66" Access By ID
        ............-> Index "I_TABLE_66_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        {data_list[0]}

        {qry_list[1]}
        Select Expression
        ....-> Filter
        ........-> Table "TABLE_66" as "T66" Access By ID
        ............-> Index "I_TABLE_66_DESC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        {data_list[1]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TABLE_66" as "T66" Access By ID
        ............-> Index "PUBLIC"."I_TABLE_66_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        {data_list[0]}

        {qry_list[1]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TABLE_66" as "T66" Access By ID
        ............-> Index "PUBLIC"."I_TABLE_66_DESC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        {data_list[1]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
