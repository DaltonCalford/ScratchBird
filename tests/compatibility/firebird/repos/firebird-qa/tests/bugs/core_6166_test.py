#coding:utf-8

"""
ID:          issue-6414
ISSUE:       6414
TITLE:       Problems with long object names (> 255 bytes)
DESCRIPTION:
  We define several objects with non-ascii names of max allowed length (63 characters) and make check statements.
  Result no matter, but these statements must finished without errors.
  Then we extract metadata and add the same set of check statements to this sql script.
  Applying of this script to empty (another) database must end also without any error.

  Confirmed problem on 4.0.0.1633: ISQL crashed when performing script which contains DDL with non-ascii names
  of  max allowed len (63 characters).
JIRA:        CORE-6166
FBTEST:      bugs.core_6166
NOTES:
    [03.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

act = python_act('db')

ddl_script = """
    set term ^;
    recreate package "ПакетДляРешенияЛинейныхГиперболическихИТрансцендентныхУравнений" as
    begin
            function "МетодЗейделяДляЛинейныхГиперболическихИТрансцендентныхУравнений"() returns int;
            function "МетодНьютонаДляЛинейныхГиперболическихИТрансцендентныхУравнений"() returns int;
    end
    ^
    recreate package body "ПакетДляРешенияЛинейныхГиперболическихИТрансцендентныхУравнений" as
    begin
            function "МетодЗейделяДляЛинейныхГиперболическихИТрансцендентныхУравнений"() returns int as
            begin
                    return 123;
            end
            function "МетодНьютонаДляЛинейныхГиперболическихИТрансцендентныхУравнений"() returns int as
            begin
                    return 456;
            end

    end
    ^
    set term ;^
    commit;
    create table "КоэффициентыДляЛинейныхГиперболическихИТрансцендентныхУравнений" (
            "КоэффициентЦДляЛинейныхГиперболическихИТрансцендентныхУравнений" int
       ,"КоэффициентЫДляЛинейныхГиперболическихИТрансцендентныхУравнений" int
       ,"КоэффициентЧДляЛинейныхГиперболическихИТрансцендентныхУравнений" int
    );
    create unique index "КоэффициентыЛинейныхГиперболическихИТрансцендентныхУравненийЦЫЧ"
    on "КоэффициентыДляЛинейныхГиперболическихИТрансцендентныхУравнений" (
            "КоэффициентЦДляЛинейныхГиперболическихИТрансцендентныхУравнений"
       ,"КоэффициентЫДляЛинейныхГиперболическихИТрансцендентныхУравнений"
       ,"КоэффициентЧДляЛинейныхГиперболическихИТрансцендентныхУравнений"
    );
    commit;
"""

test_script = """
    show package;
    show index "КоэффициентыДляЛинейныхГиперболическихИТрансцендентныхУравнений";
    set list on;
    select "ПакетДляРешенияЛинейныхГиперболическихИТрансцендентныхУравнений"."МетодЗейделяДляЛинейныхГиперболическихИТрансцендентныхУравнений"() from rdb$database;
    select "ПакетДляРешенияЛинейныхГиперболическихИТрансцендентныхУравнений"."МетодНьютонаДляЛинейныхГиперболическихИТрансцендентныхУравнений"() from rdb$database;
    rollback;
"""

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action):

    expected_stdout_5x = """
        ПакетДляРешенияЛинейныхГиперболическихИТрансцендентныхУравнений
        КоэффициентыЛинейныхГиперболическихИТрансцендентныхУравненийЦЫЧ UNIQUE INDEX ON КоэффициентыДляЛинейныхГиперболическихИТрансцендентныхУравнений(КоэффициентЦДляЛинейныхГиперболическихИТрансцендентныхУравнений, КоэффициентЫДляЛинейныхГиперболическихИТрансцендентныхУравнений, КоэффициентЧДляЛинейныхГиперболическихИТрансцендентныхУравнений)
        МетодЗейделяДляЛинейныхГиперболическихИТрансцендентныхУравнений 123
        МетодНьютонаДляЛинейныхГиперболическихИТрансцендентныхУравнений 456
    """

    expected_stdout_6x = """
        PUBLIC."ПакетДляРешенияЛинейныхГиперболическихИТрансцендентныхУравнений"
        PUBLIC."КоэффициентыЛинейныхГиперболическихИТрансцендентныхУравненийЦЫЧ" UNIQUE INDEX ON "КоэффициентыДляЛинейныхГиперболическихИТрансцендентныхУравнений"("КоэффициентЦДляЛинейныхГиперболическихИТрансцендентныхУравнений", "КоэффициентЫДляЛинейныхГиперболическихИТрансцендентныхУравнений", "КоэффициентЧДляЛинейныхГиперболическихИТрансцендентныхУравнений")
        МетодЗейделяДляЛинейныхГиперболическихИТрансцендентныхУравнений 123
        МетодНьютонаДляЛинейныхГиперболическихИТрансцендентныхУравнений 456
    """
    expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x

    act.expected_stdout = expected_stdout
    act.isql(switches=[], input=ddl_script + test_script, charset='utf8')
    assert act.clean_stdout == act.clean_expected_stdout

    # Extract metadata
    act.reset()
    act.isql(switches=['-x'], charset='utf8')
    meta = act.stdout
    # drop + recreate database
    act.db.drop()
    act.db.create(sql_dialect=3)
    # Recereate metadata
    act.reset()
    act.isql(switches=[], input=meta, charset='utf8')

    # Check 2
    act.reset()
    act.expected_stdout = expected_stdout
    act.isql(switches=[], input=test_script, combine_output = True, charset='utf8')
    assert act.clean_stdout == act.clean_expected_stdout
