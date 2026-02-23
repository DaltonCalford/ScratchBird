#coding:utf-8

"""
ID:          optimizer.left-join-05
TITLE:       LEFT OUTER JOIN with full match, but limited in ON clause
DESCRIPTION:
  TableX LEFT OUTER JOIN TableY with full match, but TableY results limited in ON clause.
  Which should result in partial NULL results.

  This test also tests if not the ON clause is distributed to the outer context TableX.
  Also if not the extra created nodes (comparisons) from a equality node and a A # B
  node (# =, <, <=, >=, >) are distributed to the outer context.
FBTEST:      functional.arno.optimizer.opt_left_join_05
NOTES:
    [08.07.2025] pzotov
    Refactored: explained plan is used to be checked in expected_out.
    Added ability to use several queries and their datasets for check - see 'qry_list' and 'qry_data' tuples.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.930; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    CREATE TABLE Colors (
      ColorID INTEGER NOT NULL,
      ColorName VARCHAR(20)
    );

    CREATE TABLE Flowers (
      FlowerID INTEGER NOT NULL,
      FlowerName VARCHAR(30),
      ColorID INTEGER
    );

    COMMIT;

    /* Value 0 represents -no value- */
    INSERT INTO Colors (ColorID, ColorName) VALUES (0, 'Not defined');
    INSERT INTO Colors (ColorID, ColorName) VALUES (1, 'Red');
    INSERT INTO Colors (ColorID, ColorName) VALUES (2, 'Yellow');

    /* insert some data with references */
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (1, 'Rose', 1);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (2, 'Tulip', 2);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (3, 'Gerbera', 0);

    COMMIT;

    /* Normally these indexes are created by the primary/foreign keys,
       but we don't want to rely on them for this test */
    CREATE UNIQUE ASC INDEX PK_Colors ON Colors (ColorID);
    CREATE UNIQUE ASC INDEX PK_Flowers ON Flowers (FlowerID);
    CREATE ASC INDEX FK_Flowers_Colors ON Flowers (ColorID);

    COMMIT;
"""

db = db_factory(sql_dialect=3, init=init_script)

qry_list = (
    """
    SELECT
      f.FlowerName,
      c.ColorName
    FROM
      Flowers f
      LEFT JOIN Colors c ON ((c.ColorID = f.ColorID) AND (c.ColorID >= 1))
    """,
)
data_list = (
    """
    FLOWERNAME : Rose
    COLORNAME : Red
    FLOWERNAME : Tulip
    COLORNAME : Yellow
    FLOWERNAME : Gerbera
    COLORNAME : None
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
        ....-> Nested Loop Join (outer)
        ........-> Table "FLOWERS" as "F" Full Scan
        ........-> Filter
        ............-> Table "COLORS" as "C" Access By ID
        ................-> Bitmap
        ....................-> Index "PK_COLORS" Unique Scan
        {data_list[0]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Nested Loop Join (outer)
        ........-> Table "FLOWERS" as "F" Full Scan
        ........-> Filter
        ............-> Table "COLORS" as "C" Access By ID
        ................-> Bitmap
        ....................-> Index "PK_COLORS" Unique Scan
        {data_list[0]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Nested Loop Join (outer)
        ........-> Table "PUBLIC"."FLOWERS" as "F" Full Scan
        ........-> Filter
        ............-> Table "PUBLIC"."COLORS" as "C" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."PK_COLORS" Unique Scan
        {data_list[0]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
