#coding:utf-8

"""
ID:          issue-8434
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8434
TITLE:       Fix implicitly used collations after ALTER DOMAIN
DESCRIPTION:
    Test creates two tables ('test1' and 'test2'), both are used to check non-ascii characters; results for Natural and Indexed Reads are verified.
    First table is used to check output when 'common' indices are used, second - for check partial indices (and both tables have asc and desc indices).
    Every table has text field based on UTF8-domain, initially no collation is specified for it.
    Every table has four records with same character: 1) lowercased, with accent; 2) uppercased, with accent; 3) lowercased, "normal"; 4) uppercased, "normal".
    First table stores character 'd', second - character 'l'.
    We run query against each table with WHERE-expression like '<field> = <char>' -- where <char> is in lowercased, "normal" form (i.e. 'd' or 'l').

    When tables are in 'initial state' (i.e. before any domain altering) then such query must show only ONE record.
    Then we change domain definition three times:
    1) 'alter domain ... collate unicode_ci'.
        This must cause TWO records to be shown: 'lowercased, "normal"' and 'uppercased, "normal"'. Rows with accented chartacters must NOT be shown.
    2) 'alter domain ... collate unicode_ci_ai'.
        This must cause ALL records to be shown, i.e. in "normal" form and with accent.
    3) 'alter domain', i.e. WITHOUT any 'collate' mention.
        This must return output to what it was in 'initial state': only one record must be shown.
NOTES:
    [13.02.2025] pzotov
    ::: NB :::
    Currently only UTF8 charset is checked.
    We can NOT check single-byte character sets because some of them have incompleted definitions of ACCENT-/CASE- INSENSITIVE rules.

    For example, in src/intl/collations/pw1251cyrr.h we can see than 'e' with accent characters are defined as a standalone primary code,
    not with the same primary code as 'e' without accent.
    This causes different comparison result of characters used in French, Russian and Ukrainian alphabets, namely:
        1) 'e' vs 'é' vs 'É' // U+0065; U+00E9; U+00C9
        2) 'е' vs 'ё' vs 'Ё' // U+0435; U+0451; U+0401
        3) 'г' vs 'ґ' vs 'Ґ' // U+0433; U+0491; U+0490
    Result for "1" will be true in all cases for ISO8859_1, but result for "2" and "3" will be true only when we compare accent_lower vs accent_upper.

    Explained by Adriano, letter 13.02.2025 13:44.

    Checked on 6.0.0.336.
"""
import sys
import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

sys.stdout.reconfigure(encoding='utf-8')

init_sql = """
    create domain dm_utf8 varchar(1) character set utf8;

    create table test1 (
        id int
       ,what varchar(50)
       ,f_utf8 dm_utf8
    );

    create table test2 (
        id int
       ,what varchar(50)
       ,f_utf8 dm_utf8
    );

    ----------------------
    insert into test1 (
        id
       ,what
       ,f_utf8
    )
    select 
        1, 'lowered "D" w/accent', 'ð' from rdb$database union all select
        2, 'UPPERED "D" w/accent', 'Ð' from rdb$database union all select
        3, 'lowered "D" normal',   'd' from rdb$database union all select
        4, 'UPPERED "D" normal',   'D' from rdb$database
    ;

    insert into test2 (
        id
       ,what
       ,f_utf8
    )
    select 
        1, 'lowered "L" w/accent', 'ł' from rdb$database union all select
        2, 'UPPERED "L" w/accent', 'Ł' from rdb$database union all select
        3, 'lowered "L" normal',   'l' from rdb$database union all select
        4, 'UPPERED "L" normal',   'L' from rdb$database
    ;
    commit;

    -- ...........................
    create index test1_asc on test1(f_utf8);
    create descending index test1_dec on test1(f_utf8);

    create index test2_partial_asc on test2(f_utf8) where f_utf8 = 'l';
    create index test2_partial_dec on test2(f_utf8) where f_utf8 = 'l';
    commit;

    create view v1_chk_nr as select id, what, f_utf8 from test1 where f_utf8 || '' = 'd' order by id;
    create view v1_chk_ir_asc as select id, what, f_utf8 from test1 where f_utf8 = 'd' order by f_utf8;
    create view v1_chk_ir_dec as select id, what, f_utf8 from test1 where f_utf8 = 'd' order by f_utf8 desc;

    create view v2_chk_nr as select id, what, f_utf8 from test2 where f_utf8 || '' = 'l' order by id;
    create view v2_chk_ir_asc as select id, what, f_utf8 from test2 where f_utf8 = 'l' order by f_utf8;
    create view v2_chk_ir_dec as select id, what, f_utf8 from test2 where f_utf8 = 'l' order by f_utf8 desc;
    commit;
"""

