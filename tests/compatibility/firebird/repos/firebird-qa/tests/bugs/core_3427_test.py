#coding:utf-8

"""
ID:          issue-3789
ISSUE:       3789
TITLE:       Server crashing with UTF8 blobs
DESCRIPTION:
JIRA:        CORE-3427
FBTEST:      bugs.core_3427
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    create table tbl (blob_field blob sub_type text character set utf8 collate unicode_ci_ai);
    -- See ticket: seems that this crash depended on concrete data, so it was decided to copy these text from ticket:
    insert into tbl values ('крупнейший европейский журнал о компьютерах. Вышел на рынок компьютерных изданий с уникальной концепцией и предназначен для людей, которые интересуются компьютерами, Интернетом, средствами телекоммуникаций, аудио-, видео- и фототехникой. Каждые две недели читателям предлагаются новости индустрии, тесты оборудования и программ, обучающие курсы и практические советы. Издание интересно как новичкам, так и опытным пользователям.');
    commit;
    -- Confirmed crash on 2.5.0, fine on 2.5.1 and later (02.04.2015):
    set list on;
    select count(*)  cnt
    from tbl
    where blob_field like '%test%';
"""

act = isql_act('db', test_script)

expected_stdout = """
    CNT                             0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

