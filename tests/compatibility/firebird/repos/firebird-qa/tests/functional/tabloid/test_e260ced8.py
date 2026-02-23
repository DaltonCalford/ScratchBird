#coding:utf-8

"""
ID:          issue-e260ced8
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/5df6668c7bf5a4b27e15f687f8c6cc40e260ced8
TITLE:       Allow computable but non-invariant lists to be used for index lookup
DESCRIPTION:
NOTES:
    [08.09.2023] pzotov
    Before improvement explained plan was:
    =======
        Sub-query
            -> Filter
                -> Table "T1" as "B" Access By ID
                    -> Bitmap Or
                        -> Bitmap Or
                            -> Bitmap
                                -> Index "T1_X" Range Scan (full match)
                            -> Bitmap
                                -> Index "T1_X" Range Scan (full match)
                        -> Bitmap
                            -> Index "T1_X" Range Scan (full match)
        Select Expression
            -> Filter
                -> Table "T1" as "A" Full Scan
    =======
    After improvement only one (single) line for index "T1_X" must present in explained plan,
    and access method must be: Index "T1_X" Full Scan.
    Thanks to dimitr for suggestion, see letter 08-sep-2023 09:47.
    
    Test shows explained plan so that leading spaces are replaced in every its line with '#'
    (it is desirable to leading indents).

    Checked on 5.0.0.1190.

    [04.02.2025] pzotov
    Adjusted execution plan for EXISTS() part of recursive query: "List Scan" was replaced with "Range Scan" for 
    "where b.x in (a.x, 2*a.x, 3*a.x)". This change caused by commit 0cc77c89 ("Fix #8109: Plan/Performance regression ...")
    Checked on 6.0.0.607-1985b88, 5.0.2.1610-5e63ad0
"""

from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_sql = """
    recreate table t1(
       id int primary key,
       x int
    );
    insert into t1(id, x) select row_number()over(), mod(row_number()over(), 19) from rdb$types;
    commit;
    create index t1_x on t1(x);
"""
db = db_factory(init = init_sql)

substitutions = []

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = python_act('db', substitutions = substitutions)


#------------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#------------------------------------------------------------

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    test_sql = """
        select a.x
        from t1 a
        where exists(
            select *
            from t1 b
            -- 04-feb-2025: bitmap_Or for three values will be used here since commit
            -- 0cc77c89 ('Fix #8109: Plan/Performance regression ...')
            -- Before this change plan has: 'Index "T1_X" List Scan (full match)'
            where b.x in (a.x, 2*a.x, 3*a.x)
        )
    """

    act.expected_stdout = """
        Sub-query
        ....-> Filter
        ........-> Table "T1" as "B" Access By ID
        ............-> Bitmap Or
        ................-> Bitmap Or
        ....................-> Bitmap
        ........................-> Index "T1_X" Range Scan (full match)
        ....................-> Bitmap
        ........................-> Index "T1_X" Range Scan (full match)
        ................-> Bitmap
        ....................-> Index "T1_X" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Table "T1" as "A" Full Scan
    """
    with act.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare(test_sql)
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan .split('\n')]) )
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
