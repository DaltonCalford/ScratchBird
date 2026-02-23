#coding:utf-8

"""
ID:          optimizer.aggregate-distribution-13
TITLE:       Try to deliver HAVING CLAUSE conjunctions to the WHERE clause
DESCRIPTION:
  Comparisons which doesn't contain (anywhere hiding in the expression) aggregate-functions
  should be delivered to the where clause. The underlying aggregate stream could possible
  use it for a index and speed it up.
FBTEST:      functional.arno.optimizer.opt_aggregate_distribution_13
NOTES:
    [07.07.2025] pzotov
    Refactored: explained plan is used to be checked in expected_out.
    Added ability to use several queries and their datasets for check - see 'qry_list' and 'qry_data' tuples.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.914; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
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
    INSERT INTO Colors (ColorID, ColorName) VALUES (2, 'White');
    INSERT INTO Colors (ColorID, ColorName) VALUES (3, 'Blue');
    INSERT INTO Colors (ColorID, ColorName) VALUES (4, 'Yellow');
    INSERT INTO Colors (ColorID, ColorName) VALUES (5, 'Black');
    INSERT INTO Colors (ColorID, ColorName) VALUES (6, 'Purple');

    /* insert some data with references */
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (1, 'Red Rose', 1);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (2, 'White Rose', 2);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (3, 'Blue Rose', 3);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (4, 'Yellow Rose', 4);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (5, 'Black Rose', 5);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (6, 'Red Tulip', 1);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (7, 'White Tulip', 2);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (8, 'Yellow Tulip', 4);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (9, 'Blue Gerbera', 3);
    INSERT INTO Flowers (FlowerID, FlowerName, ColorID) VALUES (10, 'Purple Gerbera', 6);

    COMMIT;

    /* Normally these indexes are created by the primary/foreign keys,
       but we don't want to rely on them for this test */
    CREATE UNIQUE ASC INDEX PK_Colors ON Colors (ColorID);
    CREATE UNIQUE ASC INDEX PK_Flowers ON Flowers (FlowerID);
    CREATE ASC INDEX FK_Flowers_Colors ON Flowers (ColorID);
    CREATE ASC INDEX I_Colors_ColorName ON Colors (ColorName);

    COMMIT;
"""

db = db_factory(init=init_script)

qry_list = (
    """
    select
        f.colorid,
        c.colorname,
        count(*)
    from colors c
    left join flowers f on (f.colorid = c.colorid)
    group by f.colorid, c.colorname
    having c.colorname starting with 'B'
    """,
)

data_list = (
    """
    COLORID : 3
    COLORNAME : Blue
    COUNT : 2
    COLORID : 5
    COLORNAME : Black
    COUNT : 1
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
        ........-> Aggregate
        ............-> Sort record length: N, key length: M
        ................-> Nested Loop Join (outer)
        ....................-> Filter
        ........................-> Table "COLORS" as "C" Access By ID
        ............................-> Bitmap
        ................................-> Index "I_COLORS_COLORNAME" Range Scan (full match)
        ....................-> Filter
        ........................-> Table "FLOWERS" as "F" Access By ID
        ............................-> Bitmap
        ................................-> Index "FK_FLOWERS_COLORS" Range Scan (full match)
        {data_list[0]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Filter
        ........-> Aggregate
        ............-> Sort record length: N, key length: M
        ................-> Filter
        ....................-> Nested Loop Join (outer)
        ........................-> Filter
        ............................-> Table "COLORS" as "C" Access By ID
        ................................-> Bitmap
        ....................................-> Index "I_COLORS_COLORNAME" Range Scan (full match)
        ........................-> Filter
        ............................-> Table "FLOWERS" as "F" Access By ID
        ................................-> Bitmap
        ....................................-> Index "FK_FLOWERS_COLORS" Range Scan (full match)
        {data_list[0]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Filter
        ........-> Aggregate
        ............-> Sort record length: N, key length: M
        ................-> Filter
        ....................-> Nested Loop Join (outer)
        ........................-> Filter
        ............................-> Table "PUBLIC"."COLORS" as "C" Access By ID
        ................................-> Bitmap
        ....................................-> Index "PUBLIC"."I_COLORS_COLORNAME" Range Scan (full match)
        ........................-> Filter
        ............................-> Table "PUBLIC"."FLOWERS" as "F" Access By ID
        ................................-> Bitmap
        ....................................-> Index "PUBLIC"."FK_FLOWERS_COLORS" Range Scan (full match)
        {data_list[0]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
