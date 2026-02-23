#coding:utf-8

"""
ID:          issue-8710
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8710
TITLE:       Filters do not have schemas
NOTES:
    [21.08.2025] pzotov
    Confirmed problem on 6.0.0.949.
    Checked on 6.0.0.1232-f69e844
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    declare filter aboba input_type 1 output_type -4 entry_point 'desc_filter' module_name 'filterlib';
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
