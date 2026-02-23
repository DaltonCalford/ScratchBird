#coding:utf-8

"""
ID:          issue-1151
ISSUE:       1151
TITLE:       Wildcards/Regular Expressions in WHERE clause - SIMILAR TO predicate
DESCRIPTION:
JIRA:        CORE-769
FBTEST:      bugs.core_0769
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT IIF('ab' SIMILAR TO 'ab|cd|efg','true','false'),'true','''ab'' SIMILAR TO ''ab|cd|efg''' FROM RDB$DATABASE;
SELECT IIF('efg' SIMILAR TO 'ab|cd|efg','true','false'),'true','''efg'' SIMILAR TO ''ab|cd|efg''' FROM RDB$DATABASE;
SELECT IIF('a' SIMILAR TO 'ab|cd|efg','true','false'),'false','''a'' SIMILAR TO ''ab|cd|efg''' FROM RDB$DATABASE;
SELECT IIF('' SIMILAR TO 'a*','true','false'),'true',''''' SIMILAR TO ''a*''' FROM RDB$DATABASE;
SELECT IIF('a' SIMILAR TO 'a*','true','false'),'true','''a'' SIMILAR TO ''a*''' FROM RDB$DATABASE;
SELECT IIF('aaa' SIMILAR TO 'a*','true','false'),'true','''aaa'' SIMILAR TO ''a*''' FROM RDB$DATABASE;
SELECT IIF('' SIMILAR TO 'a+','true','false'),'false',''''' SIMILAR TO ''a+''' FROM RDB$DATABASE;
SELECT IIF('a' SIMILAR TO 'a+','true','false'),'true','''a'' SIMILAR TO ''a+''' FROM RDB$DATABASE;
SELECT IIF('aaa' SIMILAR TO 'a+','true','false'),'true','''aaa'' SIMILAR TO ''a+''' FROM RDB$DATABASE;
SELECT IIF('' SIMILAR TO 'a?','true','false'),'true',''''' SIMILAR TO ''a?''' FROM RDB$DATABASE;
SELECT IIF('a' SIMILAR TO 'a?','true','false'),'true','''a'' SIMILAR TO ''a?''' FROM RDB$DATABASE;
SELECT IIF('aaa' SIMILAR TO 'a?','true','false'),'false','''aaa'' SIMILAR TO ''a?''' FROM RDB$DATABASE;
SELECT IIF('' SIMILAR TO 'a{2,}','true','false'),'false',''''' SIMILAR TO ''a{2,}''' FROM RDB$DATABASE;
SELECT IIF('a' SIMILAR TO 'a{2,}','true','false'),'false','''a'' SIMILAR TO ''a{2,}''' FROM RDB$DATABASE;
SELECT IIF('aa' SIMILAR TO 'a{2,}','true','false'),'true','''aa'' SIMILAR TO ''a{2,}''' FROM RDB$DATABASE;
SELECT IIF('aaa' SIMILAR TO 'a{2,}','true','false'),'true','''aaa'' SIMILAR TO ''a{2,}''' FROM RDB$DATABASE;
SELECT IIF('' SIMILAR TO 'a{2,4}','true','false'),'false',''''' SIMILAR TO ''a{2,4}''' FROM RDB$DATABASE;
SELECT IIF('a' SIMILAR TO 'a{2,4}','true','false'),'false','''a'' SIMILAR TO ''a{2,4}''' FROM RDB$DATABASE;
SELECT IIF('aa' SIMILAR TO 'a{2,4}','true','false'),'true','''aa'' SIMILAR TO ''a{2,4}''' FROM RDB$DATABASE;
SELECT IIF('aaa' SIMILAR TO 'a{2,4}','true','false'),'true','''aaa'' SIMILAR TO ''a{2,4}''' FROM RDB$DATABASE;
SELECT IIF('aaaa' SIMILAR TO 'a{2,4}','true','false'),'true','''aaaa'' SIMILAR TO ''a{2,4}''' FROM RDB$DATABASE;
SELECT IIF('aaaaa' SIMILAR TO 'a{2,4}','true','false'),'false','''aaaaa'' SIMILAR TO ''a{2,4}''' FROM RDB$DATABASE;
SELECT IIF('' SIMILAR TO '_','true','false'),'false',''''' SIMILAR TO ''_''' FROM RDB$DATABASE;
SELECT IIF('a' SIMILAR TO '_','true','false'),'true','''a'' SIMILAR TO ''_''' FROM RDB$DATABASE;
SELECT IIF('1' SIMILAR TO '_','true','false'),'true','''1'' SIMILAR TO ''_''' FROM RDB$DATABASE;
SELECT IIF('a1' SIMILAR TO '_','true','false'),'false','''a1'' SIMILAR TO ''_''' FROM RDB$DATABASE;
SELECT IIF('' SIMILAR TO '%','true','false'),'true',''''' SIMILAR TO ''%''' FROM RDB$DATABASE;
SELECT IIF('az' SIMILAR TO 'a%z','true','false'),'true','''az'' SIMILAR TO ''a%z''' FROM RDB$DATABASE;
SELECT IIF('a123z' SIMILAR TO 'a%z','true','false'),'true','''a123z'' SIMILAR TO ''a%z''' FROM RDB$DATABASE;
SELECT IIF('azx' SIMILAR TO 'a%z','true','false'),'false','''azx'' SIMILAR TO ''a%z''' FROM RDB$DATABASE;
SELECT IIF('ab' SIMILAR TO '(ab){2}','true','false'),'false','''ab'' SIMILAR TO ''(ab){2}''' FROM RDB$DATABASE;
SELECT IIF('aabb' SIMILAR TO '(ab){2}','true','false'),'false','''aabb'' SIMILAR TO ''(ab){2}''' FROM RDB$DATABASE;
SELECT IIF('abab' SIMILAR TO '(ab){2}','true','false'),'true','''abab'' SIMILAR TO ''(ab){2}''' FROM RDB$DATABASE;
SELECT IIF('b' SIMILAR TO '[abc]','true','false'),'true','''b'' SIMILAR TO ''[abc]''' FROM RDB$DATABASE;
SELECT IIF('d' SIMILAR TO '[abc]','true','false'),'false','''d'' SIMILAR TO ''[abc]''' FROM RDB$DATABASE;
SELECT IIF('9' SIMILAR TO '[0-9]','true','false'),'true','''9'' SIMILAR TO ''[0-9]''' FROM RDB$DATABASE;
SELECT IIF('9' SIMILAR TO '[0-8]','true','false'),'false','''9'' SIMILAR TO ''[0-8]''' FROM RDB$DATABASE;
SELECT IIF('b' SIMILAR TO '[^abc]','true','false'),'false','''b'' SIMILAR TO ''[^abc]''' FROM RDB$DATABASE;
SELECT IIF('d' SIMILAR TO '[^abc]','true','false'),'true','''d'' SIMILAR TO ''[^abc]''' FROM RDB$DATABASE;
SELECT IIF('3' SIMILAR TO '[[:DIGIT:]^3]','true','false'),'false','''3'' SIMILAR TO ''[[:DIGIT:]^3]''' FROM RDB$DATABASE;
SELECT IIF('4' SIMILAR TO '[[:DIGIT:]^3]','true','false'),'true','''4'' SIMILAR TO ''[[:DIGIT:]^3]''' FROM RDB$DATABASE;
SELECT IIF('4' SIMILAR TO '[[:DIGIT:]]','true','false'),'true','''4'' SIMILAR TO ''[[:DIGIT:]]''' FROM RDB$DATABASE;
SELECT IIF('a' SIMILAR TO '[[:DIGIT:]]','true','false'),'false','''a'' SIMILAR TO ''[[:DIGIT:]]''' FROM RDB$DATABASE;
SELECT IIF('4' SIMILAR TO '[^[:DIGIT:]]','true','false'),'false','''4'' SIMILAR TO ''[^[:DIGIT:]]''' FROM RDB$DATABASE;
SELECT IIF('a' SIMILAR TO '[^[:DIGIT:]]','true','false'),'true','''a'' SIMILAR TO ''[^[:DIGIT:]]''' FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """
CASE   CONSTANT CONSTANT
====== ======== ===========================
true   true     'ab' SIMILAR TO 'ab|cd|efg'


