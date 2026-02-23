#coding:utf-8

"""
ID:          issue-95442bdf
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/95442bdfff76d22aafb57b58894047be2a89c6ea
TITLE:       Attempt to avoid hash joining for possible cardinality under-estimations
DESCRIPTION:
    Test verifies explained plan for three forms of inner join:
        * 'normal' (or 'traditional'): 'from A join B on <expr>'
        * 'using':   'from A join B using (<field>)
        * 'natural': 'from A natural join B'
    All forms must generate same plan with nested loops (i.e. without hash join).
    Lines in each explained plan are LETF-PADDED with dot character in order to keep indentation while
    analyzing differences between expected and actual output.
NOTES:
    [29.05.2024] pzotov
    Checked on intermediate snapshot 6.0.0.363 #95442bd.
    Thanks to dimitr for provided example.
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_sql = f"""
    create table t1(id int);
    create table t2(id int primary key using index t2_pk);
    insert into t1(id) select row_number()over() from rdb$types,rdb$types;
    commit;
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

#---------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#---------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    join_expr_lst = (
         't1 a join t2 b on a.id = b.id'
        ,'t1 u join t2 v using(id)'
        ,'t1 x natural join t2 y'
    )

    with act.db.connect() as con:
        cur = con.cursor()
        for x in join_expr_lst:
            ps = None
            try:
                ps = cur.prepare(f'select * from ' + x)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan .split('\n')]) )
            except DatabaseError as e:
                print( e.__str__() )
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()
   
    act.expected_stdout = """
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Table "T1" as "A" Full Scan
        ........-> Filter
        ............-> Table "T2" as "B" Access By ID
        ................-> Bitmap
        ....................-> Index "T2_PK" Unique Scan

        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Table "T1" as "U" Full Scan
        ........-> Filter
        ............-> Table "T2" as "V" Access By ID
        ................-> Bitmap
        ....................-> Index "T2_PK" Unique Scan

        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Table "T1" as "X" Full Scan
        ........-> Filter
        ............-> Table "T2" as "Y" Access By ID
        ................-> Bitmap
        ....................-> Index "T2_PK" Unique Scan
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
