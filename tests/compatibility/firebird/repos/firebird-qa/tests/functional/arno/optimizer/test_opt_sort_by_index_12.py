#coding:utf-8

"""
ID:          optimizer.sort-by-index-12
TITLE:       ORDER BY ASC, DESC using index (multi)
DESCRIPTION:
  ORDER BY X ASC, Y DESC
  When more fields are given in ORDER BY clause try to use a compound index, but look out
  for mixed directions.
FBTEST:      functional.arno.optimizer.opt_sort_by_index_12
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
    CREATE TABLE Table_53 (
      ID1 INTEGER,
      ID2 INTEGER
    );

    SET TERM ^^ ;
    CREATE PROCEDURE PR_FillTable_53
    AS
    DECLARE VARIABLE FillID INTEGER;
    DECLARE VARIABLE FillID1 INTEGER;
    BEGIN
      FillID = 1;
      WHILE (FillID <= 50) DO
      BEGIN
        FillID1 = (FillID / 10) * 10;
        INSERT INTO Table_53
          (ID1, ID2)
        VALUES
          (:FillID1, :FillID - :FillID1);
        FillID = FillID + 1;
      END
      INSERT INTO Table_53 (ID1, ID2) VALUES (0, NULL);
      INSERT INTO Table_53 (ID1, ID2) VALUES (NULL, 0);
      INSERT INTO Table_53 (ID1, ID2) VALUES (NULL, NULL);
    END
    ^^
    SET TERM ; ^^

    COMMIT;

    EXECUTE PROCEDURE PR_FillTable_53;

    COMMIT;

    CREATE ASC INDEX I_Table_53_ID1_ASC ON Table_53 (ID1);
    CREATE DESC INDEX I_Table_53_ID1_DESC ON Table_53 (ID1);
    CREATE ASC INDEX I_Table_53_ID2_ASC ON Table_53 (ID2);
    CREATE DESC INDEX I_Table_53_ID2_DESC ON Table_53 (ID2);
    CREATE ASC INDEX I_Table_53_ID1_ID2_ASC ON Table_53 (ID1, ID2);
    CREATE DESC INDEX I_Table_53_ID1_ID2_DESC ON Table_53 (ID1, ID2);
    CREATE ASC INDEX I_Table_53_ID2_ID1_ASC ON Table_53 (ID2, ID1);
    CREATE DESC INDEX I_Table_53_ID2_ID1_DESC ON Table_53 (ID2, ID1);

    COMMIT;
"""

db = db_factory(init=init_script)

qry_list = (
    """
    SELECT
      t53.ID1, t53.ID2
    FROM
      Table_53 t53
    WHERE
      t53.ID1 BETWEEN 10 and 20 and
      t53.ID2 <= 5
    ORDER BY
    t53.ID1 ASC, t53.ID2 DESC
    """,
)
data_list = (
    """
    ID1 : 10
    ID2 : 5
    ID1 : 10
    ID2 : 4
    ID1 : 10
    ID2 : 3
    ID1 : 10
    ID2 : 2
    ID1 : 10
    ID2 : 1
    ID1 : 10
    ID2 : 0
    ID1 : 20
    ID2 : 5
    ID1 : 20
    ID2 : 4
    ID1 : 20
    ID2 : 3
    ID1 : 20
    ID2 : 2
    ID1 : 20
    ID2 : 1
    ID1 : 20
    ID2 : 0
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
        ....-> Sort record length: N, key length: M
        ........-> Filter
        ............-> Table "TABLE_53" as "T53" Access By ID
        ................-> Bitmap And
        ....................-> Bitmap
        ........................-> Index "I_TABLE_53_ID2_ASC" Range Scan (upper bound: 1/1)
        ....................-> Bitmap
        ........................-> Index "I_TABLE_53_ID1_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        {data_list[0]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Filter
        ............-> Table "TABLE_53" as "T53" Access By ID
        ................-> Bitmap And
        ....................-> Bitmap
        ........................-> Index "I_TABLE_53_ID2_ASC" Range Scan (upper bound: 1/1)
        ....................-> Bitmap
        ........................-> Index "I_TABLE_53_ID1_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        {data_list[0]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Filter
        ............-> Table "PUBLIC"."TABLE_53" as "T53" Access By ID
        ................-> Bitmap And
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."I_TABLE_53_ID2_ASC" Range Scan (upper bound: 1/1)
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."I_TABLE_53_ID1_ASC" Range Scan (lower bound: 1/1, upper bound: 1/1)
        {data_list[0]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
