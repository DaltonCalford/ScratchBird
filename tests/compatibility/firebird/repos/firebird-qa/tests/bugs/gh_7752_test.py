#coding:utf-8

"""
ID:          issue-7752
ISSUE:       7752
TITLE:       The access path information is truncated to 255 characters in the PLG$PROF_RECORD_SOURCES table
NOTES:
    [02.10.2023] pzotov
    Confirmed problem (truncating of profiler data) on 5.0.0.1219, date of build: 17-sep-2023.
    Checked on 5.0.0.1235, 6.0.0.65 -- all fine.
    [14.07.2025] pzotov
    Re-implemented: use non-ascii names for table and its alias.
    Using non-ascii SCHEMA name with maximal allowed length (56 characters) - it is needed on FB 6.x.
    Adjusted for FB 6.x: it is MANDATORY to specify schema `PLG$PROFILER.` when querying created profiler tables.
    See doc/sql.extensions/README.schemas.md, section title: '### gbak'; see 'SQL_SCHEMA_PREFIX' variable here.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.970; 5.0.3.1668.
"""

import os
import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

act = python_act('db')


#---------------------------------------------------------

def replace_leading(source, char="#"):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#---------------------------------------------------------

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    PLG_SCHEMA_PREFIX = '' if act.is_version('<6') else 'PLG$PROFILER.'

    CUSTOM_SCHEMA = '' if act.is_version('<6') else '"БьТЦууКенгШщзХъЭждЛорПавЫфЯчсмиТьбЮЪхЗщШШГнЕкУцЙФывААпрО"'
    CREATE_SCHEMA_SQL = '' if act.is_version('<6') else f'create schema {CUSTOM_SCHEMA};'
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else f'{CUSTOM_SCHEMA}.'
    TEST_TABLE_NAME = '"БьТЦууКенгШщзХъЭждЛорПавЫфЯчсмиТьбЮЪхЗщШШГнЕкУцЙФывААпрО"'
    TEST_ALIAS_NAME = '"ЦууКенгШщзХъЭждЛорПавЫфЯчсмиТьбЮЪхЗщШШГнЕкУцЙФывААпрОБьТ"'

    init_sql = f"""
        {CREATE_SCHEMA_SQL}
        create table {SQL_SCHEMA_PREFIX}{TEST_TABLE_NAME}(id int);
    """

    profiler_sql = f"""
        select p.access_path
        from  {PLG_SCHEMA_PREFIX}plg$prof_record_sources p
        order by octet_length(p.access_path) desc
        rows 1
    """

    with act.db.connect(charset = 'utf8') as con:

        cur = con.cursor()
        for x in init_sql.splitlines():
            if (s := x.strip()):
                cur.execute(s)
                con.commit()

        cur.execute("select rdb$profiler.start_session('profile session 1') from rdb$database")
        for r in cur:
            pass

        cur.execute( f'select count(*) from {SQL_SCHEMA_PREFIX}{TEST_TABLE_NAME} as {TEST_ALIAS_NAME}')
        for r in cur:
            pass

        cur.callproc('rdb$profiler.finish_session', (True,))
        con.commit()

        cur.execute(profiler_sql)
        for r in cur:
            print( '\n'.join([replace_leading(s, char='.') for s in r[0].split('\n')])  )

    expected_stdout_5x = f"""
        -> Table {TEST_TABLE_NAME} as {TEST_ALIAS_NAME} Full Scan
    """

    expected_stdout_6x = f"""
        -> Table {CUSTOM_SCHEMA}.{TEST_TABLE_NAME} as {TEST_ALIAS_NAME} Full Scan
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
