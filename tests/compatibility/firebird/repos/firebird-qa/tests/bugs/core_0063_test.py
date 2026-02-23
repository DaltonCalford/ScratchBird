#coding:utf-8

"""
ID:          issue-388
ISSUE:       388
TITLE:       Sequence of commands crash server
DESCRIPTION:
JIRA:        CORE-63
FBTEST:      bugs.core_0063
"""

import pytest
from firebird.qa import *

db = db_factory(charset='WIN1252')

test_script = """
    set bail on;

    create domain d_descricao_30000_nn as varchar(30000) not null collate win_ptbr;
    create table movimento( complemento d_descricao_30000_nn );

    insert into movimento values ('');
    insert into movimento values ('1234567890');
    insert into movimento values ('');

    create domain d_text_blob as blob sub_type text collate win_ptbr;

    alter table movimento add complemento2 d_text_blob;

    update movimento set complemento2 = complemento;

    alter table movimento drop complemento, add complemento d_text_blob;

    drop domain d_descricao_30000_nn;

    update movimento set complemento = complemento2;
    set list on;
    select 'OK' as result from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT                          OK
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

