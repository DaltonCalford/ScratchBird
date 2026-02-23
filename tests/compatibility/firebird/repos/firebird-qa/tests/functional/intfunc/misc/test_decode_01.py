#coding:utf-8

"""
ID:          intfunc.misc.decode
TITLE:       DECODE()
DESCRIPTION: DECODE is a shortcut for a case when else expreession.
FBTEST:      functional.intfunc.misc.decode_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SET ECHO OFF;
CREATE  TABLE TMPTEST( id INTEGER );

insert into TMPTEST(id)
values(1);
insert into TMPTEST(id)
values(2);
insert into TMPTEST(id)
values(3);
insert into TMPTEST(id)
values(4);
insert into TMPTEST(id)
values(5);

-- count doit etre egal a 0 dans ce cas
select decode(id,1,'un',2,'deux',3,'trois','plus grand') from TMPTEST;"""

act = isql_act('db', test_script)

expected_stdout = """
DECODE
==========
un
deux
trois
plus grand
plus grand
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
