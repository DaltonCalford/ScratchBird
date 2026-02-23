#coding:utf-8

"""
ID:          issue-1306
ISSUE:       1306
TITLE:       Server crash on violation of NOT NULL constraint
DESCRIPTION:
JIRA:        CORE-907
FBTEST:      bugs.core_0907
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
    recreate table crash (a1 integer,a2 integer,a3 integer, a4 integer) ;
    commit;
    insert into crash (a1, a2, a3, a4) values ( 1, 2, 3, 4);
    insert into crash (a1, a2, a3    ) values ( 2, 3, 4   );
    insert into crash (a1, a2,     a4) values ( 2, 3,    4);
    commit;
    alter table crash add a5 computed by (a2*a3*a4);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    -- Since 3.0 one may do like this (and NOT by updating RDB tables):
    -- ALTER TABLE <table name> ALTER <field name> [NOT] NULL
    -- ALTER DOMAIN <domain name> [NOT] NULL
    -- :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    -- 02.04.2015: since rev. 61204 (build 3.0.0.31767)syntax of altering nullability for
    -- domains and tables has been changed: mandatory SET | DROP clause now needed, i.e.:
    -- ALTER TABLE <table name> ALTER <field name> {DROP | SET} NOT NULL
    -- ALTER DOMAIN <domain name> {DROP | SET} NOT NULL
    -- :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

    -- This attempt will FAIL with message:
    -- -Cannot make field A5 of table CRASH NOT NULL because there are NULLs present
    alter table crash alter a5 SET not null;
    commit;

    update crash set a3=1 where a3 is null;
    update crash set a4=1 where a4 is null;
    commit;

    alter table crash alter a1 SET not null;
    alter table crash alter a2 SET not null;
    alter table crash alter a3 SET not null;
    alter table crash alter a4 SET not null;
    alter table crash alter a5 SET not null;
    commit;
    update crash set a1=null, a2=null, a3=null,a4=null rows 1;
    commit;
    show table crash;
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
    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field A5 of table CRASH NOT NULL because there are NULLs present
    Statement failed, SQLSTATE = 23000
    validation error for column CRASH.A1, value *** null ***

    A1 INTEGER Not Null
    A2 INTEGER Not Null
    A3 INTEGER Not Null
    A4 INTEGER Not Null
    A5 Computed by: (a2*a3*a4)
"""

expected_stderr = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

