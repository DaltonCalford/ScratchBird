#coding:utf-8

"""
ID:          issue-1528
ISSUE:       1528
TITLE:       Wrong results with GROUP BY on constants
DESCRIPTION:
JIRA:        CORE-1108
FBTEST:      bugs.core_1108
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table employee (
        emp_no smallint,
        job_country varchar(15)
    );

    insert into employee values (2, 'usa');
    insert into employee values (4, 'usa');
    insert into employee values (5, 'usa');
    insert into employee values (8, 'usa');
    insert into employee values (9, 'usa');
    insert into employee values (11, 'usa');
    insert into employee values (12, 'usa');
    insert into employee values (14, 'usa');
    insert into employee values (15, 'usa');
    insert into employee values (20, 'usa');
    insert into employee values (24, 'usa');
    insert into employee values (28, 'england');
    insert into employee values (29, 'usa');
    insert into employee values (34, 'usa');
    insert into employee values (36, 'england');
    insert into employee values (37, 'england');
    insert into employee values (44, 'usa');
    insert into employee values (45, 'usa');
    insert into employee values (46, 'usa');
    insert into employee values (52, 'usa');
    insert into employee values (61, 'usa');
    insert into employee values (65, 'usa');
    insert into employee values (71, 'usa');
    insert into employee values (72, 'canada');
    insert into employee values (83, 'usa');
    insert into employee values (85, 'usa');
    insert into employee values (94, 'usa');
    insert into employee values (105, 'usa');
    insert into employee values (107, 'usa');
    insert into employee values (109, 'usa');
    insert into employee values (110, 'japan');
    insert into employee values (113, 'usa');
    insert into employee values (114, 'usa');
    insert into employee values (118, 'japan');
    insert into employee values (121, 'italy');
    insert into employee values (127, 'usa');
    insert into employee values (134, 'france');
    insert into employee values (136, 'usa');
    insert into employee values (138, 'usa');
    insert into employee values (141, 'switzerland');
    insert into employee values (144, 'usa');
    insert into employee values (145, 'usa');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set count on;
    select 'country:', job_country, count(*)
    from employee
    group by 1, 2;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CONSTANT country:
    JOB_COUNTRY canada
    COUNT 1

    CONSTANT country:
    JOB_COUNTRY england
    COUNT 3
    
    CONSTANT country:
    JOB_COUNTRY france
    COUNT 1
    
    CONSTANT country:
    JOB_COUNTRY italy
    COUNT 1
    
    CONSTANT country:
    JOB_COUNTRY japan
    COUNT 2
    
    CONSTANT country:
    JOB_COUNTRY switzerland
    COUNT 1
    
    CONSTANT country:
    JOB_COUNTRY usa
    COUNT 33
    Records affected: 7
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

