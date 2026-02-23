#coding:utf-8

"""
ID:          issue-1796
ISSUE:       1796
TITLE:       Domain names and charset issues
DESCRIPTION:
JIRA:        CORE-1378
FBTEST:      bugs.core_1378
"""

import pytest
from firebird.qa import *
from pathlib import Path

db = db_factory(charset='WIN1251')

act = python_act('db')

tmp_file = temp_file('non_ascii_ddl.sql')

sql_txt = '''
set bail on;

create collation "вид прописи" for win1251 from pxw_cyrl pad space case insensitive accent insensitive;
commit;
create domain "значение числа" as int;
create domain "число прописью" as varchar(8191) character set win1251 collate "вид прописи";
commit;
create table test( id "значение числа", txt "число прописью");
commit;
set term ^;
create procedure sp_test ( i_number "значение числа") returns( o_text "число прописью") as
begin
   suspend;
end
^
set term ;^
commit;

--set blob all;
--select rdb$procedure_source as rdb_source_blob_id from rdb$procedures where rdb$procedure_name = upper('sp_test');
set list on;

select rdb$procedure_name from rdb$procedures where rdb$procedure_name = upper('sp_test');

select
    p.rdb$parameter_name
    ,p.rdb$field_source
    ,f.rdb$field_type
    ,f.rdb$field_sub_type
    ,f.rdb$field_length
    ,f.rdb$character_length
    ,f.rdb$character_set_id
    ,f.rdb$collation_id
    ,c.rdb$character_set_name
    ,s.rdb$collation_name
from rdb$procedure_parameters p
left join rdb$fields f on p.rdb$field_source = f.rdb$field_name
left join rdb$character_sets c on f.rdb$character_set_id = c.rdb$character_set_id
left join rdb$collations s on
    f.rdb$collation_id = s.rdb$collation_id
    and c.rdb$character_set_id = s.rdb$character_set_id
where rdb$procedure_name = upper('sp_test');
'''

expected_stdout = """
    RDB$PROCEDURE_NAME              SP_TEST
    RDB$PARAMETER_NAME              I_NUMBER
    RDB$FIELD_SOURCE                значение числа
    RDB$FIELD_TYPE                  8
    RDB$FIELD_SUB_TYPE              0
    RDB$FIELD_LENGTH                4
    RDB$CHARACTER_LENGTH            <null>
    RDB$CHARACTER_SET_ID            <null>
    RDB$COLLATION_ID                <null>
    RDB$CHARACTER_SET_NAME          <null>
    RDB$COLLATION_NAME              <null>
    RDB$PARAMETER_NAME              O_TEXT
    RDB$FIELD_SOURCE                число прописью
    RDB$FIELD_TYPE                  37
    RDB$FIELD_SUB_TYPE              0
    RDB$FIELD_LENGTH                8191
    RDB$CHARACTER_LENGTH            8191
    RDB$CHARACTER_SET_ID            52
    RDB$COLLATION_ID                126
    RDB$CHARACTER_SET_NAME          WIN1251
    RDB$COLLATION_NAME              вид прописи
"""

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action, tmp_file: Path):
    tmp_file.write_bytes(sql_txt.encode('cp1251'))
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input_file=tmp_file, charset='win1251', io_enc='cp1251')
    assert act.clean_stdout == act.clean_expected_stdout


