#coding:utf-8

"""
ID:          issue-3334
ISSUE:       3334
TITLE:       Case-sensitive character class names in SIMILAR TO
DESCRIPTION:
JIRA:        CORE-2952
FBTEST:      bugs.core_2952
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

# version: 3.0

test_script_1 = """
    -- NOTE:
    -- 1. This test can NOT be applied on 2.5 because of error:
    --    Statement failed, SQLSTATE = 42000
    --    Invalid SIMILAR TO pattern
    -- 2. Seems that polish letter 'Ł' is NOT considered as having accent
    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop collation co_utf8_ci_ai';
            when any do begin end
        end

        begin
            execute statement 'drop collation co_utf8_cs_as';
            when any do begin end
        end

        begin
            execute statement 'drop collation co_utf8_ci_as';
            when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create collation co_utf8_ci_ai for utf8 from unicode case insensitive accent insensitive;
    create collation co_utf8_cs_as for utf8 from unicode case sensitive accent sensitive;
    create collation co_utf8_ci_as for utf8 from unicode case insensitive accent sensitive;
    commit;

    set list on;

    with recursive
    d as (
        select cast('aeiouyAEIOUYáéíóúýàèìòùâêîôûãñõäëïöüÿçšąęźżăşţÁÉÍÓÚÝÀÈÌÒÙÂÊÎÔÛÃÑÕÄËÏÖÜŸÇŠĄĘŹŻĂŞŢ'||ascii_char(9)||ascii_char(10)||ascii_char(32) as varchar(100) character set utf8) s
        from rdb$database
    )
    ,r as(select 1 i from rdb$database union all select r.i+1 from r where r.i < 100)
    ,e as(
        select substring(d.s from r.i for 1) c
        from d join r on r.i <= char_length(d.s)
    )
    select
         decode( e.c, ascii_char(9),'	', ascii_char(10),'
', ascii_char(32), '\\w', e.c ) c
         -- ALPHA Latin letters a..z and A..Z. With an accent-insensitive collation,
         -- this class also matches accented forms of these characters.
        ,iif( e.c collate co_utf8_ci_ai similar to '[[:aLPHA:]]', 1, 0 ) s_alpha_ci_ai
        ,iif( e.c collate co_utf8_cs_as similar to '[[:aLPHA:]]', 1, 0 ) s_alpha_cs_as
        ,iif( e.c collate co_utf8_ci_as similar to '[[:aLPHA:]]', 1, 0 ) s_alpha_ci_as
        -- [:LOWER:] Lowercase Latin letters a..z. Also matches uppercase with case-insensitive
        -- collation and accented forms with accent-insensitive collation.
        ,iif( e.c collate co_utf8_ci_ai similar to '[[:LoWer:]]', 1, 0 ) s_lower_ci_ai
        ,iif( e.c collate co_utf8_cs_as similar to '[[:lOwEr:]]', 1, 0 ) s_lower_cs_as
        ,iif( e.c collate co_utf8_ci_as similar to '[[:lowER:]]', 1, 0 ) s_lower_ci_as
        -- [:UPPER:] Uppercase Latin letters A..Z. Also matches lowercase with case-insensitive
        -- collation and accented forms with accent-insensitive collation.
        ,iif( e.c collate co_utf8_ci_ai similar to '[[:uPPer:]]', 1, 0 ) s_upper_ci_ai
        ,iif( e.c collate co_utf8_cs_as similar to '[[:uPpeR:]]', 1, 0 ) s_upper_cs_as
        ,iif( e.c collate co_utf8_ci_as similar to '[[:UpPeR:]]', 1, 0 ) s_upper_ci_as
        -- [:WHITESPACE:] Matches vertical tab (ASCII 9), linefeed (ASCII 10), horizontal
        -- tab (ASCII 11), formfeed (ASCII 12), carriage return (ASCII 13) and space (ASCII 32).
        ,iif( e.c similar to '[[:WhiTespacE:]]', 1, 0 ) s_white_space
    from e
    ;
"""

act_1 = isql_act('db', test_script_1)

expected_stdout_1 = """
    C                               a
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   1
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               e
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   1
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               i
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   1
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               o
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   1
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               u
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   1
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               y
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   1
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               A
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   1
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               E
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   1
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               I
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   1
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               O
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   1
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               U
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   1
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               Y
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   1
    S_ALPHA_CI_AS                   1
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   1
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   1
    S_UPPER_CI_AS                   1
    S_WHITE_SPACE                   0

    C                               á
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               é
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               í
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ó
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ú
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ý
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               à
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               è
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ì
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ò
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ù
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               â
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ê
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               î
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ô
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               û
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ã
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ñ
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               õ
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ä
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ë
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ï
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ö
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ü
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ÿ
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ç
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               š
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ą
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ę
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0


    C                               ź
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ż
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ă
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ş
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               ţ
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Á
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               É
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Í
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ó
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ú
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ý
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               À
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               È
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ì
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ò
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ù
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Â
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ê
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Î
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ô
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Û
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ã
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ñ
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Õ
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ä
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ë
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ï
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ö
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ü
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ÿ
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ç
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Š
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ą
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ę
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0


    C                               Ź
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ż
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ă
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ş
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C                               Ţ
    S_ALPHA_CI_AI                   1
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   1
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   1
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   0

    C
    S_ALPHA_CI_AI                   0
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   0
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   0
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   1

    C
    S_ALPHA_CI_AI                   0
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   0
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   0
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   1

    C                               \\w
    S_ALPHA_CI_AI                   0
    S_ALPHA_CS_AS                   0
    S_ALPHA_CI_AS                   0
    S_LOWER_CI_AI                   0
    S_LOWER_CS_AS                   0
    S_LOWER_CI_AS                   0
    S_UPPER_CI_AI                   0
    S_UPPER_CS_AS                   0
    S_UPPER_CI_AS                   0
    S_WHITE_SPACE                   1
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.execute()
    assert act_1.clean_stdout == act_1.clean_expected_stdout

# version: 4.0

test_script_2 = """
    -- NOTE:
    -- 1. This test can NOT be applied on 2.5 because of error:
    --    Statement failed, SQLSTATE = 42000
    --    Invalid SIMILAR TO pattern
    -- 2. Added four characters: 'Ø' 'Ð' 'Ł' and 'Ŀ' - because of fixed CORE-4739

    set term ^;
    execute block as
    begin
        begin
            execute statement 'drop collation co_utf8_ci_ai';
            when any do begin end
        end

        begin
            execute statement 'drop collation co_utf8_cs_as';
            when any do begin end
        end

        begin
            execute statement 'drop collation co_utf8_ci_as';
            when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create collation co_utf8_ci_ai for utf8 from unicode case insensitive accent insensitive;
    create collation co_utf8_cs_as for utf8 from unicode case sensitive accent sensitive;
    create collation co_utf8_ci_as for utf8 from unicode case insensitive accent sensitive;
    commit;

    set list on;

    with recursive
    d as (
        select cast('aeiouyAEIOUYáéíóúýàèìòùâêîôûãñõäëïöüÿçšąęźżăşţÁÉÍÓÚÝÀÈÌÒÙÂÊÎÔÛÃÑÕÄËÏÖÜŸÇŠĄĘŹŻĂŞŢ'
		                || ascii_char(9) || ascii_char(10) || ascii_char(32)
						|| 'øðłŀØÐŁĿ' -- added 14.10.2019
			        as varchar(100) character set utf8
				    ) s
        from rdb$database
    )
    ,r as(select 1 i from rdb$database union all select r.i+1 from r where r.i < 100)
    ,e as(
        select substring(d.s from r.i for 1) c
        from d join r on r.i <= char_length(d.s)
    )
    select
         decode( e.c, ascii_char(9),'	', ascii_char(10),'
', ascii_char(32), '\\w', e.c ) c
         -- ALPHA Latin letters a..z and A..Z. With an accent-insensitive collation,
         -- this class also matches accented forms of these characters.
        ,iif( e.c collate co_utf8_ci_ai similar to '[[:aLPHA:]]', 1, 0 ) s_alpha_ci_ai
        ,iif( e.c collate co_utf8_cs_as similar to '[[:aLPHA:]]', 1, 0 ) s_alpha_cs_as
        ,iif( e.c collate co_utf8_ci_as similar to '[[:aLPHA:]]', 1, 0 ) s_alpha_ci_as
        -- [:LOWER:] Lowercase Latin letters a..z. Also matches uppercase with case-insensitive
        -- collation and accented forms with accent-insensitive collation.
        ,iif( e.c collate co_utf8_ci_ai similar to '[[:LoWer:]]', 1, 0 ) s_lower_ci_ai
        ,iif( e.c collate co_utf8_cs_as similar to '[[:lOwEr:]]', 1, 0 ) s_lower_cs_as
        ,iif( e.c collate co_utf8_ci_as similar to '[[:lowER:]]', 1, 0 ) s_lower_ci_as
        -- [:UPPER:] Uppercase Latin letters A..Z. Also matches lowercase with case-insensitive
        -- collation and accented forms with accent-insensitive collation.
        ,iif( e.c collate co_utf8_ci_ai similar to '[[:uPPer:]]', 1, 0 ) s_upper_ci_ai
        ,iif( e.c collate co_utf8_cs_as similar to '[[:uPpeR:]]', 1, 0 ) s_upper_cs_as
        ,iif( e.c collate co_utf8_ci_as similar to '[[:UpPeR:]]', 1, 0 ) s_upper_ci_as
        -- [:WHITESPACE:] Matches vertical tab (ASCII 9), linefeed (ASCII 10), horizontal
        -- tab (ASCII 11), formfeed (ASCII 12), carriage return (ASCII 13) and space (ASCII 32).
        ,iif( e.c similar to '[[:WhiTespacE:]]', 1, 0 ) s_white_space
    from e
    ;
"""

act_2 = isql_act('db', test_script_2)

expected_stdout_2 = """
	C                               a
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   1
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               e
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   1
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               i
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   1
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               o
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   1
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               u
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   1
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               y
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   1
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               A
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   1
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               E
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   1
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               I
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   1
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               O
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   1
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               U
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   1
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               Y
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   1
	S_ALPHA_CI_AS                   1
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   1
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   1
	S_UPPER_CI_AS                   1
	S_WHITE_SPACE                   0

	C                               á
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               é
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               í
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ó
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ú
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ý
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               à
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               è
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ì
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ò
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ù
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               â
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ê
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               î
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ô
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               û
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ã
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ñ
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               õ
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ä
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ë
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ï
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ö
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ü
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ÿ
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ç
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               š
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ą
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ę
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ź
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ż
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ă
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ş
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ţ
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Á
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               É
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Í
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ó
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ú
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ý
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               À
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               È
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ì
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ò
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ù
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Â
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ê
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Î
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ô
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Û
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ã
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ñ
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Õ
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ä
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ë
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ï
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ö
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ü
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ÿ
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ç
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Š
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ą
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ę
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ź
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ż
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ă
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ş
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ţ
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C
	S_ALPHA_CI_AI                   0
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   0
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   0
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   1

	C
	S_ALPHA_CI_AI                   0
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   0
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   0
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   1

	C                               \\w
	S_ALPHA_CI_AI                   0
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   0
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   0
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   1

	C                               ø
	S_ALPHA_CI_AI                   0
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   0
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   0
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ð
	S_ALPHA_CI_AI                   0
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   0
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   0
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ł
	S_ALPHA_CI_AI                   0
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   0
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   0
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               ŀ
	S_ALPHA_CI_AI                   0
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   0
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   0
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ø
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ð
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ł
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0

	C                               Ŀ
	S_ALPHA_CI_AI                   1
	S_ALPHA_CS_AS                   0
	S_ALPHA_CI_AS                   0
	S_LOWER_CI_AI                   1
	S_LOWER_CS_AS                   0
	S_LOWER_CI_AS                   0
	S_UPPER_CI_AI                   1
	S_UPPER_CS_AS                   0
	S_UPPER_CI_AS                   0
	S_WHITE_SPACE                   0
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.execute()
    assert act_2.clean_stdout == act_2.clean_expected_stdout

