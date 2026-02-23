#coding:utf-8

"""
ID:          issue-4772
ISSUE:       4772
TITLE:       Can`t create two collations with different names if autoddl OFF in FB 2.5.3
DESCRIPTION:
JIRA:        CORE-4452
FBTEST:      bugs.core_4452
"""

import pytest
from firebird.qa import *

substitutions = [
                   ('SPECIFIC_ATTR_BLOB_ID.*', 'SPECIFIC_ATTR_BLOB_ID')
                  ,('COLL-VERSION=\\d+.\\d+(;ICU-VERSION=\\d+.\\d+)?.*', 'COLL-VERSION=<attr>')
                ]

db = db_factory()

test_script = """
    set count on;
    set list on;
    set blob all;

    create or alter view v_info as
    select
        rc.rdb$collation_name
        ,rc.rdb$collation_attributes
        ,rc.rdb$base_collation_name
        ,rc.rdb$specific_attributes as specific_attr_blob_id
        ,rs.rdb$character_set_name
        ,rs.rdb$number_of_characters
        ,rs.rdb$bytes_per_character
    from rdb$collations rc
    join rdb$character_sets rs on rc.rdb$character_set_id = rs.rdb$character_set_id
    where
        rc.rdb$system_flag is distinct from 1
    ;
    commit;

    set autoddl off;
    commit;

    create collation name_coll for utf8 from unicode CASE INSENSITIVE;
    create collation nums_coll for utf8 from unicode CASE INSENSITIVE 'NUMERIC-SORT=1';
    commit;
    select 'point-1' as msg, v.* from v_info v;

    drop collation name_coll;
    drop collation nums_coll;
    commit;
    create collation name_coll for utf8 from unicode CASE INSENSITIVE;
    create collation nums_coll for utf8 from unicode CASE INSENSITIVE 'NUMERIC-SORT=1';
    commit;

    select 'point-2' as msg, v.* from v_info v;

    drop collation name_coll;
    drop collation nums_coll;
    commit;

    select 'point-3' as msg, v.* from v_info v;
"""

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """
    MSG                             point-1
    RDB$COLLATION_NAME              NAME_COLL
    RDB$COLLATION_ATTRIBUTES        3
    RDB$BASE_COLLATION_NAME         UNICODE
    SPECIFIC_ATTR_BLOB_ID           1d:1eb
    COLL-VERSION=153.88;ICU-VERSION=63.1
    RDB$CHARACTER_SET_NAME          UTF8
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$BYTES_PER_CHARACTER         4

    MSG                             point-1
    RDB$COLLATION_NAME              NUMS_COLL
    RDB$COLLATION_ATTRIBUTES        3
    RDB$BASE_COLLATION_NAME         UNICODE
    SPECIFIC_ATTR_BLOB_ID           1d:1ec
    COLL-VERSION=153.88;ICU-VERSION=63.1;NUMERIC-SORT=1
    RDB$CHARACTER_SET_NAME          UTF8
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$BYTES_PER_CHARACTER         4
    Records affected: 2

    MSG                             point-2
    RDB$COLLATION_NAME              NAME_COLL
    RDB$COLLATION_ATTRIBUTES        3
    RDB$BASE_COLLATION_NAME         UNICODE
    SPECIFIC_ATTR_BLOB_ID           1d:1ed
    COLL-VERSION=153.88;ICU-VERSION=63.1
    RDB$CHARACTER_SET_NAME          UTF8
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$BYTES_PER_CHARACTER         4

    MSG                             point-2
    RDB$COLLATION_NAME              NUMS_COLL
    RDB$COLLATION_ATTRIBUTES        3
    RDB$BASE_COLLATION_NAME         UNICODE
    SPECIFIC_ATTR_BLOB_ID           1d:1ee
    COLL-VERSION=153.88;ICU-VERSION=63.1;NUMERIC-SORT=1
    RDB$CHARACTER_SET_NAME          UTF8
    RDB$NUMBER_OF_CHARACTERS        <null>
    RDB$BYTES_PER_CHARACTER         4
    Records affected: 2

    Records affected: 0
"""

expected_stderr = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

