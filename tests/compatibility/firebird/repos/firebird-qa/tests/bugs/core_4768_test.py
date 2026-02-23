#coding:utf-8

"""
ID:          issue-5068
ISSUE:       5068
TITLE:       CREATE USER ... TAGS ( argument_1 = 'value1', ..., argument_N = 'valueN' ) -
  wrong results of statement when there are many arguments
DESCRIPTION:
JIRA:        CORE-4768
FBTEST:      bugs.core_4768
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
        USR_NAME                        TMP$C4768_1
        SEC_PLUGIN                      Srp
        TAG_MIN                         ARG_0
        VAL_MIN                         VAL0
        TAG_MAX                         ARG_99999
        VAL_MAX                         VAL99999
        TAG_CNT                         100000
        Records affected: 1
"""

# Cleanup fixture
tmp_user = user_factory('db', name='tmp$c4768_1', password='123', plugin='Srp',
                        do_not_create=True)

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
    TAGS_COUNT = 100000
    check_lines = ['set bail on;',
                   "create or alter user tmp$c4768_1 password '123' using plugin Srp tags ("]
    for i in range(TAGS_COUNT):
        check_lines.append(f"{'  ,' if i > 0 else '  '}arg_{i}='val{i}'")
    check_lines.append(');')
    check_lines.append('commit;')
    test_script = '\n'.join(check_lines) + """
    set count on;
    set list on;
    select
         u.sec$user_name as usr_name
        ,u.sec$plugin sec_plugin
        ,upper(min( a.sec$key )) tag_min
        ,upper(min( a.sec$value )) val_min
        ,upper(max( a.sec$key )) tag_max
        ,upper(max( a.sec$value )) val_max
        ,count(*) tag_cnt
    from sec$users u
    left join sec$user_attributes a on u.sec$user_name = a.sec$user_name
    where u.sec$user_name = upper('tmp$c4768_1')
    group by 1,2 ;
    commit;
"""
    act.expected_stdout = expected_stdout
    act .isql(switches=['-q'], input=test_script)
    assert act.clean_stdout == act.clean_expected_stdout
