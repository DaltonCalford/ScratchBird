#coding:utf-8

"""
ID:          eb703ba7b5
ISSUE:       https://www.sqlite.org/src/tktview/eb703ba7b5
TITLE:       Incorrect result using index on expression with collating function
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;
    create collation name_coll for utf8 from unicode case insensitive;
    create domain dm_test varchar(50) character set utf8 collate name_coll;

    create table t1(a integer primary key, b dm_test);
    insert into t1 values(1,'coffee');
    insert into t1 values(2,'COFFEE');
    insert into t1 values(3,'stress');
    insert into t1 values(4,'STRESS');

    set count on;
    select '1:' as msg, a from t1 where substring(b from 4)='ess' collate name_coll;
    commit;

    create index t1b on t1 computed by( substring(b from 4) );
    set plan on;
    select '2:' as msg, a from t1 where substring(b from 4) = 'ess' collate name_coll;
"""

substitutions = [('[ \t]+', ' ')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG 1:
    A 3
    MSG 1:
    A 4
    Records affected: 2

    PLAN (T1 INDEX (T1B))
    MSG 2:
    A 3
    MSG 2:
    A 4
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
