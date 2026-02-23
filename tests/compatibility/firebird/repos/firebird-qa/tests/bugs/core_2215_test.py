#coding:utf-8

"""
ID:          issue-2643
ISSUE:       2643
TITLE:       GROUP BY concatenation with empty string
DESCRIPTION:
JIRA:        CORE-2215
FBTEST:      bugs.core_2215
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table attributes_dictionary (
        id integer not null,
        name varchar(25)
    );
    insert into attributes_dictionary (id, name) values (1,'attr1');
    insert into attributes_dictionary (id, name) values (2,'attr1');
    insert into attributes_dictionary (id, name) values (3,'attr2');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select atr.name as name_01, count(*) as cnt_01
    from attributes_dictionary atr
    group by 1 order by 2 desc ;

    select atr.name||'text' as name_02, count(*) as cnt_02
    from attributes_dictionary atr
    group by 1 order by 2 desc ;

    select atr.name||'' as name_03, count(*) as cnt_03
    from attributes_dictionary atr
    group by 1 order by 2 desc ;

    select atr.name||'' as name_04, count(*) as cnt_04
    from attributes_dictionary atr
    group by atr.name||'' order by count(*) desc ;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    NAME_01 attr1
    CNT_01 2
    
    NAME_01 attr2
    CNT_01 1
    
    
    NAME_02 attr1text
    CNT_02 2
    
    NAME_02 attr2text
    CNT_02 1
    
    
    NAME_03 attr1
    CNT_03 2
    
    NAME_03 attr2
    CNT_03 1
    
    
    NAME_04 attr1
    CNT_04 2
    
    NAME_04 attr2
    CNT_04 1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

