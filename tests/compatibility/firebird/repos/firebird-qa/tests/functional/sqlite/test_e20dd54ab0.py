#coding:utf-8

"""
ID:          e20dd54ab0
ISSUE:       https://www.sqlite.org/src/tktview/e20dd54ab0
TITLE:       COLLATE sequence for ORDER/GROUP BY ignored when using an index-on-expression
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    ::: NB ::: Execution on 6.x may keep 'PLAN NATURAL' if number of records is small.
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set list on;
    create collation name_coll for utf8 from unicode case insensitive;

    create table test(x varchar(36));
    insert into test select iif(rand() < 0.5, lower(x), x) from (select uuid_to_char(gen_uuid()) x from rdb$types, (select 1 i from rdb$types rows 10));
    commit;
    create index test_x on test computed by( substring(x from 25 for 12) collate name_coll );
    set planonly;
    select 1 from test order by substring(x from 25 for 12) collate name_coll;
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
    PLAN (TEST ORDER TEST_X)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
