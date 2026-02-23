#coding:utf-8

"""
ID:          optimizer.sort-by-index-02
TITLE:       ORDER BY DESC using index (unique)
DESCRIPTION:
  ORDER BY X
  When a index can be used for sorting, use it.
FBTEST:      functional.arno.optimizer.opt_sort_by_index_02
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
    CREATE TABLE Table_100 (
      ID INTEGER NOT NULL
    );

    SET TERM ^^ ;
    CREATE PROCEDURE PR_FillTable_100
    AS
    DECLARE VARIABLE FillID INTEGER;
    BEGIN
      FillID = 1;
      WHILE (FillID <= 100) DO
      BEGIN
        INSERT INTO Table_100 (ID) VALUES (:FillID);
        FillID = FillID + 1;
      END
    END
    ^^
    SET TERM ; ^^
    COMMIT;

    EXECUTE PROCEDURE PR_FillTable_100;
    COMMIT;

    CREATE DESC INDEX PK_Table_100_DESC ON Table_100 (ID);
    COMMIT;
"""

db = db_factory(init=init_script)

qry_list = (
    """
    SELECT *
    FROM Table_100 t100
    ORDER BY t100.ID DESC
    """,
)
data_list = (
    """
    ID : 100
    ID : 99
    ID : 98
    ID : 97
    ID : 96
    ID : 95
    ID : 94
    ID : 93
    ID : 92
    ID : 91
    ID : 90
    ID : 89
    ID : 88
    ID : 87
    ID : 86
    ID : 85
    ID : 84
    ID : 83
    ID : 82
    ID : 81
    ID : 80
    ID : 79
    ID : 78
    ID : 77
    ID : 76
    ID : 75
    ID : 74
    ID : 73
    ID : 72
    ID : 71
    ID : 70
    ID : 69
    ID : 68
    ID : 67
    ID : 66
    ID : 65
    ID : 64
    ID : 63
    ID : 62
    ID : 61
    ID : 60
    ID : 59
    ID : 58
    ID : 57
    ID : 56
    ID : 55
    ID : 54
    ID : 53
    ID : 52
    ID : 51
    ID : 50
    ID : 49
    ID : 48
    ID : 47
    ID : 46
    ID : 45
    ID : 44
    ID : 43
    ID : 42
    ID : 41
    ID : 40
    ID : 39
    ID : 38
    ID : 37
    ID : 36
    ID : 35
    ID : 34
    ID : 33
    ID : 32
    ID : 31
    ID : 30
    ID : 29
    ID : 28
    ID : 27
    ID : 26
    ID : 25
    ID : 24
    ID : 23
    ID : 22
    ID : 21
    ID : 20
    ID : 19
    ID : 18
    ID : 17
    ID : 16
    ID : 15
    ID : 14
    ID : 13
    ID : 12
    ID : 11
    ID : 10
    ID : 9
    ID : 8
    ID : 7
    ID : 6
    ID : 5
    ID : 4
    ID : 3
    ID : 2
    ID : 1
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
        ....-> Table "TABLE_100" as "T100" Access By ID
        ........-> Index "PK_TABLE_100_DESC" Full Scan
        {data_list[0]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Table "TABLE_100" as "T100" Access By ID
        ........-> Index "PK_TABLE_100_DESC" Full Scan
        {data_list[0]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Table "PUBLIC"."TABLE_100" as "T100" Access By ID
        ........-> Index "PUBLIC"."PK_TABLE_100_DESC" Full Scan
        {data_list[0]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
