#coding:utf-8

"""
ID:          issue-6407
ISSUE:       6407
TITLE:       substring similar - extra characters in the result for non latin characters
DESCRIPTION:
JIRA:        CORE-6158
FBTEST:      bugs.core_6158
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory(charset='WIN1251')

act = python_act('db', substitutions=[('RESULT_3_BLOB_ID.*', '')])

expected_stdout = """
	RESULT1                         = Комментарий между символами "равно" =
	RESULT2                         - Комментарий между символами "дефис" -
	Российский рубль; Турецкая лира; Доллар США
"""

script_file = temp_file('test-script.sql')

pattern_01 = '%/#*(=){3,}#"%#"(=){3,}#*/%'
pattern_02 = '%/#*(#-){3,}#"%#"(#-){3,}#*/%'

test_script = f"""
    -- This is needed to get "cannot transliterate character between character sets"
    --	on build 4.0.0.1631, see comment in the tracker 18/Oct/19 02:57 PM:
    create domain T_A64 as varchar (64) character set WIN1251 collate WIN1251;
    create table VALUT_LIST (NAME T_A64 not null);
    commit;
    insert into VALUT_LIST (NAME) values ('Российский рубль');
    insert into VALUT_LIST (NAME) values ('Турецкая лира');
    insert into VALUT_LIST (NAME) values ('Доллар США');
    commit;

    set list on;
    set blob all;

    select substring('
    aaa
    /*==== Комментарий между символами "равно" ====*/
    bbb
    ccc
    ddd
    eee
    fff
    jjj
    ' similar '{pattern_01}' escape '#') as result1
    from rdb$database;

    -- additional check for special character '-' as delimiter:
    select substring('здесь написан /*---- Комментарий между символами "дефис" ----*/ - и больше ничего!' similar '{pattern_02}' escape '#') as result2
    from rdb$database;

    -- Confirmed fail on 4.0.0.1631 with "cannot transliterate character between character sets":
    select substring(list(T.NAME, '; ') from 1 for 250) as result_3_blob_id from VALUT_LIST T;
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, script_file: Path):
    script_file.write_text(test_script, encoding='cp1251')
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input_file=script_file, charset='win1251')
    assert act.clean_stdout == act.clean_expected_stdout