CASE   CONSTANT CONSTANT
====== ======== ============================
true   true     'efg' SIMILAR TO 'ab|cd|efg'


CASE   CONSTANT CONSTANT
====== ======== ==========================
false  false    'a' SIMILAR TO 'ab|cd|efg'


CASE   CONSTANT CONSTANT
====== ======== ==================
true   true     '' SIMILAR TO 'a*'


CASE   CONSTANT CONSTANT
====== ======== ===================
true   true     'a' SIMILAR TO 'a*'


CASE   CONSTANT CONSTANT
====== ======== =====================
true   true     'aaa' SIMILAR TO 'a*'


CASE   CONSTANT CONSTANT
====== ======== ==================
false  false    '' SIMILAR TO 'a+'


CASE   CONSTANT CONSTANT
====== ======== ===================
true   true     'a' SIMILAR TO 'a+'


CASE   CONSTANT CONSTANT
====== ======== =====================
true   true     'aaa' SIMILAR TO 'a+'


CASE   CONSTANT CONSTANT
====== ======== ==================
true   true     '' SIMILAR TO 'a?'


CASE   CONSTANT CONSTANT
====== ======== ===================
true   true     'a' SIMILAR TO 'a?'


CASE   CONSTANT CONSTANT
====== ======== =====================
false  false    'aaa' SIMILAR TO 'a?'


