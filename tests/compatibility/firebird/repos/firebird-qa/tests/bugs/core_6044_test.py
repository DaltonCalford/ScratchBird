#coding:utf-8

"""
ID:          issue-6294
ISSUE:       6294
TITLE:       ISQL issues with increased identifier length
DESCRIPTION:
JIRA:        CORE-6044
FBTEST:      bugs.core_6044
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214
"""

import pytest
from firebird.qa import *

substitutions = [  ('current value.*', 'current value')
                  ,("'COLL-VERSION.*", "'<attr>'")
                  #,('COLL-VERSION=\\d+.\\d+(;ICU-VERSION=\\d+.\\d+)?.*', '<attr>')
                ]

db = db_factory(charset='UTF8')

test_script_ = """
	set bail on;
	create  exception "ИсключениеДляСообщенияПользователюОНевозможностиПреобразованияя" 'Ваша строка не может быть преобразована в число.';
	create collation  "КоллацияДляСортировкиСтроковыхДанныхКоторыеПредставимыКакЧислаа" for utf8 from unicode case insensitive 'NUMERIC-SORT=1';
	create domain     "ДоменДляХраненияСтроковыхДанныхКоторыеПредставимыДляСортировкии" as varchar(160) character set utf8 collate "КоллацияДляСортировкиСтроковыхДанныхКоторыеПредставимыКакЧислаа";
	create sequence   "ГенераторКоторыйДолженСодержатьНомераПоследнихУдаленнДокументов";
	create table      "ТаблицаКотораяВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю"(
					  "СтолбецКоторыйВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю"
					  "ДоменДляХраненияСтроковыхДанныхКоторыеПредставимыДляСортировкии"
					  ,constraint
					  "ПервичныйКлючНаТаблицуКотораяВсегдаДолжнаСодержатьСвежайшуюИнфу"
					  primary key
					 ("СтолбецКоторыйВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю")
	);
	set bail off;

	show domain;    -- this passed OK
	show exception; -- this passed OK
	show collation; -- this passed OK
	show table;     -- this passed OK
	show sequ;      -- this led to crash
	show table "ТаблицаКотораяВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю"; -- this also led to crash
"""

act = isql_act('db', test_script_, substitutions=substitutions)

expected_stdout_5x = """
	ДоменДляХраненияСтроковыхДанныхКоторыеПредставимыДляСортировкии
	ИсключениеДляСообщенияПользователюОНевозможностиПреобразованияя; Msg: Ваша строка не может быть преобразована в число.
	КоллацияДляСортировкиСтроковыхДанныхКоторыеПредставимыКакЧислаа, CHARACTER SET UTF8, FROM EXTERNAL ('UNICODE'), PAD SPACE, CASE INSENSITIVE, 'COLL-VERSION=153.88;NUMERIC-SORT=1'
	ТаблицаКотораяВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю
	Generator ГенераторКоторыйДолженСодержатьНомераПоследнихУдаленнДокументов, current value: 0, initial value: 0, increment: 1
	СтолбецКоторыйВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю (ДоменДляХраненияСтроковыхДанныхКоторыеПредставимыДляСортировкии) VARCHAR(160) CHARACTER SET UTF8 Not Null
	COLLATE КоллацияДляСортировкиСтроковыхДанныхКоторыеПредставимыКакЧислаа
	CONSTRAINT ПервичныйКлючНаТаблицуКотораяВсегдаДолжнаСодержатьСвежайшуюИнфу:
	Primary key (СтолбецКоторыйВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю)
"""

expected_stdout_6x = """
    PUBLIC."ДоменДляХраненияСтроковыхДанныхКоторыеПредставимыДляСортировкии"
    PUBLIC."ИсключениеДляСообщенияПользователюОНевозможностиПреобразованияя"; Msg: Ваша строка не может быть преобразована в число.
    PUBLIC."КоллацияДляСортировкиСтроковыхДанныхКоторыеПредставимыКакЧислаа", CHARACTER SET SYSTEM.UTF8, FROM EXTERNAL ('UNICODE'), PAD SPACE, CASE INSENSITIVE, '<attr>'
    PUBLIC."ТаблицаКотораяВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю"
    Generator PUBLIC."ГенераторКоторыйДолженСодержатьНомераПоследнихУдаленнДокументов", current value
    Table: PUBLIC."ТаблицаКотораяВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю"
    "СтолбецКоторыйВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю" (PUBLIC."ДоменДляХраненияСтроковыхДанныхКоторыеПредставимыДляСортировкии") VARCHAR(160) CHARACTER SET SYSTEM.UTF8 COLLATE PUBLIC."КоллацияДляСортировкиСтроковыхДанныхКоторыеПредставимыКакЧислаа" Not Null
    CONSTRAINT "ПервичныйКлючНаТаблицуКотораяВсегдаДолжнаСодержатьСвежайшуюИнфу":
    Primary key ("СтолбецКоторыйВсегдаДолжнаСодержатьТолькоСамуюСвежуюИнформациюю")

"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(charset='utf8')
    assert act.clean_stdout == act.clean_expected_stdout
