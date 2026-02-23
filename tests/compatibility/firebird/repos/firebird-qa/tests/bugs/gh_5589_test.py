#coding:utf-8

"""
ID:          issue-5589
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5589
TITLE:       Support full SQL standard character string literal syntax [CORE5312]
DESCRIPTION:
JIRA:        CORE-5312
NOTES:
    [15.09.2024] pzotov
    Commit (13.05.2021): 
    https://github.com/FirebirdSQL/firebird/commit/8a7927aac4fef3740e54b7941146b6d044b864b1
    
    Checked on 6.0.0.457, 5.0.2.1499
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    set list on;
    select 'ab' 'cd'		'ef' as good_chr_01 from rdb$database;
    select 'ab'/*comment*/	'cd'		/**/ 'ef'  as good_chr_02 from rdb$database;
    select 'ab'/* foo
    bar */'cd'
    ''
    /*
    */

    'ef'  as good_chr_03 from rdb$database;

    select 'ab' -- foo
    'cd' -- bar
    'ef' as good_chr_04 from rdb$database;

    select'ab'
    'cd'
    'ef' as good_chr_05 from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    GOOD_CHR_01 abcdef
    GOOD_CHR_02 abcdef
    GOOD_CHR_03 abcdef
    GOOD_CHR_04 abcdef
    GOOD_CHR_05 abcdef
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
