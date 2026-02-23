#coding:utf-8

"""
ID:          issue-4688
ISSUE:       4688
TITLE:       Wrong result of WHERE predicate when it contains NULL IS NOT DISTINCT FROM (select min(NULL) from ...)
DESCRIPTION:
JIRA:        CORE-4366
FBTEST:      bugs.core_4366
"""

import pytest
from firebird.qa import *

init_script = """
recreate table tf(id int primary key, nm varchar(3)); commit;
insert into tf values(5, 'qwe');
insert into tf values(1, 'rty');
insert into tf values(2, 'asd');
insert into tf values(4, 'fgh');
insert into tf values(3, 'mnb');
insert into tf values(7, 'bvc');
insert into tf values(9, 'zxc');
insert into tf values(0, 'lkj');
insert into tf values(6, 'oiu');
insert into tf values(8, 'fgh');
commit;
"""

db = db_factory(init=init_script)

test_script = """
select nm from tf where null is not distinct from (select min(null) from tf) order by id rows 10;
"""

act = isql_act('db', test_script)

expected_stdout = """
NM
======
lkj
rty
asd
mnb
fgh
qwe
oiu
bvc
fgh
zxc
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

