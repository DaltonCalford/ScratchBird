#coding:utf-8

"""
ID:          optimizer.sort-by-index-05
TITLE:       MAX() and DESC index (non-unique)
DESCRIPTION:
 SELECT MAX(FieldX) FROM X
  When a index can be used for sorting, use it.
FBTEST:      functional.arno.optimizer.opt_sort_by_index_05
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
    CREATE TABLE Table_66 (
      ID INTEGER
    );

    SET TERM ^^ ;
    CREATE PROCEDURE PR_FillTable_66
    AS
    DECLARE VARIABLE FillID INTEGER;
    BEGIN
      FillID = 2147483647;
      WHILE (FillID > 0) DO
      BEGIN
        INSERT INTO Table_66 (ID) VALUES (:FillID);
        FillID = FillID / 2;
      END
      INSERT INTO Table_66 (ID) VALUES (NULL);
      INSERT INTO Table_66 (ID) VALUES (0);
      INSERT INTO Table_66 (ID) VALUES (NULL);
      FillID = -2147483648;
      WHILE (FillID < 0) DO
      BEGIN
        INSERT INTO Table_66 (ID) VALUES (:FillID);
        FillID = FillID / 2;
      END
    END
    ^^
    SET TERM ; ^^

    COMMIT;

    EXECUTE PROCEDURE PR_FillTable_66;

    COMMIT;

    CREATE ASC INDEX I_Table_66_ASC ON Table_66 (ID);
    CREATE DESC INDEX I_Table_66_DESC ON Table_66 (ID);

    COMMIT;
"""

db = db_factory(init=init_script)

qry_list = (
    """
    SELECT MAX(t66.ID) AS MAX_ID
    FROM Table_66 t66
    """,
)
data_list = (
    """
    MAX_ID : 2147483647
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
        ....-> Aggregate
        ........-> Table "TABLE_66" as "T66" Access By ID
        ............-> Index "I_TABLE_66_DESC" Full Scan
        {data_list[0]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Aggregate
        ........-> Table "TABLE_66" as "T66" Access By ID
        ............-> Index "I_TABLE_66_DESC" Full Scan
        {data_list[0]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Aggregate
        ........-> Table "PUBLIC"."TABLE_66" as "T66" Access By ID
        ............-> Index "PUBLIC"."I_TABLE_66_DESC" Full Scan
        {data_list[0]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
