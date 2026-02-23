#coding:utf-8

"""
ID:          issue-5067
ISSUE:       5067
TITLE:       CREATE USER ... TAGS ( attr = 'prefix #suffix' ): suffix will be removed from
  storage because of character '#' in the value of attribute
DESCRIPTION:
JIRA:        CORE-4767
FBTEST:      bugs.core_4767
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create or alter view v_user_tags as
    select
         a.sec$key tag
        ,a.sec$value val
    from sec$users u
    left join sec$user_attributes a on u.sec$user_name = a.sec$user_name
    where upper(u.sec$user_name) = upper('tmp$c4767') and upper(u.sec$plugin) = upper('Srp')
    ;
    commit;

    create or alter user tmp$c4767 password '123' using plugin Srp  tags (
        attr00001='attr #00001'
       ,attr00002='#'
       ,attr00003='##'
       ,attr00004='$'
       ,attr00005='$$'
    );
    commit;

    select * from v_user_tags;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    TAG                             ATTR00001
    VAL                             attr #00001

    TAG                             ATTR00002
    VAL                             #

    TAG                             ATTR00003
    VAL                             ##

    TAG                             ATTR00004
    VAL                             $

    TAG                             ATTR00005
    VAL                             $$
"""

# cleanup fixture
tmp_user=user_factory('db', name='tmp$c4767', password='123', plugin='Srp',
                      do_not_create=True)

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

