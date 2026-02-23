#coding:utf-8

"""
ID:          issue-7924
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7924
TITLE:       ALTER TABLE ALTER COLUMN <textual_field> can not be changed properly in some cases
NOTES:
    [22.01.2024] pzotov
        Checked on 6.0.0.219 (after commit https://github.com/FirebirdSQL/firebird/commit/bcc53d43c8cd0b904d2963173c153056f9465a09)
        TODO: check ability to insert into fields some data specific to appropriate collation and proper order of selected characters.
    [06.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.914.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    alter character set utf8 set default collation unicode_ci;
    alter character set win1250 set default collation win_cz_ci_ai;
    alter character set win1252 set default collation win_ptbr;
    alter character set win1257 set default collation win1257_ee;
    commit;
     
    create table test(
        f_init_1252 varchar(10) character set win1252
       ,f_init_1257 varchar(10) character set win1257
       ,f_init_utf8 varchar(10) character set utf8
    );
    commit;

    alter table test
        alter column f_init_1252 to f_curr_1250
       ,alter column f_curr_1250 type varchar(10) character set win1250
       -----------------------------------------------------------
       ,alter column f_init_1257 to f_curr_1252
       ,alter column f_curr_1252 type varchar(10) character set win1252
       -----------------------------------------------------------
       ,alter column f_init_utf8 to f_curr_1257
       ,alter column f_curr_1257 type varchar(10) character set win1257
    ;
    commit;
     
    show table test; -------------------- [ 1 ]
"""

substitutions = [('[ \t]+', ' '), (r'Table(:)?\s+\S+.*', '')]
act = isql_act('db', test_script, substitutions = substitutions)


@pytest.mark.version('>=6.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'SYSTEM.'
    expected_stdout = f"""
       F_CURR_1250                     VARCHAR(10) CHARACTER SET {SQL_SCHEMA_PREFIX}WIN1250 COLLATE {SQL_SCHEMA_PREFIX}WIN_CZ_CI_AI Nullable
       F_CURR_1252                     VARCHAR(10) CHARACTER SET {SQL_SCHEMA_PREFIX}WIN1252 COLLATE {SQL_SCHEMA_PREFIX}WIN_PTBR Nullable
       F_CURR_1257                     VARCHAR(10) CHARACTER SET {SQL_SCHEMA_PREFIX}WIN1257 COLLATE {SQL_SCHEMA_PREFIX}WIN1257_EE Nullable
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
