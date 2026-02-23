#coding:utf-8

"""
ID:          issue-3192
ISSUE:       3192
TITLE:       Problems with COMMENT ON and strings using introducer (charset)
DESCRIPTION:
JIRA:        CORE-2804
FBTEST:      bugs.core_2804
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core2804-ods11.fbk')

test_script = """
    -- Database was prepared with following statements:
    -- recreate table test1(id int);
    -- recreate table test2(id int);
    -- commit;
    -- comment on table test1 is _win1251 'win1251: <cyrillic text here encoded with win1251>';
    -- comment on table test2 is _unicode_fss 'unicode_fss: <cyrillic text here encoded with UTF8>';
    -- commit;

    set blob all;
    set list on;
    select r.rdb$description as descr_blob_id
    from rdb$relations r where r.rdb$relation_name in ( upper('test1'), upper('test2') )
    order by r.rdb$relation_name;
"""

act = isql_act('db', test_script, substitutions=[('DESCR_BLOB_ID.*', '')])

expected_stdout = """
    DESCR_BLOB_ID                   0:3
    win1251: Протокол собрания об уплотнении квартир дома

    DESCR_BLOB_ID                   0:6
    unicode_fss: Протокол собрания о помощи детям Германии
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

