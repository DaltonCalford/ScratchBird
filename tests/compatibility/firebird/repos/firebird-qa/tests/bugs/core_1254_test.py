#coding:utf-8

"""
ID:          issue-1678
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1678
TITLE:       Problem with DISTINCT and insensitive collations
DESCRIPTION: See https://github.com/FirebirdSQL/firebird/issues/2965
JIRA:        CORE-1254
FBTEST:      bugs.core_1254
NOTES:
    1. Confirmed problem on 2.1.3.18185
       Both queries: 'select ... group by ...' and 'select distinct ...' issued six rows:
        GROUP_ID     QUESTION 
        ======== ============ 
        a                   1 
        a                   2 
        a                   3 
        A                   1 
        A                   2 
        A                   3 
        (instead of expected three rows with 'a' or 'A' in the 1st column).
        The only correct result issued when index was used.

     2. Values in 1st column can vary if OptimizeForFirstRows = true (FB 5.x+).
        Because of this, we have to check only COUNT of letters in this column
        that are unique being compared using case SENSITIVE collation.
        In all cases (for queries and with/without index) this count must be 1.
"""

import pytest
from firebird.qa import *

init_script = """
    create table test(
        group_id varchar(1) character set utf8 collate unicode_ci,
        question integer,
        score integer
    );
    commit;
    insert into test (group_id,question,score) values ('a',1,11);
    insert into test (group_id,question,score) values ('a',3,13);
    insert into test (group_id,question,score) values ('A',1,14);
    insert into test (group_id,question,score) values ('a',2,12);
    insert into test (group_id,question,score) values ('A',2,15);
    insert into test (group_id,question,score) values ('A',3,16);
    commit;
    -- See https://github.com/FirebirdSQL/firebird/issues/2965#issue-866882047
    -- GROUP BY will use an index on multi-byte or insensitive collation only
    -- when this index is: 1) UNIQUE and 2) ASCENDING.
    create UNIQUE index test_gr_que_score on test(group_id, question, score);
    commit;
"""


db = db_factory(charset='UTF8', init=init_script)

test_script = """
    --set explain on;
    --set plan on;
    set list on;
    alter index test_gr_que_score inactive;
    commit;
    
    select count(
        distinct cast( group_id as varchar(1)
                       -- Check count of unique values in 1st column using
                       -- case SENSITIVE collation:
                       -- #########################
                       character set ascii
                     )
                ) as "case_SENSITIVE_distinct_gr_1"
    from (
        select group_id, question from test group by 1,2
    );

    select count( distinct cast(group_id as varchar(1) character set ascii)) as "case_SENSITIVE_distinct_gr_2"
    from (
        select distinct group_id, question from test
    );

    alter index test_gr_que_score active;
    commit;

    select count( distinct cast(group_id as varchar(1) character set ascii)) as "case_SENSITIVE_distinct_gr_3"
    from (
        select group_id, question from test group by 1,2
    );

    select count( distinct cast(group_id as varchar(1) character set ascii)) as "case_SENSITIVE_distinct_gr_4"
    from (
        select distinct group_id, question from test
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
    case_SENSITIVE_distinct_gr_1    1
    case_SENSITIVE_distinct_gr_2    1
    case_SENSITIVE_distinct_gr_3    1
    case_SENSITIVE_distinct_gr_4    1
"""

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

