#coding:utf-8

"""
ID:          issue-1271
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1271
TITLE:       Column involved in the constraint (e.g. PK) could NOT be dropped if constraint has user-defined name
DESCRIPTION:
JIRA:        CORE-878
FBTEST:      bugs.core_0878
NOTES:
    [23.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table pk1 (i1 integer not null, i2 integer);
    alter table pk1 add primary key (i1);
    commit;
    show table pk1;
    alter table pk1 drop i1;
    commit;

    create table pk2 (i1 integer not null, i2 integer);
    alter table pk2 add constraint pk2_pk primary key (i1);
    commit;
    show table pk2;
    alter table pk2 drop i1;
    commit;

    create table pk3 (i1 integer not null primary key, i2 integer);
    commit;
    show table pk3;
    alter table pk3 drop i1;
    commit;

    show table pk1;

    show table pk2;

    show table pk3;
"""


# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' '), ('Table: .*', '')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)


expected_stdout = """
    I1                              INTEGER Not Null
    I2                              INTEGER Nullable
    CONSTRAINT INTEG_2:
      Primary key (I1)
    
    I1                              INTEGER Not Null
    I2                              INTEGER Nullable
    CONSTRAINT PK2_PK:
      Primary key (I1)
    
    I1                              INTEGER Not Null
    I2                              INTEGER Nullable
    CONSTRAINT INTEG_5:
      Primary key (I1)
    
    I2                              INTEGER Nullable

    I2                              INTEGER Nullable

    I2                              INTEGER Nullable
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

