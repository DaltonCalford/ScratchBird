#coding:utf-8

"""
ID:          optimizer.full-join-04
TITLE:       FULL OUTER JOIN,  list all values, but filtered in WHERE clause
DESCRIPTION:
  TableX FULL OUTER JOIN TableY with relation in the ON clause.
  Three tables are used, where 1 table (RC) holds references to the two other tables (R and C).
  The two tables R and C contain both 1 value that isn't inside RC.
  =====
  NB: 'UNION ALL' is used here, so PLAN for 2.5 will be of TWO separate rows.
NOTES:
    [01.08.2023] pzotov
        Adjusted plan to actual for FB 5.x after letter from dimitr.
    [07.07.2025] pzotov
        Refactored: explained plan is used to be checked in expected_out.
        Added ability to use several queries and their datasets for check - see 'qry_list' and 'qry_data' tuples.
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.914; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
    [20.02.2026] pzotov
        Adjusted expected output to actual: FULL JOIN execution plan has changed since 19.02.2026 6.0.0.1458,
        commit: "6a76c1da Better index usage in full outer joins...". Discussed with dimitr, 20.02.2026 1723.
        Checked on 6.0.0.1461-5e98812.
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_script = """
    create table relations (
      relationid integer,
      relationname varchar(35)
    );

    create table categories (
      categoryid integer,
      description varchar(20)
    );

    create table relationcategories (
      relationid integer,
      categoryid integer
    );

    commit;

    insert into relations (relationid, relationname) values (1, 'diving snorkel shop');
    insert into relations (relationid, relationname) values (2, 'bakery garbage');
    insert into relations (relationid, relationname) values (3, 'racing turtle');
    insert into relations (relationid, relationname) values (4, 'folding air-hook shop');

    insert into categories (categoryid, description) values (1, 'relation');
    insert into categories (categoryid, description) values (2, 'debtor');
    insert into categories (categoryid, description) values (3, 'creditor');
    insert into categories (categoryid, description) values (4, 'newsletter');

    insert into relationcategories (relationid, categoryid) values (1, 1);
    insert into relationcategories (relationid, categoryid) values (2, 1);
    insert into relationcategories (relationid, categoryid) values (3, 1);
    insert into relationcategories (relationid, categoryid) values (1, 2);
    insert into relationcategories (relationid, categoryid) values (2, 2);
    insert into relationcategories (relationid, categoryid) values (1, 3);

    commit;

    -- normally these indexes are created by the primary/foreign keys,
    -- but we don't want to rely on them for this test
    create unique asc index pk_relations on relations (relationid);
    create unique asc index pk_categories on categories (categoryid);
    create unique asc index pk_relationcategories on relationcategories (relationid, categoryid);
    create asc index fk_rc_relations on relationcategories (relationid);
    create asc index fk_rc_categories on relationcategories (categoryid);

    commit;
