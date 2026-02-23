#coding:utf-8

"""
ID:          issue-5500
ISSUE:       5500
TITLE:       ISQL -X: double quotes are missed for COLLATE <C> of CREATE DOMAIN statement
  when <C> is from any non-ascii charset
DESCRIPTION:
  We create in init_script two collations with non-ascii names and two varchar domains which use these collations.
  Then we extract metadata and save it to file as .sql script to be applied further.
  This script should contain CORRECT domains definition, i.e. collations should be enclosed in double quotes.
  We check correctness by removing from database all objects and applying this script: no errors should occur at that point.
  Then we extract metadata second time, store it to second .sql and COMPARE this file with result of first metadata extraction.
  These files should be equal, i.e. difference should be empty.
JIRA:        CORE-5220
FBTEST:      bugs.core_5220
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from difflib import unified_diff
from firebird.qa import *

init_script = """
    create collation "Циферки" for utf8 from unicode case insensitive 'NUMERIC-SORT=1';
    create collation "Испания" for iso8859_1 from es_es_ci_ai 'SPECIALS-FIRST=1';
    commit;
    create domain "Артикулы" varchar(12) character set utf8 collate "Циферки";
    create domain "Комрады" varchar(40) character set iso8859_1 collate "Испания";
    commit;
"""

db = db_factory(init=init_script, charset='UTF8')

act = python_act('db')

expected_stdout = """
    Records affected: 0
    Records affected: 0
"""

remove_metadata = """
    drop domain "Комрады";
    drop domain "Артикулы";
    drop collation "Испания";
    drop collation "Циферки";
    commit;

    set list on;
    set count on;
    select f.rdb$field_name
    from rdb$fields f
    where
        f.rdb$system_flag is distinct from 1
        and f.rdb$field_name not starting with upper('rdb$');

    select r.rdb$collation_name
    from rdb$collations r
    where
        r.rdb$system_flag is distinct from 1;
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    #
    act.isql(switches=['-x'], combine_output = True)
    metadata = act.stdout
    act.reset()

    # Remove metadata
    act.expected_stdout = expected_stdout
    act.isql(switches=[], input=remove_metadata, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # Apply metadata
    act.isql(switches=[], input=metadata, combine_output = True)
    act.reset()

    # Extract metadatata again
    act.isql(switches=['-x'], combine_output = True)

    # Check metadata
    meta_diff = list(unified_diff(metadata.splitlines(), act.stdout.splitlines()))
    assert meta_diff == []
