#coding:utf-8

"""
ID:          issue-1406
ISSUE:       1406
TITLE:       select with FIRST and LEFT JOIN needs excess SORT in plan
DESCRIPTION:
JIRA:        CORE-995
FBTEST:      bugs.core_0995
NOTES:
    [23.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table orgaccount (id numeric(15,0));
    commit;
    recreate table org(
      id numeric(15,0) primary key using index pk_org,
      name varchar(1000)
    );

    recreate table orgaccount (
      id numeric(15,0) primary key using index pk_orgaccount,
      name varchar(1000),
      org_id numeric (15,0) not null references org using index fk_orgaccount
    );

    insert into org values (1, 'org1');
    insert into org values (2, 'org2');
    insert into org values (3, 'org3');
    insert into org values (4, 'org4');
    insert into org values (5, 'org5');
    insert into org values (6, 'org6');

    insert into orgaccount values (1, 'account1', 1);
    insert into orgaccount values (2, 'account2', 1);
    insert into orgaccount values (3, 'account3', 1);
    insert into orgaccount values (4, 'account4', 1);
    insert into orgaccount values (5, 'account5', 1);
    insert into orgaccount values (6, 'account6', 2);
    insert into orgaccount values (7, 'account7', 2);
    insert into orgaccount values (8, 'account8', 3);

    commit;
    set statistics index pk_org;
    set statistics index pk_orgaccount;
    set statistics index fk_orgaccount;
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set planonly;

    -- testing plan for OUTER join:
    select first 10 *
    from orgaccount
    LEFT join org on org.id=orgaccount.org_id
    where orgaccount.id>4
    order by orgaccount.id;

    -- testing plan for INNER join:
    select first 10 *
    from orgaccount
    INNER join org on org.id=orgaccount.org_id
    where orgaccount.id>4
    order by orgaccount.id;
"""


# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' ')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    PLAN JOIN (ORGACCOUNT ORDER PK_ORGACCOUNT, ORG INDEX (PK_ORG))
    PLAN JOIN (ORGACCOUNT ORDER PK_ORGACCOUNT, ORG INDEX (PK_ORG))
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

