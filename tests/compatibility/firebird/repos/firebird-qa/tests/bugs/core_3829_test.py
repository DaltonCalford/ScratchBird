#coding:utf-8

"""
ID:          issue-4171
ISSUE:       4171
TITLE:       Bad alias table choice joining CTE
DESCRIPTION:
JIRA:        CORE-3829
FBTEST:      bugs.core_3829
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3829.fbk')

test_script = """
    set list on;
    with totalk (kk1, variant, tt, qq, mm, ff, f1, f2, f3, f4, f5) as
    (
        select ll.kk1, ll.variant,
        sum (iif(ll.selector_y_n='Y', 1, 0)),
        count(*),      -- <<<< problem here
        -- count(1),   --  <<<< this doesn't work either
        -- sum(1),     --  <<<<< this neither
        -- sum(iif(ll.variant=1,1,1)), -- <<<< workaround
        sum (iif(ll.selector_m_f='M' and ll.selector_y_n='Y', 1, 0)),
        sum (iif(ll.selector_m_f='F' and ll.selector_y_n='Y', 1, 0)),
        sum(iif(ll.selector_1_5='1' and ll.selector_y_n='Y', 1, 0)),
        sum(iif(ll.selector_1_5='2' and ll.selector_y_n='Y', 1, 0)),
        sum(iif(ll.selector_1_5='3' and ll.selector_y_n='Y', 1, 0)),
        sum(iif(ll.selector_1_5='4' and ll.selector_y_n='Y', 1, 0)),
        sum(iif(ll.selector_1_5='5' and ll.selector_y_n='Y', 1, 0))
        from testcte ll
        group by 1, 2
    )
    select
        ff.kk1,
        ff.descrkk,
        sum(t1.tt) "TT 1",
        sum(t2.tt) "TT 2",
        sum(t1.qq) "QQ 1",
        sum(t2.qq) "QQ 2",
        sum(t1.mm) "MM 1",
        sum(t2.mm) "MM 2",
        sum(t1.ff) "FF 1",
        sum(t2.ff) "FF 2",
        sum(t1.f1) "G1 1",
        sum(t2.f1) "G1 2",
        sum(t1.f2) "G2 1",
        sum(t2.f2) "G2 2",
        sum(t1.f3) "G3 1",
        sum(t2.f3) "G3 2",
        sum(t1.f4) "G4 1",
        sum(t2.f4) "G4 2",
        sum(t1.f5) "G5 1",
        sum(t2.f5) "G5 2"
    from testmain ff
    left outer join totalk t1 on t1.kk1=ff.kk1 and t1.variant = 1
    left outer join totalk t2 on t2.kk1=ff.kk1 and t2.variant = 2
    group by 1, 2
    ;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """

    KK1                             A1
    DESCRKK                         A A A A A
    TT 1                            44
    TT 2                            52
    QQ 1                            103
    QQ 2                            111
    MM 1                            24
    MM 2                            23
    FF 1                            20
    FF 2                            29
    G1 1                            9
    G1 2                            10
    G2 1                            10
    G2 2                            5
    G3 1                            10
    G3 2                            13
    G4 1                            9
    G4 2                            13
    G5 1                            6
    G5 2                            11

    KK1                             A2
    DESCRKK                         AA AA AA
    TT 1                            41
    TT 2                            47
    QQ 1                            103
    QQ 2                            98
    MM 1                            25
    MM 2                            25
    FF 1                            16
    FF 2                            22
    G1 1                            7
    G1 2                            10
    G2 1                            9
    G2 2                            8
    G3 1                            7
    G3 2                            7
    G4 1                            9
    G4 2                            10
    G5 1                            9
    G5 2                            12

    KK1                             A3
    DESCRKK                         AAA AAA
    TT 1                            55
    TT 2                            47
    QQ 1                            107
    QQ 2                            114
    MM 1                            26
    MM 2                            20
    FF 1                            29
    FF 2                            27
    G1 1                            5
    G1 2                            13
    G2 1                            10
    G2 2                            7
    G3 1                            14
    G3 2                            11
    G4 1                            12
    G4 2                            6
    G5 1                            14
    G5 2                            10

    KK1                             A4
    DESCRKK                         AAAA
    TT 1                            56
    TT 2                            56
    QQ 1                            113
    QQ 2                            109
    MM 1                            31
    MM 2                            31
    FF 1                            25
    FF 2                            25
    G1 1                            10
    G1 2                            16
    G2 1                            11
    G2 2                            9
    G3 1                            10
    G3 2                            10
    G4 1                            10
    G4 2                            11
    G5 1                            15
    G5 2                            10

    KK1                             A5
    DESCRKK                         AAAAA
    TT 1                            55
    TT 2                            51
    QQ 1                            118
    QQ 2                            117
    MM 1                            29
    MM 2                            27
    FF 1                            26
    FF 2                            24
    G1 1                            15
    G1 2                            6
    G2 1                            10
    G2 2                            13
    G3 1                            10
    G3 2                            5
    G4 1                            10
    G4 2                            12
    G5 1                            10
    G5 2                            15

    KK1                             A6
    DESCRKK                         AAAAAA
    TT 1                            <null>
    TT 2                            <null>
    QQ 1                            <null>
    QQ 2                            <null>
    MM 1                            <null>
    MM 2                            <null>
    FF 1                            <null>
    FF 2                            <null>
    G1 1                            <null>
    G1 2                            <null>
    G2 1                            <null>
    G2 2                            <null>
    G3 1                            <null>
    G3 2                            <null>
    G4 1                            <null>
    G4 2                            <null>
    G5 1                            <null>
    G5 2                            <null>

    KK1                             B1
    DESCRKK                         B B B B B
    TT 1                            69
    TT 2                            72
    QQ 1                            116
    QQ 2                            129
    MM 1                            33
    MM 2                            46
    FF 1                            36
    FF 2                            26
    G1 1                            15
    G1 2                            14
    G2 1                            9
    G2 2                            14
    G3 1                            13
    G3 2                            19
    G4 1                            15
    G4 2                            17
    G5 1                            17
    G5 2                            8

    KK1                             B2
    DESCRKK                         BB BB BB
    TT 1                            58
    TT 2                            61
    QQ 1                            118
    QQ 2                            124
    MM 1                            24
    MM 2                            27
    FF 1                            34
    FF 2                            34
    G1 1                            10
    G1 2                            15
    G2 1                            10
    G2 2                            14
    G3 1                            14
    G3 2                            10
    G4 1                            16
    G4 2                            8
    G5 1                            8
    G5 2                            14

    KK1                             B3
    DESCRKK                         BBB BBB
    TT 1                            54
    TT 2                            62
    QQ 1                            126
    QQ 2                            115
    MM 1                            32
    MM 2                            28
    FF 1                            22
    FF 2                            34
    G1 1                            16
    G1 2                            14
    G2 1                            10
    G2 2                            10
    G3 1                            6
    G3 2                            9
    G4 1                            12
    G4 2                            14
    G5 1                            10
    G5 2                            15

    KK1                             B4
    DESCRKK                         BBBB
    TT 1                            <null>
    TT 2                            <null>
    QQ 1                            <null>
    QQ 2                            <null>
    MM 1                            <null>
    MM 2                            <null>
    FF 1                            <null>
    FF 2                            <null>
    G1 1                            <null>
    G1 2                            <null>
    G2 1                            <null>
    G2 2                            <null>
    G3 1                            <null>
    G3 2                            <null>
    G4 1                            <null>
    G4 2                            <null>
    G5 1                            <null>
    G5 2                            <null>

    KK1                             B5
    DESCRKK                         BBBBB
    TT 1                            <null>
    TT 2                            <null>
    QQ 1                            <null>
    QQ 2                            <null>
    MM 1                            <null>
    MM 2                            <null>
    FF 1                            <null>
    FF 2                            <null>
    G1 1                            <null>
    G1 2                            <null>
    G2 1                            <null>
    G2 2                            <null>
    G3 1                            <null>
    G3 2                            <null>
    G4 1                            <null>
    G4 2                            <null>
    G5 1                            <null>
    G5 2                            <null>

    KK1                             B6
    DESCRKK                         BBBBBB
    TT 1                            <null>
    TT 2                            <null>
    QQ 1                            <null>
    QQ 2                            <null>
    MM 1                            <null>
    MM 2                            <null>
    FF 1                            <null>
    FF 2                            <null>
    G1 1                            <null>
    G1 2                            <null>
    G2 1                            <null>
    G2 2                            <null>
    G3 1                            <null>
    G3 2                            <null>
    G4 1                            <null>
    G4 2                            <null>
    G5 1                            <null>
    G5 2                            <null>

    KK1                             C1
    DESCRKK                         C C C C C
    TT 1                            60
    TT 2                            65
    QQ 1                            117
    QQ 2                            121
    MM 1                            36
    MM 2                            32
    FF 1                            24
    FF 2                            33
    G1 1                            11
    G1 2                            18
    G2 1                            7
    G2 2                            9
    G3 1                            16
    G3 2                            17
    G4 1                            14
    G4 2                            9
    G5 1                            12
    G5 2                            12

    KK1                             C2
    DESCRKK                         CC CC CC
    TT 1                            66
    TT 2                            60
    QQ 1                            123
    QQ 2                            113
    MM 1                            33
    MM 2                            29
    FF 1                            33
    FF 2                            31
    G1 1                            18
    G1 2                            12
    G2 1                            12
    G2 2                            15
    G3 1                            12
    G3 2                            9
    G4 1                            12
    G4 2                            14
    G5 1                            12
    G5 2                            10

    KK1                             C3
    DESCRKK                         CCC CCC
    TT 1                            57
    TT 2                            76
    QQ 1                            123
    QQ 2                            134
    MM 1                            34
    MM 2                            33
    FF 1                            23
    FF 2                            43
    G1 1                            15
    G1 2                            11
    G2 1                            13
    G2 2                            22
    G3 1                            8
    G3 2                            15
    G4 1                            11
    G4 2                            16
    G5 1                            10
    G5 2                            12

    KK1                             C4
    DESCRKK                         CCCC
    TT 1                            66
    TT 2                            52
    QQ 1                            121
    QQ 2                            102
    MM 1                            35
    MM 2                            27
    FF 1                            31
    FF 2                            25
    G1 1                            11
    G1 2                            11
    G2 1                            16
    G2 2                            13
    G3 1                            13
    G3 2                            10
    G4 1                            12
    G4 2                            11
    G5 1                            14
    G5 2                            7

    KK1                             C5
    DESCRKK                         CCCCC
    TT 1                            58
    TT 2                            51
    QQ 1                            113
    QQ 2                            112
    MM 1                            29
    MM 2                            17
    FF 1                            29
    FF 2                            34
    G1 1                            8
    G1 2                            15
    G2 1                            14
    G2 2                            8
    G3 1                            11
    G3 2                            11
    G4 1                            12
    G4 2                            6
    G5 1                            13
    G5 2                            11

    KK1                             C6
    DESCRKK                         CCCCCC
    TT 1                            <null>
    TT 2                            <null>
    QQ 1                            <null>
    QQ 2                            <null>
    MM 1                            <null>
    MM 2                            <null>
    FF 1                            <null>
    FF 2                            <null>
    G1 1                            <null>
    G1 2                            <null>
    G2 1                            <null>
    G2 2                            <null>
    G3 1                            <null>
    G3 2                            <null>
    G4 1                            <null>
    G4 2                            <null>
    G5 1                            <null>
    G5 2                            <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