CASE   CONSTANT CONSTANT
====== ======== =====================
false  false    '' SIMILAR TO 'a{2,}'


CASE   CONSTANT CONSTANT
====== ======== ======================
false  false    'a' SIMILAR TO 'a{2,}'


CASE   CONSTANT CONSTANT
====== ======== =======================
true   true     'aa' SIMILAR TO 'a{2,}'


CASE   CONSTANT CONSTANT
====== ======== ========================
true   true     'aaa' SIMILAR TO 'a{2,}'


CASE   CONSTANT CONSTANT
====== ======== ======================
false  false    '' SIMILAR TO 'a{2,4}'


CASE   CONSTANT CONSTANT
====== ======== =======================
false  false    'a' SIMILAR TO 'a{2,4}'


CASE   CONSTANT CONSTANT
====== ======== ========================
true   true     'aa' SIMILAR TO 'a{2,4}'


CASE   CONSTANT CONSTANT
====== ======== =========================
true   true     'aaa' SIMILAR TO 'a{2,4}'


CASE   CONSTANT CONSTANT
====== ======== ==========================
true   true     'aaaa' SIMILAR TO 'a{2,4}'


CASE   CONSTANT CONSTANT
====== ======== ===========================
false  false    'aaaaa' SIMILAR TO 'a{2,4}'


CASE   CONSTANT CONSTANT
====== ======== =================
false  false    '' SIMILAR TO '_'


CASE   CONSTANT CONSTANT
====== ======== ==================
true   true     'a' SIMILAR TO '_'


CASE   CONSTANT CONSTANT
====== ======== ==================
true   true     '1' SIMILAR TO '_'


CASE   CONSTANT CONSTANT
====== ======== ===================
false  false    'a1' SIMILAR TO '_'


CASE   CONSTANT CONSTANT
====== ======== =================
true   true     '' SIMILAR TO '%'


CASE   CONSTANT CONSTANT
====== ======== =====================
true   true     'az' SIMILAR TO 'a%z'


CASE   CONSTANT CONSTANT
====== ======== ========================
true   true     'a123z' SIMILAR TO 'a%z'


CASE   CONSTANT CONSTANT
====== ======== ======================
false  false    'azx' SIMILAR TO 'a%z'


CASE   CONSTANT CONSTANT
====== ======== =========================
false  false    'ab' SIMILAR TO '(ab){2}'


CASE   CONSTANT CONSTANT
====== ======== ===========================
false  false    'aabb' SIMILAR TO '(ab){2}'


CASE   CONSTANT CONSTANT
====== ======== ===========================
true   true     'abab' SIMILAR TO '(ab){2}'


CASE   CONSTANT CONSTANT
====== ======== ======================
true   true     'b' SIMILAR TO '[abc]'


CASE   CONSTANT CONSTANT
====== ======== ======================
false  false    'd' SIMILAR TO '[abc]'


CASE   CONSTANT CONSTANT
====== ======== ======================
true   true     '9' SIMILAR TO '[0-9]'


CASE   CONSTANT CONSTANT
====== ======== ======================
false  false    '9' SIMILAR TO '[0-8]'


CASE   CONSTANT CONSTANT
====== ======== =======================
false  false    'b' SIMILAR TO '[^abc]'


CASE   CONSTANT CONSTANT
====== ======== =======================
true   true     'd' SIMILAR TO '[^abc]'


CASE   CONSTANT CONSTANT
====== ======== ==============================
false  false    '3' SIMILAR TO '[[:DIGIT:]^3]'


CASE   CONSTANT CONSTANT
====== ======== ==============================
true   true     '4' SIMILAR TO '[[:DIGIT:]^3]'


CASE   CONSTANT CONSTANT
====== ======== ============================
true   true     '4' SIMILAR TO '[[:DIGIT:]]'


CASE   CONSTANT CONSTANT
====== ======== ============================
false  false    'a' SIMILAR TO '[[:DIGIT:]]'


CASE   CONSTANT CONSTANT
====== ======== =============================
false  false    '4' SIMILAR TO '[^[:DIGIT:]]'


CASE   CONSTANT CONSTANT
====== ======== =============================
true   true     'a' SIMILAR TO '[^[:DIGIT:]]'

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

