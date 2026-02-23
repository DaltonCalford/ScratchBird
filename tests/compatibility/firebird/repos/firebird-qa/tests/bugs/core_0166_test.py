#coding:utf-8

"""
ID:          issue-495
ISSUE:       495
TITLE:       Cannot specify PLAN in UPDATE statement
DESCRIPTION:
JIRA:        CORE-166
FBTEST:      bugs.core_0166
NOTES:
    [22.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table company(id int not null primary key, contact_id int, company_name varchar(60));
    recreate table contact(id int not null  primary key using index contact_id, contact_name varchar(60));
    alter table company add constraint company_fk foreign key(contact_id) references contact(id);
    commit;

    insert into contact values(1, '+784956253581, Vovan');
    insert into contact values(2, '+375172223217, Shurik');
    insert into contact values(3, '+380442057337, Vitalik');

    insert into company values(100, 1, 'Pepsico, Inc.');
    insert into company values(101, 1, '');
    insert into company values(102, 2, 'Balaha, Inc.');
    insert into company values(103, 2, '');
    insert into company values(104, 2, null);
    insert into company values(105, 3, null);
    insert into company values(106, 3, 'Firebird Foundation');
    commit;

    set list on;

    select c.*
    from company c order by c.id;

    set plan on;
    set count on;

    update company c set c.company_name =
    ( select k.contact_name
      from contact k
      where k.id = c.contact_id
      PLAN (K INDEX (CONTACT_ID))
    )
    where c.company_name is null or c.company_name = ''
    PLAN (C NATURAL)
    ;

    set plan off;
    set count off;

    select c.*
    from company c order by c.id;
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [ ('line: \\d+, col: \\d+', '') ]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ID                              100
    CONTACT_ID                      1
    COMPANY_NAME                    Pepsico, Inc.

    ID                              101
    CONTACT_ID                      1
    COMPANY_NAME

    ID                              102
    CONTACT_ID                      2
    COMPANY_NAME                    Balaha, Inc.

    ID                              103
    CONTACT_ID                      2
    COMPANY_NAME

    ID                              104
    CONTACT_ID                      2
    COMPANY_NAME                    <null>

    ID                              105
    CONTACT_ID                      3
    COMPANY_NAME                    <null>

    ID                              106
    CONTACT_ID                      3
    COMPANY_NAME                    Firebird Foundation

    PLAN (K INDEX (CONTACT_ID))
    PLAN (C NATURAL)
    Records affected: 4

    ID                              100
    CONTACT_ID                      1
    COMPANY_NAME                    Pepsico, Inc.

    ID                              101
    CONTACT_ID                      1
    COMPANY_NAME                    +784956253581, Vovan

    ID                              102
    CONTACT_ID                      2
    COMPANY_NAME                    Balaha, Inc.

    ID                              103
    CONTACT_ID                      2
    COMPANY_NAME                    +375172223217, Shurik

    ID                              104
    CONTACT_ID                      2
    COMPANY_NAME                    +375172223217, Shurik

    ID                              105
    CONTACT_ID                      3
    COMPANY_NAME                    +380442057337, Vitalik

    ID                              106
    CONTACT_ID                      3
    COMPANY_NAME                    Firebird Foundation
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

