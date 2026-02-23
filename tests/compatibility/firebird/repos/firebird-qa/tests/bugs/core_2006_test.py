#coding:utf-8

"""
ID:          issue-2443
ISSUE:       2443
TITLE:       SUBSTRING with regular expression (SIMILAR TO) capability
DESCRIPTION:
JIRA:        CORE-2006
FBTEST:      bugs.core_2006
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    ------------------------------------------------------------------------------

    -- Test for matching with percent characters before and after pattern:
    select
         trim(str) str
        ,trim(ptn) ptn
        ,iif( trim(str) similar to '%'||trim(ptn)||'%', 1, 0 ) "str similar to %ptn%"
        ,substring( trim(str) similar '%\\"' || trim(ptn) || '\\"%' escape '\\' ) "subs(str similar to %ptn%)"
    from(
        select
            'WDWDWDWD' str
           ,'((DW)|(WD)){4}' ptn
        from rdb$database

        union all

        select
            'AAXYAAXYAAAAXYAAAXYAA' str
           ,'(AAXY|AAAX){2,}' ptn
        from rdb$database

        union all

        select
            'YZZXYZZ0Z0YZZYZZYYZZZYZZ0Z0YZZ'
           ,'(0Z0(Y|Z)*){2}'
        from rdb$database

        union all

        select
            'AARARARAARARAR'
           ,'RA(AR){3}'
        from rdb$database

        union all

        select
            'eiavieieav' str
           ,'(ie){2,}' ptn
        from rdb$database

        union all

        select
             'avieieavav' str
             ,'(av|ie){2,}' ptn
        from rdb$database

        union all

        select
            'avieieieav' str
           ,'((av)|(ie)){2,}' ptn
        from rdb$database

    );

    ----------------------

    -- Test for exact matching to pattern:
    select
         trim(str) str
        ,trim(ptn) ptn
        ,iif( trim(str) similar to trim(ptn), 1, 0 ) "str similar to ptn"
        ,substring( trim(str) similar '\\"' || trim(ptn) || '\\"' escape '\\' ) "subs(str similar to ptn)"
    from(
        select ----------- core-2389
             'x/t' str
            ,'%[/]t' ptn
        from rdb$database


        union all

        select ------------------- core-2756
              '2015-04-13' str
             ,'[[:DIGIT:]]{4}[-][[:DIGIT:]]{2}[-][[:DIGIT:]]{2}' ptn
        from rdb$database

        union all

        select ------------------- core-2780
             'WI-T3.0.0.31780 Firebird 3.0 Beta 2'
            ,'%[0-9]+.[0-9]+.[0-9]+((.?[0-9]+)*)[[:WHITESPACE:]]%'
        from rdb$database

        union all

        select ----------- core-3523
             'm'
            ,'[p-k]'
        from rdb$database

        union all

        ------------------- core-3754

        select '1', '(1|2){0,}' from rdb$database union all select
        '1', '(1|2){0,1}'       from rdb$database union all select
        '1', '(1|2){1}'         from rdb$database union all select
        '123', '(1|12[3]?){1}'  from rdb$database union all select
        '123', '(1|12[3]?)+'    from rdb$database union all select

        ------------- core-0769
        'ab', 'ab|cd|efg' from rdb$database union all select
        'efg', 'ab|cd|efg' from rdb$database union all select
        'a', 'ab|cd|efg' from rdb$database union all select   -- 0
        '', 'a*' from rdb$database union all select
        'a', 'a*' from rdb$database union all select
        'aaa', 'a*' from rdb$database union all select
        '', 'a+' from rdb$database union all select           -- 0
        'a', 'a+' from rdb$database union all select
        'aaa', 'a+' from rdb$database union all select
        '', 'a?' from rdb$database union all select
        'a', 'a?' from rdb$database union all select
        'aaa', 'a?' from rdb$database union all select        -- 0
        '', 'a{2,}' from rdb$database union all select        -- 0
        'a', 'a{2,}' from rdb$database union all select       -- 0
        'aa', 'a{2,}' from rdb$database union all select
        'aaa', 'a{2,}' from rdb$database union all select
        '', 'a{2,4}' from rdb$database union all select       -- 0
        'a', 'a{2,4}' from rdb$database union all select      -- 0
        'aa', 'a{2,4}' from rdb$database union all select
        'aaa', 'a{2,4}' from rdb$database union all select
        'aaaa', 'a{2,4}' from rdb$database union all select
        'aaaaa', 'a{2,4}' from rdb$database union all select  -- 0
        '', '_' from rdb$database union all select            -- 0
        'a', '_' from rdb$database union all select
        '1', '_' from rdb$database union all select
        'a1', '_' from rdb$database union all select          -- 0
        '', '%' from rdb$database union all select
        'az', 'a%z' from rdb$database union all select
        'a123z', 'a%z' from rdb$database union all select
        'azx', 'a%z' from rdb$database union all select       -- 0
        'ab', '(ab){2}' from rdb$database union all select    -- 0
        'aabb', '(ab){2}' from rdb$database union all select  -- 0
        'abab', '(ab){2}' from rdb$database union all select
        'b', '[abc]' from rdb$database union all select
        'd', '[abc]' from rdb$database union all select       -- 0
        '9', '[0-9]' from rdb$database union all select
        '9', '[0-8]' from rdb$database union all select       -- 0
        'b', '[^abc]' from rdb$database union all select      -- 0
        'd', '[^abc]' from rdb$database union all select
        '3', '[[:DIGIT:]^3]' from rdb$database union all select -- 0
        '4', '[[:DIGIT:]^3]' from rdb$database union all select
        '4', '[[:DIGIT:]]' from rdb$database union all select
        'a', '[[:DIGIT:]]' from rdb$database union all select   -- 0
        '4', '[^[:DIGIT:]]' from rdb$database union all select  -- 0
        'a', '[^[:DIGIT:]]' from rdb$database
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    STR                             WDWDWDWD
    PTN                             ((DW)|(WD)){4}
    str similar to %ptn%            1
    subs(str similar to %ptn%)      WDWDWDWD

    STR                             AAXYAAXYAAAAXYAAAXYAA
    PTN                             (AAXY|AAAX){2,}
    str similar to %ptn%            1
    subs(str similar to %ptn%)      AAXYAAXY

    STR                             YZZXYZZ0Z0YZZYZZYYZZZYZZ0Z0YZZ
    PTN                             (0Z0(Y|Z)*){2}
    str similar to %ptn%            1
    subs(str similar to %ptn%)      0Z0YZZYZZYYZZZYZZ0Z0YZZ

    STR                             AARARARAARARAR
    PTN                             RA(AR){3}
    str similar to %ptn%            1
    subs(str similar to %ptn%)      RAARARAR

    STR                             eiavieieav
    PTN                             (ie){2,}
    str similar to %ptn%            1
    subs(str similar to %ptn%)      ieie

    STR                             avieieavav
    PTN                             (av|ie){2,}
    str similar to %ptn%            1
    subs(str similar to %ptn%)      avieieavav

    STR                             avieieieav
    PTN                             ((av)|(ie)){2,}
    str similar to %ptn%            1
    subs(str similar to %ptn%)      avieieieav



    STR                             x/t
    PTN                             %[/]t
    str similar to ptn              1
    subs(str similar to ptn)        x/t

    STR                             2015-04-13
    PTN                             [[:DIGIT:]]{4}[-][[:DIGIT:]]{2}[-][[:DIGIT:]]{2}
    str similar to ptn              1
    subs(str similar to ptn)        2015-04-13

    STR                             WI-T3.0.0.31780 Firebird 3.0 Beta 2
    PTN                             %[0-9]+.[0-9]+.[0-9]+((.?[0-9]+)*)[[:WHITESPACE:]]%
    str similar to ptn              1
    subs(str similar to ptn)        WI-T3.0.0.31780 Firebird 3.0 Beta 2

    STR                             m
    PTN                             [p-k]
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             1
    PTN                             (1|2){0,}
    str similar to ptn              1
    subs(str similar to ptn)        1

    STR                             1
    PTN                             (1|2){0,1}
    str similar to ptn              1
    subs(str similar to ptn)        1

    STR                             1
    PTN                             (1|2){1}
    str similar to ptn              1
    subs(str similar to ptn)        1

    STR                             123
    PTN                             (1|12[3]?){1}
    str similar to ptn              1
    subs(str similar to ptn)        123

    STR                             123
    PTN                             (1|12[3]?)+
    str similar to ptn              1
    subs(str similar to ptn)        123

    STR                             ab
    PTN                             ab|cd|efg
    str similar to ptn              1
    subs(str similar to ptn)        ab

    STR                             efg
    PTN                             ab|cd|efg
    str similar to ptn              1
    subs(str similar to ptn)        efg

    STR                             a
    PTN                             ab|cd|efg
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR
    PTN                             a*
    str similar to ptn              1
    subs(str similar to ptn)

    STR                             a
    PTN                             a*
    str similar to ptn              1
    subs(str similar to ptn)        a

    STR                             aaa
    PTN                             a*
    str similar to ptn              1
    subs(str similar to ptn)        aaa

    STR
    PTN                             a+
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             a
    PTN                             a+
    str similar to ptn              1
    subs(str similar to ptn)        a

    STR                             aaa
    PTN                             a+
    str similar to ptn              1
    subs(str similar to ptn)        aaa

    STR
    PTN                             a?
    str similar to ptn              1
    subs(str similar to ptn)

    STR                             a
    PTN                             a?
    str similar to ptn              1
    subs(str similar to ptn)        a

    STR                             aaa
    PTN                             a?
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR
    PTN                             a{2,}
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             a
    PTN                             a{2,}
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             aa
    PTN                             a{2,}
    str similar to ptn              1
    subs(str similar to ptn)        aa

    STR                             aaa
    PTN                             a{2,}
    str similar to ptn              1
    subs(str similar to ptn)        aaa

    STR
    PTN                             a{2,4}
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             a
    PTN                             a{2,4}
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             aa
    PTN                             a{2,4}
    str similar to ptn              1
    subs(str similar to ptn)        aa

    STR                             aaa
    PTN                             a{2,4}
    str similar to ptn              1
    subs(str similar to ptn)        aaa

    STR                             aaaa
    PTN                             a{2,4}
    str similar to ptn              1
    subs(str similar to ptn)        aaaa

    STR                             aaaaa
    PTN                             a{2,4}
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR
    PTN                             _
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             a
    PTN                             _
    str similar to ptn              1
    subs(str similar to ptn)        a

    STR                             1
    PTN                             _
    str similar to ptn              1
    subs(str similar to ptn)        1

    STR                             a1
    PTN                             _
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR
    PTN                             %
    str similar to ptn              1
    subs(str similar to ptn)

    STR                             az
    PTN                             a%z
    str similar to ptn              1
    subs(str similar to ptn)        az

    STR                             a123z
    PTN                             a%z
    str similar to ptn              1
    subs(str similar to ptn)        a123z

    STR                             azx
    PTN                             a%z
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             ab
    PTN                             (ab){2}
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             aabb
    PTN                             (ab){2}
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             abab
    PTN                             (ab){2}
    str similar to ptn              1
    subs(str similar to ptn)        abab

    STR                             b
    PTN                             [abc]
    str similar to ptn              1
    subs(str similar to ptn)        b

    STR                             d
    PTN                             [abc]
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             9
    PTN                             [0-9]
    str similar to ptn              1
    subs(str similar to ptn)        9

    STR                             9
    PTN                             [0-8]
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             b
    PTN                             [^abc]
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             d
    PTN                             [^abc]
    str similar to ptn              1
    subs(str similar to ptn)        d

    STR                             3
    PTN                             [[:DIGIT:]^3]
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             4
    PTN                             [[:DIGIT:]^3]
    str similar to ptn              1
    subs(str similar to ptn)        4

    STR                             4
    PTN                             [[:DIGIT:]]
    str similar to ptn              1
    subs(str similar to ptn)        4

    STR                             a
    PTN                             [[:DIGIT:]]
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             4
    PTN                             [^[:DIGIT:]]
    str similar to ptn              0
    subs(str similar to ptn)        <null>

    STR                             a
    PTN                             [^[:DIGIT:]]
    str similar to ptn              1
    subs(str similar to ptn)        a
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

