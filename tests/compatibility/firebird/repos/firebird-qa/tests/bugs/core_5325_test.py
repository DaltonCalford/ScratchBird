#coding:utf-8

"""
ID:          issue-5601
ISSUE:       5601
TITLE:       Malformed string error when using cyrilic symbols and x'0d0a' in exception
DESCRIPTION:
JIRA:        CORE-5325
FBTEST:      bugs.core_5325
    [01.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
    Separated expected output for FB major versions prior/since 6.x.
    
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from pathlib import Path
from firebird.qa import *

substitutions = [('exception [\\d]+', 'exception'),
                 ('-At block line(:)?\\s+[\\d]+.*', ''),
                 ('After line(:)?\\s+[\\d]+.*', '')]

db = db_factory()

act = python_act('db', substitutions=substitutions)

test_script = """
    create exception exc_qwerty 'йцукенг';
    commit;
    set term ^;
    execute block as
    begin
        exception exc_qwerty 'йцу' || _win1251 x'0d0a' || 'кенг';
    end^
    set term ;^
"""

script_file = temp_file('test-script.sql')

@pytest.mark.version('>=3.0')
def test_1(act: Action, script_file: Path):
    script_file.write_text(test_script, encoding='cp1251')

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    EXCEPTION_NAME = ('exc_qwerty' if act.is_version('<6') else '"exc_qwerty"').upper()
    expected_out = f"""
        Statement failed, SQLSTATE = HY000
        exception 1
        -{SQL_SCHEMA_PREFIX}{EXCEPTION_NAME}
        -йцу

        кенг
    """
    act.expected_stdout = expected_out
    act.isql(switches=['-q'], input_file=script_file, charset='WIN1251', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout


