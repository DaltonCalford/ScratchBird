#coding:utf-8

"""
ID:          issue-1564
ISSUE:       1564
TITLE:       Cannot alter generator's comment to the same value
DESCRIPTION:
JIRA:        CORE-1142
NOTES:
    [31.12.2024] pzotov
    Removed 'SHOW' command because its output can change during intensive development.
    Also, 'SHOW COMMENT ON <objname>' is not valid in ISQL, see:
    https://github.com/FirebirdSQL/firebird-qa/pull/33/files

    Parsing problem appeared on 6.0.0.0.570 after d6ad19aa07deeaac8107a25a9243c5699a3c4ea1
    ("Refactor ISQL creating FrontendParser class").
    
    Checked on 6.0.0.570, 5.0.2.1583
"""

import pytest
from firebird.qa import *

GEN_NAME = 'TEST_GEN'

init_script = f"""
    create generator {GEN_NAME};
    create view v_show_gen_descr as
    select g.rdb$description as descr_blob_id
    from rdb$generators g
    where g.rdb$generator_name = '{GEN_NAME.upper()}';
    commit;
"""

db = db_factory(init=init_script)

test_script = f"""
    set blob all;
    set list on;
    set count on;
    comment on generator {GEN_NAME} is 'comment N1';
    commit;
    select * from v_show_gen_descr;

    comment on generator {GEN_NAME} is 'comment N1';
    commit;
    select * from v_show_gen_descr;

    comment on generator {GEN_NAME} is 'comment N11';
    commit;
    select * from v_show_gen_descr;

    comment on generator {GEN_NAME} is 'comment N11';
    commit;
    select * from v_show_gen_descr;
"""

act = isql_act('db', test_script,  substitutions=[('[ \t]+', ' '), ('DESCR_BLOB_ID .*', '')])

expected_stdout = """
    comment N1
    Records affected: 1
    
    comment N1
    Records affected: 1
    
    comment N11
    Records affected: 1

    comment N11
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

