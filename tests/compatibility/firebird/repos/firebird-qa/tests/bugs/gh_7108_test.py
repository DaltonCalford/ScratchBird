#coding:utf-8

"""
ID:          issue-7108
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7108
TITLE:       Firebird does not find an record when adding a foreign key
NOTES:
    [01.03.2023] pzotov
    Confirmed bug on 4.0.1.2701; fixed on 4.0.1.2711
    Checked on 4.0.3.2904, 5.0.0.964.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table main (str varchar(1) character set utf8);
    commit;

    insert into main(str) values ('X');
    commit;

    create table ref (id varchar(1) character set utf8 not null primary key);
    commit;

    insert into ref(id) values ('1');
    commit;

    alter table main add ref_id varchar(1) character set utf8 default '1' not null;
    commit;

    alter table main add constraint FK_MAIN foreign key (ref_id) references ref (id);
"""

act = isql_act('db', test_script)

""

expected_stdout = """
"""

@pytest.mark.version('>=4.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
