#coding:utf-8

"""
ID:          issue-3437
ISSUE:       3437
TITLE:       Allow the usage of blobs in COMPUTED BY expressions
DESCRIPTION:
JIRA:        CORE-3057
FBTEST:      bugs.core_3057
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(
        b1 blob sub_type 1 character set utf8 collate unicode_ci_ai,
        b2 blob sub_type 1 character set utf8 collate unicode_ci_ai,
        bconcat1 computed by ( b1 || b2 ) -- this FAILS on 2.5.4
    );
    commit;
    alter table test
	     add bconcat2 computed by ( b1 || b2 || bconcat1 )
		,add brepl1_2 computed by ( replace(b1, b2, '1') )
		,add brepl2_1 computed by ( replace(b2, b1, '2') )
	;
    commit;
    --show table test;
    insert into test(b1, b2) values(
	    'ÁÉÍÓÚÝÀÈÌÒÙÂÊÎÔÛÃÑÕÄËÏÖÜŸÇŠĄĘŹŻĂŞŢ',
		'aeiouyaeiouaeiouanoaeiouycsaezzast'
	);

    set list on;
    set blob all;
    select
		b1 as blob_id_b1,
		b2 as blob_id_b2,
		bconcat1 as blob_id_bconcat1,
		bconcat2 as blob_id_bconcat2,
		brepl1_2 as blob_id_repl1_2,
		brepl2_1 as blob_id_repl2_1
	from test
	;
"""

act = isql_act('db', test_script, substitutions=[('BLOB_ID_.*', 'BLOB_ID')])

expected_stdout = """
	BLOB_ID_B1                      86:0
	ÁÉÍÓÚÝÀÈÌÒÙÂÊÎÔÛÃÑÕÄËÏÖÜŸÇŠĄĘŹŻĂŞŢ
	BLOB_ID_B2                      86:1
	aeiouyaeiouaeiouanoaeiouycsaezzast
	BLOB_ID_BCONCAT1                0:12
	ÁÉÍÓÚÝÀÈÌÒÙÂÊÎÔÛÃÑÕÄËÏÖÜŸÇŠĄĘŹŻĂŞŢaeiouyaeiouaeiouanoaeiouycsaezzast
	BLOB_ID_BCONCAT2                0:f
	ÁÉÍÓÚÝÀÈÌÒÙÂÊÎÔÛÃÑÕÄËÏÖÜŸÇŠĄĘŹŻĂŞŢaeiouyaeiouaeiouanoaeiouycsaezzastÁÉÍÓÚÝÀÈÌÒÙÂÊÎÔÛÃÑÕÄËÏÖÜŸÇŠĄĘŹŻĂŞŢaeiouyaeiouaeiouanoaeiouycsaezzast
	BLOB_ID_REPL1_2                 0:8
	1
	BLOB_ID_REPL2_1                 0:5
	2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

