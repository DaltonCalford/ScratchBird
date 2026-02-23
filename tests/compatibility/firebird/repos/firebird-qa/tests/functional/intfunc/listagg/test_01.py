#coding:utf-8

"""
ID:          intfunc.listagg.01
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8689
TITLE:       LISTAGG function - basic test
DESCRIPTION:
    Only basic features are checked here, code contains only examples from tracker.
    More complex test(s) will be implemented later.
NOTES:
    [18.11.2025] pzotov
    Checked on 6.0.0.1356-df73f92.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test
    	(col1 int, col2 varchar(2), col3 varchar(2), col4 varchar(2), col5 boolean, col6 varchar(2)
    	character set win1251);
    commit;
    insert into test values(1, 'a', 'a', 'j', false, 'п');
    insert into test values(2, 'b', 'b', 'i', false, 'д');
    insert into test values(3, 'c', 'a', 'l', true,  'ж');
    insert into test values(4, 'd', 'b', 'k', true,  'й');
    commit;

    select listagg (all col4, ':') as blob_id from test;
    select listagg (distinct col4, ':') as blob_id from test;
    select listagg (distinct col3, ':') as blob_id from test;
    select listagg (distinct col3, ':') within group (order by col2) as blob_id from test;
    select listagg (distinct col3, ':') within group (order by col2 descending) as blob_id from test;
    select listagg (col2, ':') within group (order by col2 descending) as blob_id from test;
    select listagg (col4, ':') within group (order by col3 desc) as blob_id from test;
    select listagg (col3, ':') within group (order by col5 ascending) as blob_id from test;
    select listagg (col4, ':') within group (order by col3 asc) as blob_id from test;
    select listagg (all col2) within group (order by col4) as blob_id from test;
    select listagg (col2, ':') within group (order by col3 desc, col4 asc) as blob_id from test;
    select listagg (col2, ':') within group (order by col3 desc, col4 desc) as blob_id from test;
    select listagg (col2, ':') within group (order by col3 asc, col4 desc) as blob_id from test;
    select listagg (all col6, ':') as blob_id from test;
    select listagg (all col6, ':') within group (order by col2 desc) as blob_id from test;
    select listagg (all col2, ':') within group (order by col6) as blob_id from test;

    insert into test values(5, 'e', null, null, null, null);
    insert into test values(6, 'f', 'c', 'n', true, 'к');

    select listagg (all col2, ':') within group (order by col3) as blob_id from test;
    select listagg (all col2, ':') within group (order by col3 nulls last) as blob_id from test;
    select listagg (all col2, ':') within group (order by col6 nulls first) as blob_id from test;

    -- following sttm is shown as raising error (see github.com/FirebirdSQL/firebird/pull/8689#issue-3295810019)
    -- but this is not so on 6.0.0.1356-df73f92:
    select listagg (distinct col3, ':') within group (order by col2) as blob_id from test;
"""

act = isql_act('db', test_script, substitutions = [('BLOB_ID .*', '')])

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        j:i:l:k
        i:j:k:l
        a:b
        a:b
        a:b
        d:c:b:a
        i:k:j:l
        a:b:a:b
        j:l:i:k
        b,a,d,c
        b:d:a:c
        d:b:c:a
        c:a:d:b
        п:д:ж:й
        й:ж:д:п
        b:c:d:a
        e:a:c:b:d:f
        a:c:b:d:f:e
        e:b:c:d:f:a
        a:b:c
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
