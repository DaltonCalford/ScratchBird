#coding:utf-8

"""
ID:          issue-1766
ISSUE:       1766
TITLE:       Unexpected "cannot transliterate" error
DESCRIPTION:
JIRA:        CORE-1347
FBTEST:      bugs.core_1347
"""

import pytest
from firebird.qa import *
from pathlib import Path

db = db_factory(charset='WIN1251')

act = python_act('db')

expected_stdout = """
    RDB$PROCEDURE_NAME              SP_TEST
    Records affected: 1
"""

tmp_file = temp_file('non_ascii_ddl.sql')

sql_txt = '''set bail on;

set term ^ ;
create procedure sp_test (
  p_tablename varchar(30) ,
  p_idname varchar(30) ,
  p_seqname varchar(30) ,
  p_isusefunc smallint
)
returns (
  column_value bigint
)
as
declare variable l_maxid bigint;
begin
  /*
  -- Находим разрыв в значениях ПК таблицы
  -- если разрыв отсутствует то дергаем секвенс
  -- p_IsUseFunc=1 - дергать секвенс ч/з ф-цию GetSeqValue
  */
end ^
set term ;^
commit;

set list on;
set count on;

select pr.rdb$procedure_name
from rdb$procedures pr
where pr.rdb$procedure_source containing '1'
and pr.rdb$procedure_name = upper('sp_test');
'''

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action, tmp_file: Path):
    tmp_file.write_bytes(sql_txt.encode('cp1251'))
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input_file=tmp_file, charset='win1251', io_enc='cp1251')
    assert act.clean_stdout == act.clean_expected_stdout


