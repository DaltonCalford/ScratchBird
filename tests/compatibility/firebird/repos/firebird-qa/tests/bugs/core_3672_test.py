#coding:utf-8

"""
ID:          issue-4022
ISSUE:       4022
TITLE:       computed index by substring function for long columns
DESCRIPTION:
JIRA:        CORE-3672
FBTEST:      bugs.core_3672
NOTES:
    [27.06.2025] pzotov
    Added 'SCHEMA_PREFIX' to be substituted in expected_out on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

IDX_EXPR_1 = 'substring(col1 from 1 for 169)'
IDX_EXPR_2 = 'substring(trim( col2 from col1 ) from 1 for 169) '
IDX_EXPR_3 = 'col2'
IDX_EXPR_4 = 'substring(col2 from 1 for 169)'
IDX_EXPR_5 = 'col3'
IDX_EXPR_6 = 'substring(col3 from 1 for 169)'

test_script = f"""
    recreate table test(
         col1 varchar(8190) character set utf8 collate unicode_ci_ai
        ,col2 computed by ( substring(col1 from 1 for 169) )
        ,col3 computed by ( substring(trim( col2 from col1 ) from 1 for 169) )
    );
    commit;

    -- Index on UTF8-field which should be collate in CI_AI manner, will contain 6 bytes per one character (hereafter this is 'N').
    -- This mean that maximum length of key for default page_size = 4096 is:
    -- max_key_length = floor( (page_size / 4 - 9) / N ) = 169 characters.

    -- Verify that we CAN do that w/o error:
    create index test_col1_idx_a on test computed by ( {IDX_EXPR_1} );
    create index test_col1_idx_b on test computed by ( {IDX_EXPR_2} );
    create index test_col2_idx_a on test computed by ( {IDX_EXPR_3} );
    create index test_col2_idx_b on test computed by ( {IDX_EXPR_4} );
    create index test_col3_idx_a on test computed by ( {IDX_EXPR_5} );
    create index test_col3_idx_b on test computed by ( {IDX_EXPR_6} );
    commit;
    -- Confirmed for 2.5.5: "-key size exceeds implementation restriction"

    show index test_col1_idx_a;
    show index test_col1_idx_b;
    show index test_col2_idx_a;
    show index test_col2_idx_b;
    show index test_col3_idx_a;
    show index test_col3_idx_b;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):

    SCHEMA_PREFIX = '' if act.is_version('<6') else 'PUBLIC.'

    expected_stdout = f"""
        {SCHEMA_PREFIX}TEST_COL1_IDX_A INDEX ON TEST COMPUTED BY ( {IDX_EXPR_1} )
        {SCHEMA_PREFIX}TEST_COL1_IDX_B INDEX ON TEST COMPUTED BY ( {IDX_EXPR_2} )
        {SCHEMA_PREFIX}TEST_COL2_IDX_A INDEX ON TEST COMPUTED BY ( {IDX_EXPR_3} )
        {SCHEMA_PREFIX}TEST_COL2_IDX_B INDEX ON TEST COMPUTED BY ( {IDX_EXPR_4} )
        {SCHEMA_PREFIX}TEST_COL3_IDX_A INDEX ON TEST COMPUTED BY ( {IDX_EXPR_5} )
        {SCHEMA_PREFIX}TEST_COL3_IDX_B INDEX ON TEST COMPUTED BY ( {IDX_EXPR_6} )
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
