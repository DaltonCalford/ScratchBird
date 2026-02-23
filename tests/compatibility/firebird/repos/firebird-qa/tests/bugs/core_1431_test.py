#coding:utf-8

"""
ID:          issue-1849
ISSUE:       1849
TITLE:       Greek characters in cp1251 vs uppercasing
DESCRIPTION:
JIRA:        CORE-1431
FBTEST:      bugs.core_1431
"""

import pytest
from firebird.qa import *

init_script = """
	recreate table c1251(c char(1) character set win1251, id int, descr varchar(80) );
	commit;

	-- http://www.columbia.edu/kermit/cp1251.html
    --char dec col/row oct hex  description
    insert into c1251( c, id, descr ) values('Ђ', 128, 'CYRILLIC CAPITAL LETTER DJE');
    insert into c1251( c, id, descr ) values('Ѓ', 129, 'CYRILLIC CAPITAL LETTER GJE');
    insert into c1251( c, id, descr ) values('‚', 130, 'LOW 9 SINGLE QUOTE');
    insert into c1251( c, id, descr ) values('ѓ', 131, 'CYRILLIC SMALL LETTER GJE');
    insert into c1251( c, id, descr ) values('„', 132, 'LOW 9 DOUBLE QUOTE');
    insert into c1251( c, id, descr ) values('…', 133, 'ELLIPSIS');
    insert into c1251( c, id, descr ) values('†', 134, 'DAGGER');
    insert into c1251( c, id, descr ) values('‡', 135, 'DOUBLE DAGGER');
    insert into c1251( c, id, descr ) values('€', 136, 'EURO SIGN');
    insert into c1251( c, id, descr ) values('‰', 137, 'PER MIL SIGN');
    insert into c1251( c, id, descr ) values('Љ', 138, 'CYRILLIC CAPITAL LETTER LJE');
    insert into c1251( c, id, descr ) values('‹', 139, 'LEFT SINGLE QUOTE BRACKET');
    insert into c1251( c, id, descr ) values('Њ', 140, 'CYRILLIC CAPITAL LETTER NJE');
    insert into c1251( c, id, descr ) values('Ќ', 141, 'CYRILLIC CAPITAL LETTER KJE');
    insert into c1251( c, id, descr ) values('Ћ', 142, 'CYRILLIC CAPITAL LETTER TSHE');
    insert into c1251( c, id, descr ) values('Џ', 143, 'CYRILLIC CAPITAL LETTER DZHE');
    insert into c1251( c, id, descr ) values('ђ', 144, 'CYRILLIC SMALL LETTER DJE');
    insert into c1251( c, id, descr ) values('‘', 145, 'HIGH 6 SINGLE QUOTE');
    insert into c1251( c, id, descr ) values('’', 146, 'HIGH 9 SINGLE QUOTE');
    insert into c1251( c, id, descr ) values('“', 147, 'HIGH 6 DOUBLE QUOTE');
    insert into c1251( c, id, descr ) values('”', 148, 'HIGH 9 DOUBLE QUOTE');
    insert into c1251( c, id, descr ) values('•', 149, 'LARGE CENTERED DOT');
    insert into c1251( c, id, descr ) values('–', 150, 'EN DASH');
    insert into c1251( c, id, descr ) values('—', 151, 'EM DASH');
    insert into c1251( c, id, descr ) values('™', 153, 'TRADEMARK SIGN');
    insert into c1251( c, id, descr ) values('љ', 154, 'CYRILLIC SMALL LETTER LJE');
    insert into c1251( c, id, descr ) values('›', 155, 'RIGHT SINGLE QUOTE BRACKET');
    insert into c1251( c, id, descr ) values('њ', 156, 'CYRILLIC SMALL LETTER NJE');
    insert into c1251( c, id, descr ) values('ќ', 157, 'CYRILLIC SMALL LETTER KJE');
    insert into c1251( c, id, descr ) values('ћ', 158, 'CYRILLIC CAPITAL LETTER TSHE');
    insert into c1251( c, id, descr ) values('џ', 159, 'CYRILLIC CAPITAL LETTER DZHE');
    --insert into c1251( c, id, descr ) values(', ', 160, 'NO-BREAK SPACE');
    insert into c1251( c, id, descr ) values('Ў', 161, 'CYRILLIC CAPITAL LETTER SHORT U');
    insert into c1251( c, id, descr ) values('ў', 162, 'CYRILLIC SMALL LETTER SHORT U');
    insert into c1251( c, id, descr ) values('Ј', 163, 'CYRILLIC CAPITAL LETTER JE');
    insert into c1251( c, id, descr ) values('¤', 164, 'CURRENCY SIGN');
    insert into c1251( c, id, descr ) values('Ґ', 165, 'CYRILLIC CAPITAL LETTER GHE WITH UPTURN');
    insert into c1251( c, id, descr ) values('¦', 166, 'BROKEN BAR');
    insert into c1251( c, id, descr ) values('§', 167, 'PARAGRAPH SIGN');
    insert into c1251( c, id, descr ) values('Ё', 168, 'CYRILLIC CAPITAL LETTER IO');
    insert into c1251( c, id, descr ) values('©', 169, 'COPYRIGHT SIGN');
    insert into c1251( c, id, descr ) values('Є', 170, 'CYRILLIC CAPITAL LETTER UKRAINIAN IE');
    insert into c1251( c, id, descr ) values('«', 171, 'LEFT ANGLE QUOTATION MARK');
    insert into c1251( c, id, descr ) values('¬', 172, 'NOT SIGN');

    -- insert into c1251( c, id, descr ) values('[SKIPPED! CAN NOT BE INTERPRETED ON LINUX!]', 173, 'SOFT HYPHEN');

    insert into c1251( c, id, descr ) values('®', 174, 'REGISTERED TRADE MARK SIGN');
    insert into c1251( c, id, descr ) values('Ї', 175, 'CYRILLIC CAPITAL LETTER YI');
    insert into c1251( c, id, descr ) values('°', 176, 'DEGREE SIGN, RING ABOVE');
    insert into c1251( c, id, descr ) values('±', 177, 'PLUS-MINUS SIGN');
    insert into c1251( c, id, descr ) values('І', 178, 'CYRILLIC CAPITAL LETTER BYELORUSSION-UKRAINIAN I');
    insert into c1251( c, id, descr ) values('і', 179, 'CYRILLIC SMALL LETTER BYELORUSSION-UKRAINIAN I');
    insert into c1251( c, id, descr ) values('ґ', 180, 'CYRILLIC SMALL LETTER GHE WITH UPTURN');
    insert into c1251( c, id, descr ) values('µ', 181, 'MICRO SIGN');
    insert into c1251( c, id, descr ) values('¶', 182, 'PILCROW SIGN');
    insert into c1251( c, id, descr ) values('·', 183, 'MIDDLE DOT');
    insert into c1251( c, id, descr ) values('ё', 184, 'CYRILLIC SMALL LETTER IO');
    insert into c1251( c, id, descr ) values('№', 185, 'NUMERO SIGN');
    insert into c1251( c, id, descr ) values('є', 186, 'CYRILLIC SMALL LETTER UKRAINIAN IE');
    insert into c1251( c, id, descr ) values('»', 187, 'RIGHT ANGLE QUOTATION MARK');
    insert into c1251( c, id, descr ) values('ј', 188, 'CYRILLIC SMALL LETTER JE');
    insert into c1251( c, id, descr ) values('Ѕ', 189, 'CYRILLIC CAPITAL LETTER DZE');
    insert into c1251( c, id, descr ) values('ѕ', 190, 'CYRILLIC SMALL LETTER DZE');
    insert into c1251( c, id, descr ) values('ї', 191, 'CYRILLIC SMALL LETTER YI');
    insert into c1251( c, id, descr ) values('А', 192, 'CYRILLIC CAPITAL LETTER A');
    insert into c1251( c, id, descr ) values('Б', 193, 'CYRILLIC CAPITAL LETTER BE');
    insert into c1251( c, id, descr ) values('В', 194, 'CYRILLIC CAPITAL LETTER VE');
    insert into c1251( c, id, descr ) values('Г', 195, 'CYRILLIC CAPITAL LETTER GHE');
    insert into c1251( c, id, descr ) values('Д', 196, 'CYRILLIC CAPITAL LETTER DE');
    insert into c1251( c, id, descr ) values('Е', 197, 'CYRILLIC CAPITAL LETTER IE');
    insert into c1251( c, id, descr ) values('Ж', 198, 'CYRILLIC CAPITAL LETTER ZHE');
    insert into c1251( c, id, descr ) values('З', 199, 'CYRILLIC CAPITAL LETTER ZE');
    insert into c1251( c, id, descr ) values('И', 200, 'CYRILLIC CAPITAL LETTER I');
    insert into c1251( c, id, descr ) values('Й', 201, 'CYRILLIC CAPITAL LETTER SHORT I');
    insert into c1251( c, id, descr ) values('К', 202, 'CYRILLIC CAPITAL LETTER KA');
    insert into c1251( c, id, descr ) values('Л', 203, 'CYRILLIC CAPITAL LETTER EL');
    insert into c1251( c, id, descr ) values('М', 204, 'CYRILLIC CAPITAL LETTER EM');
    insert into c1251( c, id, descr ) values('Н', 205, 'CYRILLIC CAPITAL LETTER EN');
    insert into c1251( c, id, descr ) values('О', 206, 'CYRILLIC CAPITAL LETTER O');
    insert into c1251( c, id, descr ) values('П', 207, 'CYRILLIC CAPITAL LETTER PE');
    insert into c1251( c, id, descr ) values('Р', 208, 'CYRILLIC CAPITAL LETTER ER');
    insert into c1251( c, id, descr ) values('С', 209, 'CYRILLIC CAPITAL LETTER ES');
    insert into c1251( c, id, descr ) values('Т', 210, 'CYRILLIC CAPITAL LETTER TE');
    insert into c1251( c, id, descr ) values('У', 211, 'CYRILLIC CAPITAL LETTER U');
    insert into c1251( c, id, descr ) values('Ф', 212, 'CYRILLIC CAPITAL LETTER EF');
    insert into c1251( c, id, descr ) values('Х', 213, 'CYRILLIC CAPITAL LETTER HA');
    insert into c1251( c, id, descr ) values('Ц', 214, 'CYRILLIC CAPITAL LETTER TSE');
    insert into c1251( c, id, descr ) values('Ч', 215, 'CYRILLIC CAPITAL LETTER CHE');
    insert into c1251( c, id, descr ) values('Ш', 216, 'CYRILLIC CAPITAL LETTER SHA');
    insert into c1251( c, id, descr ) values('Щ', 217, 'CYRILLIC CAPITAL LETTER SHCHA');
    insert into c1251( c, id, descr ) values('Ъ', 218, 'CYRILLIC CAPITAL LETTER HARD SIGN');
    insert into c1251( c, id, descr ) values('Ы', 219, 'CYRILLIC CAPITAL LETTER YERU');
    insert into c1251( c, id, descr ) values('Ь', 220, 'CYRILLIC CAPITAL LETTER SOFT SIGN');
    insert into c1251( c, id, descr ) values('Э', 221, 'CYRILLIC CAPITAL LETTER E');
    insert into c1251( c, id, descr ) values('Ю', 222, 'CYRILLIC CAPITAL LETTER YU');
    insert into c1251( c, id, descr ) values('Я', 223, 'CYRILLIC CAPITAL LETTER YA');
    insert into c1251( c, id, descr ) values('а', 224, 'CYRILLIC SMALL LETTER A');
    insert into c1251( c, id, descr ) values('б', 225, 'CYRILLIC SMALL LETTER BE');
    insert into c1251( c, id, descr ) values('в', 226, 'CYRILLIC SMALL LETTER VE');
    insert into c1251( c, id, descr ) values('г', 227, 'CYRILLIC SMALL LETTER GHE');
    insert into c1251( c, id, descr ) values('д', 228, 'CYRILLIC SMALL LETTER DE');
    insert into c1251( c, id, descr ) values('е', 229, 'CYRILLIC SMALL LETTER IE');
    insert into c1251( c, id, descr ) values('ж', 230, 'CYRILLIC SMALL LETTER ZHE');
    insert into c1251( c, id, descr ) values('з', 231, 'CYRILLIC SMALL LETTER ZE');
    insert into c1251( c, id, descr ) values('и', 232, 'CYRILLIC SMALL LETTER I');
    insert into c1251( c, id, descr ) values('й', 233, 'CYRILLIC SMALL LETTER SHORT I');
    insert into c1251( c, id, descr ) values('к', 234, 'CYRILLIC SMALL LETTER KA');
    insert into c1251( c, id, descr ) values('л', 235, 'CYRILLIC SMALL LETTER EL');
    insert into c1251( c, id, descr ) values('м', 236, 'CYRILLIC SMALL LETTER EM');
    insert into c1251( c, id, descr ) values('н', 237, 'CYRILLIC SMALL LETTER EN');
    insert into c1251( c, id, descr ) values('о', 238, 'CYRILLIC SMALL LETTER O');
    insert into c1251( c, id, descr ) values('п', 239, 'CYRILLIC SMALL LETTER PE');
    insert into c1251( c, id, descr ) values('р', 240, 'CYRILLIC SMALL LETTER ER');
    insert into c1251( c, id, descr ) values('с', 241, 'CYRILLIC SMALL LETTER ES');
    insert into c1251( c, id, descr ) values('т', 242, 'CYRILLIC SMALL LETTER TE');
    insert into c1251( c, id, descr ) values('у', 243, 'CYRILLIC SMALL LETTER U');
    insert into c1251( c, id, descr ) values('ф', 244, 'CYRILLIC SMALL LETTER EF');
    insert into c1251( c, id, descr ) values('х', 245, 'CYRILLIC SMALL LETTER HA');
    insert into c1251( c, id, descr ) values('ц', 246, 'CYRILLIC SMALL LETTER TSE');
    insert into c1251( c, id, descr ) values('ч', 247, 'CYRILLIC SMALL LETTER CHE');
    insert into c1251( c, id, descr ) values('ш', 248, 'CYRILLIC SMALL LETTER CHA');
    insert into c1251( c, id, descr ) values('щ', 249, 'CYRILLIC SMALL LETTER SHCHA');
    insert into c1251( c, id, descr ) values('ъ', 250, 'CYRILLIC SMALL LETTER HARD SIGN');
    insert into c1251( c, id, descr ) values('ы', 251, 'CYRILLIC SMALL LETTER YERU');
    insert into c1251( c, id, descr ) values('ь', 252, 'CYRILLIC SMALL LETTER SOFT SIGN');
    insert into c1251( c, id, descr ) values('э', 253, 'CYRILLIC SMALL LETTER E');
    insert into c1251( c, id, descr ) values('ю', 254, 'CYRILLIC SMALL LETTER YU');
    insert into c1251( c, id, descr ) values('я', 255, 'CYRILLIC SMALL LETTER YA');
	commit;
"""

