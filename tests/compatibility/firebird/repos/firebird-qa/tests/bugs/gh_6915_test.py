#coding:utf-8

"""
ID:          issue-6915
ISSUE:       6915
TITLE:       Allow attribute DISABLE-COMPRESSIONS in UNICODE collations
DESCRIPTION:
    Original discussion:
    https://sourceforge.net/p/firebird/mailman/firebird-devel/thread/9361c612-d720-eb76-d412-7101518ca60d%40ibphoenix.cz/

    Only ability to use 'DISABLE-COMPRESSION' in attributes list is checked here.
    Performance comparison with and without this attribute will be checked in separate test.
NOTES:
    [24.12.2024] pzotov
    Several tests have been added in order to check PERFORMANCE affect of 'DISABLE-COMPRESSIONS=1':
    * bugs/gh_6915_cs_cz_test.py
    * bugs/gh_6915_hu_hu_test.py
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create collation coll_cs_dc
       for UTF8
       from UNICODE
       case sensitive
       'LOCALE=cs_CZ;DISABLE-COMPRESSIONS=1'
    ;

    create collation coll_ci_dc
       for UTF8
       from UNICODE
       case insensitive
       'LOCALE=cs_CZ;DISABLE-COMPRESSIONS=1'
    ;

    create collation coll_cs_dc_ns
       for UTF8
       from UNICODE
       case sensitive
       'LOCALE=cs_CZ;DISABLE-COMPRESSIONS=1;NUMERIC-SORT=1'
    ;

    create collation coll_ci_dc_ns
       for UTF8
       from UNICODE
       case insensitive
       'LOCALE=cs_CZ;DISABLE-COMPRESSIONS=1;NUMERIC-SORT=1'
    ;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.intl
@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
