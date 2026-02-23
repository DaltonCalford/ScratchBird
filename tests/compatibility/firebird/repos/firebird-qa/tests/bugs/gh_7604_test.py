#coding:utf-8

"""
ID:          issue-7604
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7604
TITLE:       PSQL functions do not convert the output BLOB to the connection character set
NOTES:
    [03.06.2023] pzotov
        Confirmed problem on 4.0.3.2943, 5.0.0.1060.
        Checked on 4.0.3.2947, 5.0.0.1063 -- all fine.
    [14.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
    [04.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214.
"""
import locale
import pytest
from firebird.qa import *

init_sql = """
    set term ^;
    create or alter function sp_test_func returns blob sub_type text character set utf8 as
        declare runtotal blob sub_type text;
    begin
        runtotal = 'мама мыла раму';
        return runtotal;
    end
    ^
    set term ;^
    commit;
"""
db = db_factory(charset = 'utf8', init = init_sql)

act = python_act('db', substitutions = [ ('^((?!SQLSTATE|sqltype:).)*$',''),('[ \t]+',' '),('.*alias:.*','') ] )

@pytest.mark.version('>=4.0.3')
def test_1(act: Action):

    test_sql = """
        set list on;
        set sqlda_display on;
        select sp_test_func() as runtotal from rdb$database rows 0;
    """

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'SYSTEM.'
    expected_stdout = f"""
        01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 52 {SQL_SCHEMA_PREFIX}WIN1251
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], charset = 'win1251', input = test_sql, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
