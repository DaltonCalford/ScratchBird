#coding:utf-8

"""
ID:          issue-8722
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8722
TITLE:       The "IF NOT EXISTS" clause is missing for DECLARE FILTER
NOTES:
    [01.09.2025] pzotov
    Checked on 6.0.0.1261-8d5bb71.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    declare filter if not exists aboba input_type 1 output_type -4 entry_point 'desc_filter' module_name 'filterlib';
    comment on filter aboba is 'comment1';
    commit;
    set list on;
    set count on;
    select
        rdb$function_name as filter_name
       ,rdb$description as descr_blob_id
       ,rdb$module_name as module_name
       ,rdb$entrypoint as entry_point
       ,rdb$input_sub_type as input_subtype
       ,rdb$output_sub_type as output_subtype
       ,rdb$system_flag as system_flag
    from rdb$filters;
"""
substitutions = [('[ \t]+', ' '), ('DESCR_BLOB_ID .*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        FILTER_NAME ABOBA
        comment1
        MODULE_NAME filterlib
        ENTRY_POINT desc_filter
        INPUT_SUBTYPE 1
        OUTPUT_SUBTYPE -4
        SYSTEM_FLAG 0
        Records affected: 1
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
