#coding:utf-8

"""
ID:          intfunc.misc.gen_uuid
TITLE:       GEN_UUID()
DESCRIPTION: Returns a universal unique number.
FBTEST:      functional.intfunc.misc.gen_uuid_01
"""

import pytest
from firebird.qa import *

db = db_factory()

N_COUNT = 50;

test_script = f"""
    set list on;
    create table test( s char(16) character set octets, b blob );
    set term ^;
    execute block as
        declare i int  = 0;
    begin
        while (i < {N_COUNT}) do
        begin
            insert into test(s, b) values(GEN_UUID(), GEN_UUID());
            i = i + 1;
        end
    end
    ^
    set term ;^
    commit;

    select count(distinct s) as count_uniq_char from test;
    select count(distinct b) as count_uniq_blob from test;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = f"""
    COUNT_UNIQ_CHAR {N_COUNT}
    COUNT_UNIQ_BLOB {N_COUNT}
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
