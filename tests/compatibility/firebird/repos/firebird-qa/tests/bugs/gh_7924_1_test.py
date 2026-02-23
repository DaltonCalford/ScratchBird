#coding:utf-8

"""
ID:          issue-7924
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7924
TITLE:       ALTER TABLE ALTER COLUMN <textual_field> can not be changed properly in some cases
NOTES:
    [22.01.2024] pzotov
        Checked on 6.0.0.219 (after commit https://github.com/FirebirdSQL/firebird/commit/bcc53d43c8cd0b904d2963173c153056f9465a09)
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
        f01 varchar(10) character set win1252
       ,f02 varchar(10) character set win1257
       ,f03 varchar(10) character set win1251
    );
    commit;

     
    alter table test
        alter column f01 type varchar(10) character set win1250
       -----------------------------------------------------------
       ,alter column f02 type varchar(10) character set win1252
       -----------------------------------------------------------
       ,alter column f03 type varchar(10) character set win1257
    ;
    commit;

	-- 1. Check that SHOW TABLE will display proper character sets:
    show table test;

	-- 2. All three statements below raised "Cannot transliterate" before fix:
	--     Statement failed, SQLSTATE = 22018
	-- 	   arithmetic exception, numeric overflow, or string truncation
	--     -Cannot transliterate character between character sets	
	-- Now all of them must PASS:
	insert into test(f01) values ('Ł');
	insert into test(f02) values ('Ð');
	insert into test(f03) values ('Ģ');
"""

substitutions = [('[ \t]+', ' '), (r'Table(:)?\s+\S+.*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'SYSTEM.'
    expected_stdout = f"""
        F01                             VARCHAR(10) CHARACTER SET {SQL_SCHEMA_PREFIX}WIN1250 COLLATE {SQL_SCHEMA_PREFIX}WIN_CZ_CI_AI Nullable
        F02                             VARCHAR(10) CHARACTER SET {SQL_SCHEMA_PREFIX}WIN1252 COLLATE {SQL_SCHEMA_PREFIX}WIN_PTBR Nullable
        F03                             VARCHAR(10) CHARACTER SET {SQL_SCHEMA_PREFIX}WIN1257 COLLATE {SQL_SCHEMA_PREFIX}WIN1257_EE Nullable
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
