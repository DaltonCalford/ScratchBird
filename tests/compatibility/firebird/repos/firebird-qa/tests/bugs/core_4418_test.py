#coding:utf-8

"""
ID:          issue-4740
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/4740
TITLE:       Regression: Can not run ALTER TABLE DROP CONSTRAINT <FK_name> after recent changes in svn
DESCRIPTION:
JIRA:        CORE-4418
FBTEST:      bugs.core_4418
NOTES:
    [28.04.2022] pzotov
    Comfirmed problem on 3.0.0.31099.
    Checked on 5.0.0.488, 4.0.1.2692, 3.0.8.33535.

    [05.10.2023] pzotov
    Removed SHOW command. It is enough for this test just to show 'Completed' message when all FK have been dropped because set bail = ON.
    Checked on 6.0.0.66, 5.0.0.1235, 4.0.4.2998, 3.0.12.33713.
"""

import pytest
from firebird.qa import *

substitutions = [    
                   ('SPECIFIC_ATTR_BLOB_ID.*', 'SPECIFIC_ATTR_BLOB_ID')
                  ,('COLL-VERSION=\\d+.\\d+(;ICU-VERSION=\\d+.\\d+)?.*', 'COLL-VERSION=<attr>')
                ]


db = db_factory(charset='UTF8')

test_script = """
    set bail on;
    set heading off;

    /*
    create or alter view v_coll_info as
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
    */

    create collation nums_coll for utf8 from unicode case insensitive 'NUMERIC-SORT=1';
    create domain dm_nums as varchar(20) character set utf8 collate nums_coll;
    create domain dm_ids as bigint;
    commit;

    recreate table tm(
      id dm_ids,
      nm dm_nums,
      constraint tm_pk primary key(id)
    );

    recreate table td(
      id dm_ids,
      pid dm_ids,
      nm dm_nums,
      constraint td_pk primary key(id),
      constraint td_fk foreign key(pid) references tm
    );

    set autoddl off;
    commit;

    alter table td drop constraint td_fk;
    alter table td drop constraint td_pk;
    alter table tm drop constraint tm_pk;
    commit;

    select 'Completed.' from rdb$database;
"""

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """
    Completed.
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

