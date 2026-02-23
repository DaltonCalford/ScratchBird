#coding:utf-8

"""
ID:          issue-1375
ISSUE:       1375
TITLE:       Case insensitive collation for UTF-8
DESCRIPTION:
JIRA:        CORE-972
FBTEST:      bugs.core_0972
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    create table test(fc varchar(50) collate unicode_ci, fb blob sub_type 1collate unicode_ci);
    commit;
    insert into test(fc,fb) values ('bibliothèque Éditeur', 'mêlés à des OBJETS DE L''EXTRÊME-Orient');
    insert into test(fc,fb) values ('bibliothÈque éditeur', 'MÊlés à des objETS DE l''extrême-orient');
    commit;
    set list on;
    set blob all;
    set count on;
    select fc as blob_id from test
    where fc='BiblioTHÈQUE ÉDiteuR'

    UNION ALL

    select fb as blob_id from test
    where fb
        between 'MÊLÉS À DES OBJETS DE L''EXTRÊME-ORIENT'
            and 'mÊLÉS À des objETS DE L''EXTRÊME-ORIENT'
    order by 1;
"""

act = isql_act('db', test_script, substitutions=[('BLOB_ID .*', '')])

expected_stdout = """
    bibliothèque Éditeur
    bibliothÈque éditeur
    mêlés à des OBJETS DE L'EXTRÊME-Orient
    MÊlés à des objETS DE l'extrême-orient
    Records affected: 4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

