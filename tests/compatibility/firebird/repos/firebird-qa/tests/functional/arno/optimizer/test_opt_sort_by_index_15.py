#coding:utf-8

"""
ID:          optimizer.sort-by-index-15
TITLE:       ORDER BY ASC NULLS LAST using index
DESCRIPTION:
  ORDER BY X ASC NULLS LAST
  When a index can be used for sorting, use it.
FBTEST:      functional.arno.optimizer.opt_sort_by_index_15
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
    SELECT
      ID
    FROM
      Table_66 t66
    ORDER BY
    t66.ID ASC NULLS LAST
    """,
)
data_list = (
    """
    ID : -2147483648
    ID : -1073741824
    ID : -536870912
    ID : -268435456
    ID : -134217728
    ID : -67108864
    ID : -33554432
    ID : -16777216
    ID : -8388608
    ID : -4194304
    ID : -2097152
    ID : -1048576
    ID : -524288
    ID : -262144
    ID : -131072
    ID : -65536
    ID : -32768
    ID : -16384
    ID : -8192
    ID : -4096
    ID : -2048
    ID : -1024
    ID : -512
    ID : -256
    ID : -128
    ID : -64
    ID : -32
    ID : -16
    ID : -8
    ID : -4
    ID : -2
    ID : -1
    ID : 0
    ID : 1
    ID : 3
    ID : 7
    ID : 15
    ID : 31
    ID : 63
    ID : 127
    ID : 255
    ID : 511
    ID : 1023
    ID : 2047
    ID : 4095
    ID : 8191
    ID : 16383
    ID : 32767
    ID : 65535
    ID : 131071
    ID : 262143
    ID : 524287
    ID : 1048575
    ID : 2097151
    ID : 4194303
    ID : 8388607
    ID : 16777215
    ID : 33554431
    ID : 67108863
    ID : 134217727
    ID : 268435455
    ID : 536870911
    ID : 1073741823
    ID : 2147483647
    ID : None
    ID : None
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
        ........-> Table "TABLE_66" as "T66" Full Scan
        {data_list[0]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Table "TABLE_66" as "T66" Full Scan
        {data_list[0]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Sort record length: N, key length: M
        ........-> Table "PUBLIC"."TABLE_66" as "T66" Full Scan
        {data_list[0]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
