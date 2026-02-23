#coding:utf-8

"""
ID:          issue-2177
ISSUE:       2177
TITLE:       Results of a join with different collations depend on the execution plan
DESCRIPTION:
JIRA:        CORE-1752
FBTEST:      bugs.core_1752
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core1752.fbk')

test_script = """
    select a.zeitstempel, b.artikelnr
    from t1 a
    join t2 b on b.artikelnr = a.artikelnr --collate de_de
    order by 1,2
    ;

    select a.zeitstempel, b.artikelnr
    from t1 a
    join t2 b on b.artikelnr = a.artikelnr collate de_de
    order by 1,2
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
ZEITSTEMPEL                      ARTIKELNR
================================ ===============
11997831915bde60630658f7ed147baa           10045
1199797956ebe80ac53a54ebd164c707            9930
1199798232de53f704854945c17be47e           10005
1199799582060cc10385b2eafcdfd567           10045
1199799582060cc10385b2eafcdfd567           10056
119981802446bba36e437bf0181bb41c            9761
119981811832e8ec4bb875a5d54ca202            9510
1199823157b3f2d13b93184ab1e3b29d            9703
1199827557240a59c5148b42a9374fc9            9920


ZEITSTEMPEL                      ARTIKELNR
================================ ===============
11997831915bde60630658f7ed147baa           10045
1199797956ebe80ac53a54ebd164c707            9930
1199798232de53f704854945c17be47e           10005
1199799582060cc10385b2eafcdfd567           10045
1199799582060cc10385b2eafcdfd567           10056
119981802446bba36e437bf0181bb41c            9761
119981811832e8ec4bb875a5d54ca202            9510
1199823157b3f2d13b93184ab1e3b29d            9703
1199827557240a59c5148b42a9374fc9            9920
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

