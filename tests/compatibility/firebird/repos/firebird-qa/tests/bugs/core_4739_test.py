#coding:utf-8

"""
ID:          issue-5044
ISSUE:       5044
TITLE:       Accent insensitive comparison: diacritical letters with DIAGONAL crossing
  stroke pass only test on EQUALITY to their non-accented forms
DESCRIPTION:
JIRA:        CORE-4739
FBTEST:      bugs.core_4739
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    create collation co_utf8_ci_ai for utf8 from unicode case insensitive accent insensitive;
    commit;

    set list on;
    set count on;

    with recursive
    d as (
        select
         cast( 'ḂĊḊḞĠṀṖṠṪ' || 'ẀẂŴŶ' || 'ŠŽ' || 'Ø' || 'Ð' || 'Ł' || 'Ŀ' || 'ĘĄĂÂÎŢŐŰĖÅĽĢÁÉÍÓÚÝÀÈÌÒÙÂÊÎÔÛÃÑÕÄËÏÖÜŸÇŠĄĘŹŻĂŞŢ' as varchar(80) character set utf8) s
        ,cast( 'BCDFGMPST' || 'WWWY' || 'SZ' || 'O' || 'D' || 'L' || 'L' || 'EAAAITOUEALGAEIOUYAEIOUAEIOUANOAEIOUYCSAEZZAST' as varchar(80) character set utf8) ascii_only_char
        from rdb$database
    )
    ,r as(select 1 i from rdb$database union all select r.i+1 from r where r.i < 100)
    ,e as(
        select
             substring(d.s from r.i for 1) non_ascii_char
            ,substring(d.ascii_only_char from r.i for 1) ascii_only_char
        from d join r on r.i <= char_length(d.s)
    )
    ,f as (
        select
             e.non_ascii_char --as utf_char
            ,e.ascii_only_char
            ,iif( e.non_ascii_char collate co_utf8_ci_ai = e.ascii_only_char, 1, 0 ) equal_test
            ,iif( e.non_ascii_char collate co_utf8_ci_ai between e.ascii_only_char and e.ascii_only_char, 1, 0 ) between_test
            ,iif( position(e.ascii_only_char, e.non_ascii_char collate co_utf8_ci_ai) >0 , 1, 0 ) pos_test
            ,iif( e.non_ascii_char collate co_utf8_ci_ai starting with e.ascii_only_char, 1, 0 ) start_with_test
            ,iif( e.non_ascii_char collate co_utf8_ci_ai like e.ascii_only_char, 1, 0 ) like_test
            ,iif( e.non_ascii_char collate co_utf8_ci_ai containing e.ascii_only_char, 1, 0 ) containing_test
            ,iif( e.non_ascii_char collate co_utf8_ci_ai similar to e.ascii_only_char, 1, 0 ) similar_to_letter_test
            ,iif( e.non_ascii_char collate co_utf8_ci_ai similar to '[[:ALPHA:]]', 1, 0 ) similar_to_alpha_test
        from e
    )
    select *
    from f
    where minvalue( equal_test, between_test, pos_test, start_with_test, like_test, containing_test, similar_to_letter_test, similar_to_alpha_test  ) = 0
    order by non_ascii_char
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

