#coding:utf-8

"""
ID:          issue-4914
ISSUE:       4914
TITLE:       REPLACE function works incorrectly with multibyte charsets
DESCRIPTION:
  Integral test for comparison latin-1 characters with diacritical marks and their
  ascii-equivalents, all kinds of string matching.
JIRA:        CORE-4599
FBTEST:      bugs.core_4599
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
	set list on;
	with recursive
	d as (
		select
		 -- The following is full list of latin-1 letters with diacritical marks
		 -- ***EXCEPT*** those which are enumerated in CORE-4739 and currently
		 -- do NOT pass comparison using accent insensitive collation:
		 -- Ø = U+00D8 // LATIN CAPITAL LETTER O WITH STROKE' (U+00D8), used in danish & iceland alphabets;
		 -- Ð = U+00D0 // LATIN CAPITAL LETTER ETH' (U+00D0), iceland
		 -- Ŀ = U+013F // LATIN CAPITAL LETTER L WITH MIDDLE DOT' (U+013F), catalone (valencian)
		 -- Ł = U+0141 // LATIN CAPITAL LETTER L WITH STROKE' (U+0141), polish
		 cast( 'ĄÁÃÀÅĂÂÄÇĘÊĖÈËÉĢÏÍÎÌĽÑÒŐÕÔÖÓŞŠŢÚÙÛŰÜŸÝŹŻ' as varchar(80) character set utf8) collate unicode_ci_ai utf8_chr
		,cast( 'AAAAAAAACEEEEEEGIIIILNOOOOOOSSTUUUUUYYZZ' as varchar(80) character set utf8) collate unicode_ci_ai ascii_up
		,cast( 'aaaaaaaaceeeeeegiiiilnoooooosstuuuuuyyzz' as varchar(80) character set utf8) collate unicode_ci_ai ascii_lo
		from rdb$database
	)
	,r as(select 1 i from rdb$database union all select r.i+1 from r where r.i < 100)
	,e as(
		select
			 substring(d.utf8_chr from r.i for 1) utf8_chr
			,substring(d.ascii_up from r.i for 1) ascii_up
			,substring(d.ascii_lo from r.i for 1) ascii_lo
		from d join r on r.i <= char_length(d.utf8_chr)
	)
	,f as (
		select
			 e.utf8_chr as utf_char
			,e.ascii_up as ascii_char
			------------------------------------------------------------------
			,iif( e.utf8_chr = e.ascii_up, 1, 0 ) equal_test_up
			,iif( position(e.ascii_up, e.utf8_chr) >0 , 1, 0 ) position_test_up
			,iif( e.utf8_chr starting with e.ascii_up, 1, 0 ) start_with_test_up
			,iif( e.utf8_chr like e.ascii_up, 1, 0 ) like_test_up
			,iif( e.utf8_chr similar to e.ascii_up, 1, 0 ) similar_to_ascii_test_up
			,iif( e.utf8_chr similar to '[[:ALPHA:]]', 1, 0 ) similar_to_alpha_test_up
			,'.'||replace(e.utf8_chr, e.ascii_up,'*')||'.' replace_utf8_to_ascii_up
			,iif( overlay(e.utf8_chr placing e.ascii_up from 1) = e.ascii_up, 1, 0 ) overlay_utf8_to_ascii_up
			-----------------------------------------------------------------
			,iif( e.utf8_chr = e.ascii_lo, 1, 0 ) equal_test_lo
			,iif( position(e.ascii_lo, e.utf8_chr) >0 , 1, 0 ) position_test_lo
			,iif( e.utf8_chr starting with e.ascii_lo, 1, 0 ) start_with_test_lo
			,iif( e.utf8_chr like e.ascii_lo, 1, 0 ) like_test_lo
			,iif( e.utf8_chr similar to e.ascii_lo, 1, 0 ) similar_to_ascii_test_lo
			,iif( e.utf8_chr similar to '[[:lower:]]', 1, 0 ) similar_to_alpha_test_lo
			,'.'||replace(e.utf8_chr, e.ascii_lo,'*')||'.' replace_utf8_to_ascii_lo
			,iif( overlay(e.utf8_chr placing e.ascii_lo from 1) = e.ascii_lo, 1, 0) overlay_utf8_to_ascii_lo
		from e
	)
	select *
	from f
	order by utf_char
	;
"""

act = isql_act('db', test_script)

expected_stdout = """
	UTF_CHAR                        Á
	ASCII_CHAR                      A
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        À
	ASCII_CHAR                      A
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ă
	ASCII_CHAR                      A
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Â
	ASCII_CHAR                      A
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Å
	ASCII_CHAR                      A
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ä
	ASCII_CHAR                      A
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ã
	ASCII_CHAR                      A
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ą
	ASCII_CHAR                      A
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ç
	ASCII_CHAR                      C
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        É
	ASCII_CHAR                      E
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        È
	ASCII_CHAR                      E
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ê
	ASCII_CHAR                      E
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ë
	ASCII_CHAR                      E
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ė
	ASCII_CHAR                      E
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ę
	ASCII_CHAR                      E
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ģ
	ASCII_CHAR                      G
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Í
	ASCII_CHAR                      I
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ì
	ASCII_CHAR                      I
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Î
	ASCII_CHAR                      I
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ï
	ASCII_CHAR                      I
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ľ
	ASCII_CHAR                      L
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ñ
	ASCII_CHAR                      N
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ó
	ASCII_CHAR                      O
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ò
	ASCII_CHAR                      O
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ô
	ASCII_CHAR                      O
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ö
	ASCII_CHAR                      O
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ő
	ASCII_CHAR                      O
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Õ
	ASCII_CHAR                      O
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Š
	ASCII_CHAR                      S
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ş
	ASCII_CHAR                      S
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ţ
	ASCII_CHAR                      T
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ú
	ASCII_CHAR                      U
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ù
	ASCII_CHAR                      U
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Û
	ASCII_CHAR                      U
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ü
	ASCII_CHAR                      U
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ű
	ASCII_CHAR                      U
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ý
	ASCII_CHAR                      Y
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ÿ
	ASCII_CHAR                      Y
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ź
	ASCII_CHAR                      Z
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1

	UTF_CHAR                        Ż
	ASCII_CHAR                      Z
	EQUAL_TEST_UP                   1
	POSITION_TEST_UP                1
	START_WITH_TEST_UP              1
	LIKE_TEST_UP                    1
	SIMILAR_TO_ASCII_TEST_UP        1
	SIMILAR_TO_ALPHA_TEST_UP        1
	REPLACE_UTF8_TO_ASCII_UP        .*.
	OVERLAY_UTF8_TO_ASCII_UP        1
	EQUAL_TEST_LO                   1
	POSITION_TEST_LO                1
	START_WITH_TEST_LO              1
	LIKE_TEST_LO                    1
	SIMILAR_TO_ASCII_TEST_LO        1
	SIMILAR_TO_ALPHA_TEST_LO        1
	REPLACE_UTF8_TO_ASCII_LO        .*.
	OVERLAY_UTF8_TO_ASCII_LO        1
"""

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

