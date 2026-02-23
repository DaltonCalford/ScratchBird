#coding:utf-8

"""
ID:          issue-3645
ISSUE:       3645
TITLE:       Wrong result for RIGHT(UTF8 varchar)
DESCRIPTION: Text was taken from Gutenberg project, several European languages are used
JIRA:        CORE-3277
FBTEST:      bugs.core_3277
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core3277.fbk')

test_script = """
     set list on;
     select s,left(s,10) ls10, right(s,10) rs10 from t;
"""

act = isql_act('db', test_script)

expected_stdout = """
S                               moří; že zplodí bezpočtu hrdin, kteří ponesou svou hořící duši
LS10                            moří; že z
RS10                            ořící duši

S                               käsiimme. Jospa vaan tapaisin heidän päällikkönsä!
LS10                            käsiimme.
RS10                            llikkönsä!

S                               την καιροσκόπον πολιτικήν, όπως ερρύθμιζον αυτήν ηνωμένοι
LS10                            την καιροσ
RS10                            ν ηνωμένοι

S                               Devido à existência de erros tipográficos neste
LS10                            Devido à e
RS10                            icos neste

S                               На секунду он почувствовал, что разделяет чувство Агафьи
LS10                            На секунду
RS10                            тво Агафьи

S                               Domin: (běží ke krbu) Spálila! (poklekne ke krbu a přehrabává v něm) Nic
LS10                            Domin: (bě
RS10                            v něm) Nic

S                               Päällikkö karkasi kiinni läheisempään ja mättäsi hänet alas
LS10                            Päällikkö
RS10                            hänet alas

S                               Sa du något?
LS10                            Sa du någo
RS10                             du något?

S                               Ja, hvarför? Hur många lugna timmar tror du jag har haft på tretti år?
LS10                            Ja, hvarfö
RS10                            tretti år?

S                               Αφού δε απέθανεν η προφορική ποίησις — ίσως κατά τον τέταρτον
LS10                            Αφού δε απ
RS10                            ν τέταρτον

S                               Busman: Počet. Udělali jsme Robotů příliš mnoho. Namoutě, to se přece dalo
LS10                            Busman: Po
RS10                            přece dalo

S                               Men dansken sa _nej_, han. För Vår Herre och fotografen, sa han, ä
LS10                            Men danske
RS10                             sa han, ä
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(charset='utf8')
    assert act.clean_stdout == act.clean_expected_stdout

