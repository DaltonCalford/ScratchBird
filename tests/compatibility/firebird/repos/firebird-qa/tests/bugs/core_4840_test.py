#coding:utf-8

"""
ID:          issue-5136
ISSUE:       5136
TITLE:       Transactions with isc_tpb_autocommit can hang the server
DESCRIPTION:
JIRA:        CORE-4840
FBTEST:      bugs.core_4840
"""

import pytest
from firebird.qa import *
from firebird.driver import TPB, Isolation

db = db_factory(charset='UTF8')

act = python_act('db')

sp_ddl = """
create or alter procedure sp_test (in1 integer, in2 float)
returns (
    out1 varchar(20),
    out2 double precision,
    out3 integer
) as
    declare x integer;
begin
    out1 = 'out string';
    out2 = 2 * in2;
    out3 = 3 * in1;
    suspend;
end
"""

expected_stdout = """
    OUT1                            out string
    OUT2                            6.283185005187988
    OUT3                            37035
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        custom_tpb = TPB(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, auto_commit=True).get_buffer()
        con.begin(custom_tpb)
        c = con.cursor()
        sp_rem = "comment on procedure sp_test is 'Det är inte alla präster, som göra prästgården eller dess torp till änkesäte åt orkeslösa trotjänarinnor, hade biskopen en gång sagt om Helenas prost.'"
        # For confirmation of ticket issues it is enough to do just this:
        c.execute(sp_ddl)
        c.execute(sp_rem)
        con.commit()
    #
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input='set list on; select * from sp_test(12345, 3.1415926);')
    assert act.clean_stdout == act.clean_expected_stdout
