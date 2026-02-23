#coding:utf-8

"""
ID:          issue-5049
ISSUE:       5049
TITLE:       ALTER DATABASE SET DEFAULT CHARACTER SET: 1) take effect only for once for current attachment; 2) does not check that new char set exists untill it will be used
DESCRIPTION:
JIRA:        CORE-4744
FBTEST:      bugs.core_4744
NOTES:
    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    set list on;

    select rdb$character_set_name as db_char_set from rdb$database;

    commit;

    create or alter view v_table_field_cset as
    select
       rf.rdb$relation_name tab_name
      ,rf.rdb$field_name fld_name
      ,cs.rdb$character_set_name fld_cset
      ,rb.RDB$CHARACTER_SET_NAME db_default_cset
    from rdb$relation_fields rf
    join rdb$fields ff on rf.rdb$field_source = ff.rdb$field_name
    left join rdb$character_sets cs on ff.rdb$character_set_id = cs.rdb$character_set_id
    cross join rdb$database rb
    ;
    commit;

    -------- CHANGE DATABASE DEFAULT CHARSET, COMMAND # 1 --------

    alter database set default character set dos850;
    commit;

    create table tab_850( txt_850 varchar(10)); commit;
    select * from v_table_field_cset where tab_name=upper('tab_850');

    -------- CHANGE DATABASE DEFAULT CHARSET, COMMAND # 2 --------

    alter database set default character set dos866;
    commit;

    create table tab_866( txt_866 varchar(10)); commit;
    select * from v_table_field_cset where tab_name=upper('tab_866');

    -------- CHANGE DATABASE DEFAULT CHARSET, COMMAND # 3 --------

    alter database set default character set win1252;
    commit;

    create table tab_1252( txt_1252 varchar(10)); commit;
    select * from v_table_field_cset where tab_name=upper('tab_1252');

    -------- CHANGE DATABASE DEFAULT CHARSET TO NON-EXISTED VALUE --------

    alter database set default character set FOO_BAR_8859_4;
    commit;

    create table tab_foo( txt_8859_4 varchar(10) );
    commit;
    create table tab_bar( txt_8859_4 varchar(10) character set FOO_BAR_8859_4);
    commit;

    select * from v_table_field_cset where tab_name=upper('tab_foo');
    select * from v_table_field_cset where tab_name=upper('tab_bar');
"""

act = isql_act('db', test_script)


expected_stdout_5x = """
    DB_CHAR_SET                     UTF8

    TAB_NAME                        TAB_850
    FLD_NAME                        TXT_850
    FLD_CSET                        DOS850
    DB_DEFAULT_CSET                 DOS850

    TAB_NAME                        TAB_866
    FLD_NAME                        TXT_866
    FLD_CSET                        DOS866
    DB_DEFAULT_CSET                 DOS866

    TAB_NAME                        TAB_1252
    FLD_NAME                        TXT_1252
    FLD_CSET                        WIN1252
    DB_DEFAULT_CSET                 WIN1252

    Statement failed, SQLSTATE = 2C000
    unsuccessful metadata update
    -ALTER DATABASE failed
    -CHARACTER SET FOO_BAR_8859_4 is not defined
    
    Statement failed, SQLSTATE = HY004
    unsuccessful metadata update
    -CREATE TABLE TAB_BAR failed
    -Dynamic SQL Error
    -SQL error code = -204
    -Data type unknown
    -CHARACTER SET FOO_BAR_8859_4 is not defined

    TAB_NAME                        TAB_FOO
    FLD_NAME                        TXT_8859_4
    FLD_CSET                        WIN1252
    DB_DEFAULT_CSET                 WIN1252
"""

expected_stdout_6x = """
    DB_CHAR_SET                     UTF8

    TAB_NAME                        TAB_850
    FLD_NAME                        TXT_850
    FLD_CSET                        DOS850
    DB_DEFAULT_CSET                 DOS850
    
    TAB_NAME                        TAB_866
    FLD_NAME                        TXT_866
    FLD_CSET                        DOS866
    DB_DEFAULT_CSET                 DOS866
    
    TAB_NAME                        TAB_1252
    FLD_NAME                        TXT_1252
    FLD_CSET                        WIN1252
    DB_DEFAULT_CSET                 WIN1252
    
    Statement failed, SQLSTATE = 2C000
    unsuccessful metadata update
    -ALTER DATABASE failed
    -CHARACTER SET "PUBLIC"."FOO_BAR_8859_4" is not defined
    Statement failed, SQLSTATE = HY004
    unsuccessful metadata update
    -CREATE TABLE "PUBLIC"."TAB_BAR" failed
    -Dynamic SQL Error
    -SQL error code = -204
    -Data type unknown
    -CHARACTER SET "PUBLIC"."FOO_BAR_8859_4" is not defined
    
    TAB_NAME                        TAB_FOO
    FLD_NAME                        TXT_8859_4
    FLD_CSET                        WIN1252
    DB_DEFAULT_CSET                 WIN1252
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
