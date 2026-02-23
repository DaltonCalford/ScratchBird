#coding:utf-8

"""
ID:          issue-1247
ISSUE:       1247
TITLE:       Containing not working correctly
DESCRIPTION:
JIRA:        CORE-857
FBTEST:      bugs.core_0857
NOTES:
    [31.10.2024] pzotov
    Bug was fixed for too old FB (2.0 RC4 / 2.1 Alpha 1), firebird-driver and/or QA-plugin
    will not able to run on this version in order to reproduce problem.

    Checked on 6.0.0.511 (Windows/Linux); 5.0.2.1550;  4.0.6.3165; 3.0.2.32670, 3,0,1,32609
"""
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory(charset='WIN1252')
act = isql_act('db', substitutions=[('[ \\t]+', ' ')])
tmp_sql = temp_file('tmp_core_0857.sql')

@pytest.mark.intl
@pytest.mark.version('>=3.0.0')
def test_1(act: Action, tmp_sql: Path):

    test_script = """
        set bail on;
        create collation test_coll_ci_ai for win1252 from WIN_PTBR
        case insensitive
        accent insensitive
        ;

        create table test (
            id int,
            f01 varchar(100),
            f02 varchar(100) collate WIN_PTBR
        );

        insert into test(id, f01) values(1, 'IHF|groß|850xC|P1');
        update test set f02=f01;
        commit;
        create view v_test as
        select octet_length(t.f01) - octet_length(replace(t.f01, 'ß', '')) as "octet_length diff:" from test t;
        commit;

        set list on;
        select c.rdb$character_set_name as connection_cset
        from mon$attachments a
        join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
        where a.mon$attachment_id = current_connection;

        select t.id as "test_1 result:" from rdb$database r left join test t on t.f01 not containing 'P1' and t.f01 like 'IHF|gro_|850_C|P1';
        select t.id as "test_2 result:" from rdb$database r left join test t on t.f01 containing 'P1' and t.f01 like 'IHF|gro_|850_C|P1';
        select t.id as "ci_ai result:" from rdb$database r left join test t on lower(t.f02) = upper(t.f02);
        select t.id as "between result:" from rdb$database r left join test t on lower(t.f01) between lower(t.f02) and upper(t.f02);
        select * from v_test;
    """

    # ::: NB :::
    # For proper output of test, input script must be encoded in cp1252 rather than in UTF-8.
    #
    tmp_sql.write_text(test_script, encoding = 'cp1252')

    act.expected_stdout = """
        CONNECTION_CSET                 WIN1252
        test_1 result:                  <null>
        test_2 result:                  1
        ci_ai result:                   1
        between result:                 1
        octet_length diff:              1
    """

    act.isql(switches = ['-q'], input_file = tmp_sql, charset = 'win1252', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