"""

db = db_factory(init=init_script)

qry_list = (
    """
    select
        r.relationname,
        rc.relationid,
        rc.categoryid,
        c.description
    from
        relations r
        full join relationcategories rc on (rc.relationid = r.relationid)
        full join categories c on (c.categoryid = rc.categoryid)
    where
        rc.categoryid is null and c.categoryid >= 1

    UNION ALL --- :::::::   U N I O N    A L L  :::::::

    select
        r.relationname,
        rc.relationid,
        rc.categoryid,
        c.description
    from
        relations r
        full join relationcategories rc on (rc.relationid = r.relationid)
        full join categories c on (c.categoryid = rc.categoryid)
    where
        rc.relationid is null and r.relationid >= 1
    """,
)
data_list = (
    """
    RELATIONNAME : None
    RELATIONID : None
    CATEGORYID : None
    DESCRIPTION : newsletter
    RELATIONNAME : folding air-hook shop
    RELATIONID : None
    CATEGORYID : None
    DESCRIPTION : None
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
        ....-> Union
        ........-> Filter
        ............-> Full Outer Join
        ................-> Nested Loop Join (outer)
        ....................-> Filter
        ........................-> Table "CATEGORIES" as "C" Access By ID
        ............................-> Bitmap
        ................................-> Index "PK_CATEGORIES" Range Scan (lower bound: 1/1)
        ....................-> Filter
        ........................-> Full Outer Join
        ............................-> Nested Loop Join (outer)
        ................................-> Table "RELATIONCATEGORIES" as "RC" Full Scan
        ................................-> Filter
        ....................................-> Table "RELATIONS" as "R" Access By ID
        ........................................-> Bitmap
        ............................................-> Index "PK_RELATIONS" Unique Scan
        ............................-> Nested Loop Join (anti)
        ................................-> Table "RELATIONS" as "R" Full Scan
        ................................-> Filter
        ....................................-> Table "RELATIONCATEGORIES" as "RC" Access By ID
        ........................................-> Bitmap
        ............................................-> Index "FK_RC_RELATIONS" Range Scan (full match)
        ................-> Nested Loop Join (anti)
        ....................-> Full Outer Join
        ........................-> Nested Loop Join (outer)
        ............................-> Table "RELATIONCATEGORIES" as "RC" Full Scan
        ............................-> Filter
        ................................-> Table "RELATIONS" as "R" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "PK_RELATIONS" Unique Scan
        ........................-> Nested Loop Join (anti)
        ............................-> Table "RELATIONS" as "R" Full Scan
        ............................-> Filter
        ................................-> Table "RELATIONCATEGORIES" as "RC" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "FK_RC_RELATIONS" Range Scan (full match)
        ....................-> Filter
        ........................-> Filter
        ............................-> Table "CATEGORIES" as "C" Access By ID
        ................................-> Bitmap
        ....................................-> Index "PK_CATEGORIES" Range Scan (lower bound: 1/1)
        ........-> Filter
        ............-> Full Outer Join
        ................-> Nested Loop Join (outer)
        ....................-> Table "CATEGORIES" as "C" Full Scan
        ....................-> Filter
        ........................-> Full Outer Join
        ............................-> Nested Loop Join (outer)
        ................................-> Table "RELATIONCATEGORIES" as "RC" Full Scan
        ................................-> Filter
        ....................................-> Table "RELATIONS" as "R" Access By ID
        ........................................-> Bitmap
        ............................................-> Index "PK_RELATIONS" Unique Scan
        ............................-> Nested Loop Join (anti)
        ................................-> Filter
        ....................................-> Table "RELATIONS" as "R" Access By ID
        ........................................-> Bitmap
        ............................................-> Index "PK_RELATIONS" Range Scan (lower bound: 1/1)
        ................................-> Filter
        ....................................-> Table "RELATIONCATEGORIES" as "RC" Access By ID
        ........................................-> Bitmap
        ............................................-> Index "FK_RC_RELATIONS" Range Scan (full match)
        ................-> Nested Loop Join (anti)
        ....................-> Full Outer Join
        ........................-> Nested Loop Join (outer)
        ............................-> Table "RELATIONCATEGORIES" as "RC" Full Scan
        ............................-> Filter
        ................................-> Table "RELATIONS" as "R" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "PK_RELATIONS" Unique Scan
        ........................-> Nested Loop Join (anti)
        ............................-> Filter
        ................................-> Table "RELATIONS" as "R" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "PK_RELATIONS" Range Scan (lower bound: 1/1)
        ............................-> Filter
        ................................-> Table "RELATIONCATEGORIES" as "RC" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "FK_RC_RELATIONS" Range Scan (full match)
        ....................-> Filter
        ........................-> Table "CATEGORIES" as "C" Full Scan
        {data_list[0]}
    """

    expected_out_5x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Union
        ........-> Filter
        ............-> Nested Loop Join (outer)
        ................-> Filter
        ....................-> Table "CATEGORIES" as "C" Access By ID
        ........................-> Bitmap
        ............................-> Index "PK_CATEGORIES" Range Scan (lower bound: 1/1)
        ................-> Filter
        ....................-> Full Outer Join
        ........................-> Nested Loop Join (outer)
        ............................-> Filter
        ................................-> Table "RELATIONCATEGORIES" as "RC" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "FK_RC_CATEGORIES" Range Scan (full match)
        ............................-> Filter
        ................................-> Table "RELATIONS" as "R" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "PK_RELATIONS" Unique Scan
        ........................-> Nested Loop Join (outer)
        ............................-> Table "RELATIONS" as "R" Full Scan
        ............................-> Filter
        ................................-> Table "RELATIONCATEGORIES" as "RC" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "PK_RELATIONCATEGORIES" Unique Scan
        ........-> Filter
        ............-> Nested Loop Join (outer)
        ................-> Filter
        ....................-> Nested Loop Join (outer)
        ........................-> Filter
        ............................-> Table "RELATIONS" as "R" Access By ID
        ................................-> Bitmap
        ....................................-> Index "PK_RELATIONS" Range Scan (lower bound: 1/1)
        ........................-> Filter
        ............................-> Table "RELATIONCATEGORIES" as "RC" Access By ID
        ................................-> Bitmap
        ....................................-> Index "FK_RC_RELATIONS" Range Scan (full match)
        ................-> Filter
        ....................-> Table "CATEGORIES" as "C" Access By ID
        ........................-> Bitmap
        ............................-> Index "PK_CATEGORIES" Unique Scan
        {data_list[0]}
    """

    expected_out_6x = f"""
        {qry_list[0]}
        Select Expression
        ....-> Union
        ........-> Filter
        ............-> Nested Loop Join (outer)
        ................-> Filter
        ....................-> Table "PUBLIC"."CATEGORIES" as "C" Access By ID
        ........................-> Bitmap
        ............................-> Index "PUBLIC"."PK_CATEGORIES" Range Scan (lower bound: 1/1)
        ................-> Filter
        ....................-> Full Outer Join
        ........................-> Nested Loop Join (outer)
        ............................-> Table "PUBLIC"."RELATIONS" as "R" Full Scan
        ............................-> Filter
        ................................-> Table "PUBLIC"."RELATIONCATEGORIES" as "RC" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "PUBLIC"."PK_RELATIONCATEGORIES" Unique Scan
        ........................-> Nested Loop Join (outer)
        ............................-> Filter
        ................................-> Table "PUBLIC"."RELATIONCATEGORIES" as "RC" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "PUBLIC"."FK_RC_CATEGORIES" Range Scan (full match)
        ............................-> Filter
        ................................-> Table "PUBLIC"."RELATIONS" as "R" Access By ID
        ....................................-> Bitmap
        ........................................-> Index "PUBLIC"."PK_RELATIONS" Unique Scan
        ........-> Filter
        ............-> Nested Loop Join (outer)
        ................-> Filter
        ....................-> Nested Loop Join (outer)
        ........................-> Filter
        ............................-> Table "PUBLIC"."RELATIONS" as "R" Access By ID
        ................................-> Bitmap
        ....................................-> Index "PUBLIC"."PK_RELATIONS" Range Scan (lower bound: 1/1)
        ........................-> Filter
        ............................-> Table "PUBLIC"."RELATIONCATEGORIES" as "RC" Access By ID
        ................................-> Bitmap
        ....................................-> Index "PUBLIC"."FK_RC_RELATIONS" Range Scan (full match)
        ................-> Filter
        ....................-> Table "PUBLIC"."CATEGORIES" as "C" Access By ID
        ........................-> Bitmap
        ............................-> Index "PUBLIC"."PK_CATEGORIES" Unique Scan
        {data_list[0]}
    """

    act.expected_stdout = expected_out_4x if act.is_version('<5') else expected_out_5x if act.is_version('<6') else expected_out_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