db = db_factory(init = init_sql)
act = python_act('db', substitutions = [(r'record length: \d+, key length: \d+', 'record length: NN, key length: MM')])

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=6')
def test_1(act: Action, capsys):
    qry_map = {
        1 : 'select * from v1_chk_nr'
       ,2 : 'select * from v1_chk_ir_asc'
       ,3 : 'select * from v1_chk_ir_dec'
       ,4 : 'select * from v2_chk_nr'
       ,5 : 'select * from v2_chk_ir_asc'
       ,6 : 'select * from v2_chk_ir_dec'
    }

    alter_lst = (
        ''
       ,'alter domain dm_utf8 type varchar(1) character set utf8 collate unicode_ci'
       ,'alter domain dm_utf8 type varchar(1) character set utf8 collate unicode_ci_ai'
       ,'alter domain dm_utf8 type varchar(1) character set utf8'
    )

    with act.db.connect(charset = 'utf8') as con:
        cur = con.cursor()
        for alter_i in alter_lst:
            if alter_i.strip():
                con.execute_immediate(alter_i)
                con.commit()
                print(f'\nAfter {alter_i}:')
            else:
                print('Initial state:')

            for k, v in qry_map.items():
                ps, rs = None, None
                try:
                    ps = cur.prepare(v)

                    print('Query:', v)
                    # Print explained plan with padding eash line by dots in order to see indentations:
                    print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                    print('')

                    # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                    # We have to store result of cur.execute(<psInstance>) in order to
                    # close it explicitly.
                    # Otherwise AV can occur during Python garbage collection and this
                    # causes pytest to hang on its final point.
                    # Explained by hvlad, email 26.10.24 17:42
                    rs = cur.execute(ps)
                    for r in rs:
                        print(r[0], r[1])
                except DatabaseError as e:
                    print(e.__str__())
                    print(e.gds_codes)
                finally:
                    if rs:
                        rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                    if ps:
                        ps.free()


    expected_stdout = """
        Initial state:
        Query: select * from v1_chk_nr
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_NR" "PUBLIC"."TEST1" Full Scan
        3 lowered "D" normal
        Query: select * from v1_chk_ir_asc
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_IR_ASC" "PUBLIC"."TEST1" Access By ID
        ............-> Index "PUBLIC"."TEST1_ASC" Range Scan (full match)
        3 lowered "D" normal
        Query: select * from v1_chk_ir_dec
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_IR_DEC" "PUBLIC"."TEST1" Access By ID
        ............-> Index "PUBLIC"."TEST1_DEC" Range Scan (full match)
        3 lowered "D" normal
        Query: select * from v2_chk_nr
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_NR" "PUBLIC"."TEST2" Full Scan
        3 lowered "L" normal
        Query: select * from v2_chk_ir_asc
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_IR_ASC" "PUBLIC"."TEST2" Access By ID
        ............-> Index "PUBLIC"."TEST2_PARTIAL_ASC" Full Scan
        3 lowered "L" normal
        Query: select * from v2_chk_ir_dec
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_IR_DEC" "PUBLIC"."TEST2" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST2_PARTIAL_ASC" Full Scan
        3 lowered "L" normal
        After alter domain dm_utf8 type varchar(1) character set utf8 collate unicode_ci:
        Query: select * from v1_chk_nr
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_NR" "PUBLIC"."TEST1" Full Scan
        3 lowered "D" normal
        4 UPPERED "D" normal
        Query: select * from v1_chk_ir_asc
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_IR_ASC" "PUBLIC"."TEST1" Access By ID
        ............-> Index "PUBLIC"."TEST1_ASC" Range Scan (full match)
        3 lowered "D" normal
        4 UPPERED "D" normal
        Query: select * from v1_chk_ir_dec
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_IR_DEC" "PUBLIC"."TEST1" Access By ID
        ............-> Index "PUBLIC"."TEST1_DEC" Range Scan (full match)
        4 UPPERED "D" normal
        3 lowered "D" normal
        Query: select * from v2_chk_nr
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_NR" "PUBLIC"."TEST2" Full Scan
        3 lowered "L" normal
        4 UPPERED "L" normal
        Query: select * from v2_chk_ir_asc
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_IR_ASC" "PUBLIC"."TEST2" Access By ID
        ............-> Index "PUBLIC"."TEST2_PARTIAL_ASC" Full Scan
        3 lowered "L" normal
        4 UPPERED "L" normal
        Query: select * from v2_chk_ir_dec
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_IR_DEC" "PUBLIC"."TEST2" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST2_PARTIAL_ASC" Full Scan
        4 UPPERED "L" normal
        3 lowered "L" normal
        After alter domain dm_utf8 type varchar(1) character set utf8 collate unicode_ci_ai:
        Query: select * from v1_chk_nr
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_NR" "PUBLIC"."TEST1" Full Scan
        1 lowered "D" w/accent
        2 UPPERED "D" w/accent
        3 lowered "D" normal
        4 UPPERED "D" normal
        Query: select * from v1_chk_ir_asc
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_IR_ASC" "PUBLIC"."TEST1" Access By ID
        ............-> Index "PUBLIC"."TEST1_ASC" Range Scan (full match)
        3 lowered "D" normal
        4 UPPERED "D" normal
        1 lowered "D" w/accent
        2 UPPERED "D" w/accent
        Query: select * from v1_chk_ir_dec
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_IR_DEC" "PUBLIC"."TEST1" Access By ID
        ............-> Index "PUBLIC"."TEST1_DEC" Range Scan (full match)
        2 UPPERED "D" w/accent
        1 lowered "D" w/accent
        4 UPPERED "D" normal
        3 lowered "D" normal
        Query: select * from v2_chk_nr
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_NR" "PUBLIC"."TEST2" Full Scan
        1 lowered "L" w/accent
        2 UPPERED "L" w/accent
        3 lowered "L" normal
        4 UPPERED "L" normal
        Query: select * from v2_chk_ir_asc
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_IR_ASC" "PUBLIC"."TEST2" Access By ID
        ............-> Index "PUBLIC"."TEST2_PARTIAL_ASC" Full Scan
        3 lowered "L" normal
        4 UPPERED "L" normal
        1 lowered "L" w/accent
        2 UPPERED "L" w/accent
        Query: select * from v2_chk_ir_dec
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_IR_DEC" "PUBLIC"."TEST2" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST2_PARTIAL_ASC" Full Scan
        2 UPPERED "L" w/accent
        1 lowered "L" w/accent
        4 UPPERED "L" normal
        3 lowered "L" normal
        After alter domain dm_utf8 type varchar(1) character set utf8:
        Query: select * from v1_chk_nr
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_NR" "PUBLIC"."TEST1" Full Scan
        3 lowered "D" normal
        Query: select * from v1_chk_ir_asc
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_IR_ASC" "PUBLIC"."TEST1" Access By ID
        ............-> Index "PUBLIC"."TEST1_ASC" Range Scan (full match)
        3 lowered "D" normal
        Query: select * from v1_chk_ir_dec
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST1" as "PUBLIC"."V1_CHK_IR_DEC" "PUBLIC"."TEST1" Access By ID
        ............-> Index "PUBLIC"."TEST1_DEC" Range Scan (full match)
        3 lowered "D" normal
        Query: select * from v2_chk_nr
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_NR" "PUBLIC"."TEST2" Full Scan
        3 lowered "L" normal
        Query: select * from v2_chk_ir_asc
        Select Expression
        ....-> Filter
        ........-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_IR_ASC" "PUBLIC"."TEST2" Access By ID
        ............-> Index "PUBLIC"."TEST2_PARTIAL_ASC" Full Scan
        3 lowered "L" normal
        Query: select * from v2_chk_ir_dec
        Select Expression
        ....-> Sort (record length: NN, key length: MM)
        ........-> Filter
        ............-> Table "PUBLIC"."TEST2" as "PUBLIC"."V2_CHK_IR_DEC" "PUBLIC"."TEST2" Access By ID
        ................-> Bitmap
        ....................-> Index "PUBLIC"."TEST2_PARTIAL_ASC" Full Scan
        3 lowered "L" normal
    """
    act.expected_stdout = expected_stdout

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
