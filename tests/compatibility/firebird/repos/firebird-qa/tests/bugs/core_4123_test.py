#coding:utf-8

"""
ID:          issue-4451
ISSUE:       4451
TITLE:       Firebird crash when executing an stored procedure called by a trigger that converts string to upper
DESCRIPTION:
JIRA:        CORE-4123
FBTEST:      bugs.core_4123
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()
script_file = temp_file('test-script.sql')

act = python_act('db')

test_script = """
    set list on;
    with recursive
    d as (
        select s, char_length(s) n
        from (
            -- http://www.ic.unicamp.br/~stolfi/EXPORT/www/ISO-8859-1-Encoding.html
            -- ¡¢£¤¥¦§¨©ª«¬®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ
            select 'ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ' as s
            from rdb$database
        )
    )
    ,r as(select 1 as i from rdb$database union all select r.i+1 from r where r.i<128)
    ,c as(
        select
            substring(d.s from r.i for 1) as char_as_is
            ,upper( substring(d.s from r.i for 1) ) char_upper
            ,lower( substring(d.s from r.i for 1) ) char_lower
        from d, r
        where r.i <= d.n
    )
    select c.char_as_is, char_upper, char_lower, iif(char_upper = char_lower, '1',' ') is_upper_equ_lower
    from c
    order by 1
    ;
"""

expected_stdout = """
    CHAR_AS_IS                      À
    CHAR_UPPER                      À
    CHAR_LOWER                      à
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Á
    CHAR_UPPER                      Á
    CHAR_LOWER                      á
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Â
    CHAR_UPPER                      Â
    CHAR_LOWER                      â
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ã
    CHAR_UPPER                      Ã
    CHAR_LOWER                      ã
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ä
    CHAR_UPPER                      Ä
    CHAR_LOWER                      ä
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Å
    CHAR_UPPER                      Å
    CHAR_LOWER                      å
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Æ
    CHAR_UPPER                      Æ
    CHAR_LOWER                      æ
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ç
    CHAR_UPPER                      Ç
    CHAR_LOWER                      ç
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      È
    CHAR_UPPER                      È
    CHAR_LOWER                      è
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      É
    CHAR_UPPER                      É
    CHAR_LOWER                      é
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ê
    CHAR_UPPER                      Ê
    CHAR_LOWER                      ê
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ë
    CHAR_UPPER                      Ë
    CHAR_LOWER                      ë
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ì
    CHAR_UPPER                      Ì
    CHAR_LOWER                      ì
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Í
    CHAR_UPPER                      Í
    CHAR_LOWER                      í
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Î
    CHAR_UPPER                      Î
    CHAR_LOWER                      î
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ï
    CHAR_UPPER                      Ï
    CHAR_LOWER                      ï
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ð
    CHAR_UPPER                      Ð
    CHAR_LOWER                      ð
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ñ
    CHAR_UPPER                      Ñ
    CHAR_LOWER                      ñ
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ò
    CHAR_UPPER                      Ò
    CHAR_LOWER                      ò
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ó
    CHAR_UPPER                      Ó
    CHAR_LOWER                      ó
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ô
    CHAR_UPPER                      Ô
    CHAR_LOWER                      ô
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Õ
    CHAR_UPPER                      Õ
    CHAR_LOWER                      õ
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ö
    CHAR_UPPER                      Ö
    CHAR_LOWER                      ö
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ×
    CHAR_UPPER                      ×
    CHAR_LOWER                      ×
    IS_UPPER_EQU_LOWER              1

    CHAR_AS_IS                      Ø
    CHAR_UPPER                      Ø
    CHAR_LOWER                      ø
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ù
    CHAR_UPPER                      Ù
    CHAR_LOWER                      ù
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ú
    CHAR_UPPER                      Ú
    CHAR_LOWER                      ú
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Û
    CHAR_UPPER                      Û
    CHAR_LOWER                      û
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ü
    CHAR_UPPER                      Ü
    CHAR_LOWER                      ü
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Ý
    CHAR_UPPER                      Ý
    CHAR_LOWER                      ý
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      Þ
    CHAR_UPPER                      Þ
    CHAR_LOWER                      þ
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ß
    CHAR_UPPER                      ß
    CHAR_LOWER                      ß
    IS_UPPER_EQU_LOWER              1

    CHAR_AS_IS                      à
    CHAR_UPPER                      À
    CHAR_LOWER                      à
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      á
    CHAR_UPPER                      Á
    CHAR_LOWER                      á
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      â
    CHAR_UPPER                      Â
    CHAR_LOWER                      â
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ã
    CHAR_UPPER                      Ã
    CHAR_LOWER                      ã
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ä
    CHAR_UPPER                      Ä
    CHAR_LOWER                      ä
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      å
    CHAR_UPPER                      Å
    CHAR_LOWER                      å
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      æ
    CHAR_UPPER                      Æ
    CHAR_LOWER                      æ
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ç
    CHAR_UPPER                      Ç
    CHAR_LOWER                      ç
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      è
    CHAR_UPPER                      È
    CHAR_LOWER                      è
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      é
    CHAR_UPPER                      É
    CHAR_LOWER                      é
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ê
    CHAR_UPPER                      Ê
    CHAR_LOWER                      ê
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ë
    CHAR_UPPER                      Ë
    CHAR_LOWER                      ë
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ì
    CHAR_UPPER                      Ì
    CHAR_LOWER                      ì
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      í
    CHAR_UPPER                      Í
    CHAR_LOWER                      í
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      î
    CHAR_UPPER                      Î
    CHAR_LOWER                      î
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ï
    CHAR_UPPER                      Ï
    CHAR_LOWER                      ï
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ð
    CHAR_UPPER                      Ð
    CHAR_LOWER                      ð
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ñ
    CHAR_UPPER                      Ñ
    CHAR_LOWER                      ñ
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ò
    CHAR_UPPER                      Ò
    CHAR_LOWER                      ò
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ó
    CHAR_UPPER                      Ó
    CHAR_LOWER                      ó
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ô
    CHAR_UPPER                      Ô
    CHAR_LOWER                      ô
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      õ
    CHAR_UPPER                      Õ
    CHAR_LOWER                      õ
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ö
    CHAR_UPPER                      Ö
    CHAR_LOWER                      ö
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ÷
    CHAR_UPPER                      ÷
    CHAR_LOWER                      ÷
    IS_UPPER_EQU_LOWER              1

    CHAR_AS_IS                      ø
    CHAR_UPPER                      Ø
    CHAR_LOWER                      ø
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ù
    CHAR_UPPER                      Ù
    CHAR_LOWER                      ù
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ú
    CHAR_UPPER                      Ú
    CHAR_LOWER                      ú
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      û
    CHAR_UPPER                      Û
    CHAR_LOWER                      û
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ü
    CHAR_UPPER                      Ü
    CHAR_LOWER                      ü
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ý
    CHAR_UPPER                      Ý
    CHAR_LOWER                      ý
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      þ
    CHAR_UPPER                      Þ
    CHAR_LOWER                      þ
    IS_UPPER_EQU_LOWER

    CHAR_AS_IS                      ÿ
    CHAR_UPPER                      ÿ
    CHAR_LOWER                      ÿ
    IS_UPPER_EQU_LOWER              1
"""

@pytest.mark.version('>=3')
def test_1(act: Action, script_file: Path):
    script_file.write_text(test_script, encoding='iso-8859-1')
    act.expected_stdout = expected_stdout
    act.isql(switches=['-b', '-q'], input_file=script_file, charset='ISO8859_1')
    assert act.clean_stdout == act.clean_expected_stdout


