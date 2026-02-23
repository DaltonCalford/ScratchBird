#coding:utf-8

"""
ID:          issue-4331
ISSUE:       4331
TITLE:       Bug in unique constraints and / or NUMERIC-SORT collation
DESCRIPTION:
JIRA:        CORE-3999
FBTEST:      bugs.core_3999
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table products(productno char(1));
    commit;
    set term ^;
    execute block as
    begin
      execute statement 'drop collation unicode_num_ci_ai';
      when any do begin end
    end^
    set term ;^
    commit;
    create collation unicode_num_ci_ai for utf8 from unicode_ci_ai 'numeric-sort=1';
    recreate table products(productno varchar(100) character set utf8 collate unicode_num_ci_ai);
    alter table products add constraint uk_products unique (productno) using index uk_products;
    commit;
    insert into products values('s01');
    insert into products values('s1');
    insert into products values('01');
    insert into products values('001');
    insert into products values('-01');
    insert into products values('-001');
    commit;
    set list on;
    select distinct productno from products;
"""

act = isql_act('db', test_script)

expected_stdout = """
    PRODUCTNO                       -001
    PRODUCTNO                       -01
    PRODUCTNO                       001
    PRODUCTNO                       01
    PRODUCTNO                       s01
    PRODUCTNO                       s1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

