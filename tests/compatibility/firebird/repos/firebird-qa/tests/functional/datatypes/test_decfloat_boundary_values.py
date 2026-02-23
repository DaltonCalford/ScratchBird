#coding:utf-8

"""
ID:          decfloat.boundary-values
ISSUE:       5803
JIRA:        CORE-5535
TITLE:       Check BOUNDARY values that are defined for DECFLOAT datatype
DESCRIPTION:
  See  doc/sql.extensions/README.data_types
FBTEST:      functional.datatypes.decfloat_boundary_values
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    with
    c as (
        select
             cast(-9.999999999999999E384 as decfloat(16)) as min_df16_for_neg_scope
            ,cast(-1.0E-383 as decfloat(16)) as max_df16_for_neg_scope
            ,cast(1.0E-383 as decfloat(16)) as min_df16_for_pos_scope
            ,cast( 9.999999999999999E384 as decfloat(16)) as max_df16_for_pos_scope
            ,cast(-9.999999999999999999999999999999999E6144 as decfloat(34)) as min_df34_for_neg_scope
            ,cast(-1.0E-6143 as decfloat(34)) as max_df34_for_neg_scope
            ,cast(1.0E-6143 as decfloat(34)) as min_df34_for_pos_scope
            ,cast( 9.999999999999999999999999999999999E6144 as decfloat(34)) as max_df34_for_pos_scope
        from rdb$database
    )
    select
             c.min_df16_for_neg_scope
            ,c.max_df16_for_neg_scope
            ,c.min_df16_for_pos_scope
            ,c.max_df16_for_pos_scope
            ,c.min_df34_for_neg_scope
            ,c.max_df34_for_neg_scope
            ,c.min_df34_for_pos_scope
            ,c.max_df34_for_pos_scope
            ,c.min_df16_for_neg_scope - c.min_df16_for_neg_scope as zero_min_df16_for_neg
            ,c.max_df16_for_neg_scope - c.max_df16_for_neg_scope as zero_max_df16_for_neg
            ,c.min_df16_for_pos_scope - c.min_df16_for_pos_scope as zero_min_df16_for_pos
            ,c.max_df16_for_pos_scope - c.max_df16_for_pos_scope as zero_max_df16_for_pos
            ,c.min_df34_for_neg_scope - c.min_df34_for_neg_scope as zero_min_df34_for_neg
            ,c.max_df34_for_neg_scope - c.max_df34_for_neg_scope as zero_max_df34_for_neg
            ,c.min_df34_for_pos_scope - c.min_df34_for_pos_scope as zero_min_df34_for_pos
            ,c.max_df34_for_pos_scope - c.max_df34_for_pos_scope as zero_max_df34_for_pos
    from c
    ;
"""

act = isql_act('db', test_script, substitutions=[('[\\s]+', ' ')])

expected_stdout = """
    MIN_DF16_FOR_NEG_SCOPE          -9.999999999999999E+384
    MAX_DF16_FOR_NEG_SCOPE                        -1.0E-383
    MIN_DF16_FOR_POS_SCOPE                         1.0E-383
    MAX_DF16_FOR_POS_SCOPE           9.999999999999999E+384
    MIN_DF34_FOR_NEG_SCOPE          -9.999999999999999999999999999999999E+6144
    MAX_DF34_FOR_NEG_SCOPE                                          -1.0E-6143
    MIN_DF34_FOR_POS_SCOPE                                           1.0E-6143
    MAX_DF34_FOR_POS_SCOPE           9.999999999999999999999999999999999E+6144
    ZERO_MIN_DF16_FOR_NEG                            0E+369
    ZERO_MAX_DF16_FOR_NEG                            0E-384
    ZERO_MIN_DF16_FOR_POS                            0E-384
    ZERO_MAX_DF16_FOR_POS                            0E+369
    ZERO_MIN_DF34_FOR_NEG                                              0E+6111
    ZERO_MAX_DF34_FOR_NEG                                              0E-6144
    ZERO_MIN_DF34_FOR_POS                                              0E-6144
    ZERO_MAX_DF34_FOR_POS                                              0E+6111
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
