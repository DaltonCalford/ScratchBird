#coding:utf-8

"""
ID:          computed-fields-12
FBTEST:      functional.gtcs.computed_fields_12
TITLE:       Computed fields
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_12.script
    SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
NOTES:
    [25.09.2021]
        splitted output for 3.x and 4.x+ because of fixed gh-6845. Use SET_SQLDA_DISPLAY ON for check datatypes.
        Seel commit for apropriate GTCS-tests: e617f3d70be5018de6e6ee8624da6358d52a9ce0, 20-aug-2021 14:11
    [16.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test (fld_source integer, fld_comp_based_on_source computed by ( fld_source*3 ), fld_comp_based_on_comp computed by ( fld_comp_based_on_source * 2 ) );
    insert into test(fld_source) values(10);

    set sqlda_display on;

    -- expected output for 3rd column:
    -- 03: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16" (confirm on build 4.0.1.2613; 5.0.0.220)
    -- build 4.0.1.2536 (last before fix) issues here "03: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8"

    select * from test;
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype|FLD_)).)*$', ''),
                                                     ('[ \t]+', ' '), ('.*alias.*', '')])

expected_fb3x = """
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    02: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    03: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    FLD_SOURCE               10
    FLD_COMP_BASED_ON_SOURCE 30
    FLD_COMP_BASED_ON_COMP   60
"""

expected_fb4x = """
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    02: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    03: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    FLD_SOURCE               10
    FLD_COMP_BASED_ON_SOURCE 30
    FLD_COMP_BASED_ON_COMP   60
"""

@pytest.mark.version('>=3.0')
def test(act: Action):
    act.expected_stdout = expected_fb3x if act.is_version('<4') else expected_fb4x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
