#coding:utf-8

"""
ID:          intfunc.listagg.02
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8689
TITLE:       LISTAGG function: DISTINCT does not remove duplicates for BLOB datatype
DESCRIPTION:
NOTES:
    [20.11.2025] pzotov
    Checked on 6.0.0.1356-df73f92.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set blob all;
    recreate sequence g;
    recreate table test (
       id int
      ,b blob
      ,c varchar(12)
    );
    commit;
    insert into test(id, b) values(gen_id(g,1), 'QWERTYASDFGH' );
    update test set c = b;
    insert into test(id, b, c) select gen_id(g,1), b, c from test;
    insert into test(id, b, c) select gen_id(g,1), b, c from test;
    commit;

    -- DISTINCT must remove duplicates for all datatypes except BLOB:
    select
        listagg(distinct b, ':') as blob_id_b
       ,listagg(distinct c, ':') as blob_id_c
    from test;
"""

act = isql_act('db', test_script, substitutions = [('BLOB_ID.*', '')])

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        QWERTYASDFGH:QWERTYASDFGH:QWERTYASDFGH:QWERTYASDFGH
        QWERTYASDFGH
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
