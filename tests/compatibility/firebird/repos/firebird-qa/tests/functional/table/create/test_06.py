#coding:utf-8

"""
ID:          table.create-06
TITLE:       CREATE TABLE - two column with same name
DESCRIPTION:
FBTEST:      functional.table.create.06
NOTES:
    [12.07.2025] pzotov
    Removed 'SHOW' command.
    DML actions against a table must meet the DDL of such table.
    Non-ascii names are checked to be sure that quoting is enough for engine to distinguish them.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.949; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    -- must fail:
    create table test(
        c1 smallint,
        c1 integer
    );

    --must PASS:
    create table test(
        "col1" smallint default 1,
        "Col1" integer default 2
    );
    insert into test default values;
    select * from test;
    commit;

    -- must PASS:
    recreate table test(
        "ÇÒL1" smallint default 3,
        "CÒL1" integer default 4
    );
    insert into test default values;
    select * from test;

"""

substitutions = [('[ \t]+', ' '), (r'UNIQUE KEY constraint (")RDB\$INDEX_\d+(")? on table.*', 'UNIQUE KEY constraint RDB_INDEX_x on table')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_stdout_5x = """
        Statement failed, SQLSTATE = 23000
        unsuccessful metadata update
        -CREATE TABLE TEST failed
        -violation of PRIMARY or UNIQUE KEY constraint "RDB$INDEX_15" on table "RDB$RELATION_FIELDS"
        -Problematic key value is ("RDB$FIELD_NAME" = 'C1', "RDB$RELATION_NAME" = 'TEST')
        col1 1
        Col1 2
        ÇÒL1 3
        CÒL1 4
    """

    expected_stdout_6x = """
        Statement failed, SQLSTATE = 23000
        unsuccessful metadata update
        -CREATE TABLE "PUBLIC"."TEST" failed
        -violation of PRIMARY or UNIQUE KEY constraint "RDB$INDEX_15" on table "SYSTEM"."RDB$RELATION_FIELDS"
        -Problematic key value is ("RDB$FIELD_NAME" = 'C1', "RDB$SCHEMA_NAME" = 'PUBLIC', "RDB$RELATION_NAME" = 'TEST')
        col1 1
        Col1 2
        ÇÒL1 3
        CÒL1 4
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
