#coding:utf-8

"""
ID:          199df4168c
ISSUE:       https://www.sqlite.org/src/tktview/199df4168c
TITLE:       Different answer with and without index on IN operator with type mismatch
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create table t_chr1(c varchar(1));
    create table t_chr2(c varchar(1));
    create table t_blob(b blob sub_type text);

    insert into t_blob select ascii_char( 31+row_number()over() ) from rdb$types rows 126-31;
    insert into t_chr1 select * from t_blob;
    insert into t_chr2 select * from t_blob;
    commit;

    create unique index chr1_idx on t_chr1(c);
    create unique index chr2_idx on t_chr2(c);
    commit;
    
    set plan on;
    select count(*) as cnt_1 from t_chr1 where c in (select c from t_chr2);
    select count(*) as cnt_2 from t_blob where cast(b as varchar(1)) in (select c from t_chr2);
    quit;
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


@pytest.mark.version('>=3')
def test_1(act: Action):

    expected_stdout_5x = """
        PLAN (T_CHR2 INDEX (CHR2_IDX))
        PLAN (T_CHR1 NATURAL)
        CNT_1 95
        
        PLAN (T_CHR2 INDEX (CHR2_IDX))
        PLAN (T_BLOB NATURAL)
        CNT_2 95
    """

    expected_stdout_6x = """
        PLAN HASH (T_CHR1 NATURAL, T_CHR2 NATURAL)
        CNT_1 95
        PLAN HASH (T_BLOB NATURAL, T_CHR2 NATURAL)
        CNT_2 95
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