db = db_factory(init=init_script, charset='UTF8')

test_script = """
set list on;
-- Test: following statement should pass OK, w/o exceptions:
select min(t.descr) as has_no_upper_case_equiv
from c1251 t
group by upper(t.c)
having count(*) <> 2 ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    HAS_NO_UPPER_CASE_EQUIV         LOW 9 SINGLE QUOTE
    HAS_NO_UPPER_CASE_EQUIV         LOW 9 DOUBLE QUOTE
    HAS_NO_UPPER_CASE_EQUIV         ELLIPSIS
    HAS_NO_UPPER_CASE_EQUIV         DAGGER
    HAS_NO_UPPER_CASE_EQUIV         DOUBLE DAGGER
    HAS_NO_UPPER_CASE_EQUIV         EURO SIGN
    HAS_NO_UPPER_CASE_EQUIV         PER MIL SIGN
    HAS_NO_UPPER_CASE_EQUIV         LEFT SINGLE QUOTE BRACKET
    HAS_NO_UPPER_CASE_EQUIV         HIGH 6 SINGLE QUOTE
    HAS_NO_UPPER_CASE_EQUIV         HIGH 9 SINGLE QUOTE
    HAS_NO_UPPER_CASE_EQUIV         HIGH 6 DOUBLE QUOTE
    HAS_NO_UPPER_CASE_EQUIV         HIGH 9 DOUBLE QUOTE
    HAS_NO_UPPER_CASE_EQUIV         LARGE CENTERED DOT
    HAS_NO_UPPER_CASE_EQUIV         EN DASH
    HAS_NO_UPPER_CASE_EQUIV         EM DASH
    HAS_NO_UPPER_CASE_EQUIV         TRADEMARK SIGN
    HAS_NO_UPPER_CASE_EQUIV         RIGHT SINGLE QUOTE BRACKET
    HAS_NO_UPPER_CASE_EQUIV         CURRENCY SIGN
    HAS_NO_UPPER_CASE_EQUIV         BROKEN BAR
    HAS_NO_UPPER_CASE_EQUIV         PARAGRAPH SIGN
    HAS_NO_UPPER_CASE_EQUIV         COPYRIGHT SIGN
    HAS_NO_UPPER_CASE_EQUIV         LEFT ANGLE QUOTATION MARK
    HAS_NO_UPPER_CASE_EQUIV         NOT SIGN
    HAS_NO_UPPER_CASE_EQUIV         REGISTERED TRADE MARK SIGN
    HAS_NO_UPPER_CASE_EQUIV         DEGREE SIGN, RING ABOVE
    HAS_NO_UPPER_CASE_EQUIV         PLUS-MINUS SIGN
    HAS_NO_UPPER_CASE_EQUIV         MICRO SIGN
    HAS_NO_UPPER_CASE_EQUIV         PILCROW SIGN
    HAS_NO_UPPER_CASE_EQUIV         MIDDLE DOT
    HAS_NO_UPPER_CASE_EQUIV         NUMERO SIGN
    HAS_NO_UPPER_CASE_EQUIV         RIGHT ANGLE QUOTATION MARK
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(charset='WIN1251')
    assert act.clean_stdout == act.clean_expected_stdout


