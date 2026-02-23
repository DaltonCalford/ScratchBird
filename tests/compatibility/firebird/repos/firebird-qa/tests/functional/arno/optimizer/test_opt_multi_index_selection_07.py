#coding:utf-8

"""
ID:          optimizer.multi-index-selection-06
TITLE:       Best match index selection (multi segment)
DESCRIPTION:
  Check if it will select the index with the best selectivity and match.
  IS NULL should also be used in compound indexes.
FBTEST:      functional.arno.optimizer.opt_multi_index_selection_07
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    CREATE TABLE SelectionTest (
      F1 INTEGER NOT NULL,
      F2 INTEGER
    );

    SET TERM ^^ ;
    CREATE PROCEDURE PR_SelectionTest
    AS
    DECLARE VARIABLE FillID INTEGER;
    DECLARE VARIABLE FillF2 INTEGER;
    BEGIN
      FillID = 1;
      WHILE (FillID <= 1000) DO
      BEGIN

        IF (FillID <= 100) THEN
        BEGIN
          FillF2 = NULL;
        END ELSE BEGIN
          FillF2 = FillID - 100;
        END

        INSERT INTO SelectionTest
          (F1, F2)
        VALUES
          ((:FillID / 5) * 5, :FILLF2);
        FillID = FillID + 1;
      END
    END
    ^^
    SET TERM ; ^^

    COMMIT;

    /* Fill table with data */
    EXECUTE PROCEDURE PR_SelectionTest;

    COMMIT;

    /* Create indexes */
    CREATE ASC INDEX I_F1_ASC ON SelectionTest (F1);
    CREATE DESC INDEX I_F1_DESC ON SelectionTest (F1);
    CREATE ASC INDEX I_F2_ASC ON SelectionTest (F2);
    CREATE DESC INDEX I_F2_DESC ON SelectionTest (F2);
    CREATE ASC INDEX I_F1_F2_ASC ON SelectionTest (F1, F2);
    CREATE DESC INDEX I_F1_F2_DESC ON SelectionTest (F1, F2);

    COMMIT;
"""

db = db_factory(init=init_script)

qry_list = (
    """
    SELECT
      st.F1, st.F2
    FROM
      SelectionTest st
    WHERE
      st.F1 = 55 and
    st.F2 IS NULL
    """,
)
data_list = (
    """
    F1 : 55
    F2 : None
    F1 : 55
    F2 : None
    F1 : 55
    F2 : None
    F1 : 55
    F2 : None
    F1 : 55
    F2 : None
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
        ........-> Table "SELECTIONTEST" as "ST" Access By ID
        ............-> Bitmap
        ................-> Index "I_F1_F2_ASC" Range Scan (full match)
        {data_list[0]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Filter
        ........-> Table "SELECTIONTEST" as "ST" Access By ID
        ............-> Bitmap
        ................-> Index "I_F1_F2_ASC" Range Scan (full match)
        {data_list[0]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."SELECTIONTEST" as "ST" Access By ID
        ............-> Bitmap
        ................-> Index "PUBLIC"."I_F1_F2_ASC" Range Scan (full match)
        {data_list[0]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
