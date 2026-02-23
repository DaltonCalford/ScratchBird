#coding:utf-8

"""
ID:          procedure.create-04
TITLE:       CREATE PROCEDURE - Output paramaters
DESCRIPTION:
FBTEST:      functional.procedure.create.04
"""

import pytest
from firebird.qa import *

db = db_factory()

SP_BODY = """
        declare variable p1 smallint;
        declare variable p2 integer;
        declare variable p3 float;
        declare variable p4 double precision;
        declare variable p5 decimal(9,3);
        declare variable p6 numeric(10,4);
        declare variable p7 date;
        declare variable p8 time;
        declare variable p9 timestamp;
        declare variable p10 char(40);
        declare variable p11 varchar(60);
        declare variable p12 nchar(70);
    begin
        p1=1;
        p2=2;
        p3=3.4;
        p4=4.5;
        p5=5.6;
        p6=6.7;
        p7='31.8.1995';
        p8='13:45:57.1';
        p9='29.2.200 14:46:59.9';
        p10='text p10';
        p11='text p11';
        p12='text p13';
    end
"""

test_script = f"""
    set term ^;
    create procedure sp_test as
    {SP_BODY}
    ^
    set term ;^
    commit;
    show procedure sp_test;
"""

act = isql_act('db', test_script, substitutions = [('=====*','')])

@pytest.mark.version('>=3')
def test_1(act: Action):

    expected_stdout_5x = f"""
        Procedure text:
        {SP_BODY}
    """

    expected_stdout_6x = f"""
        Procedure: PUBLIC.SP_TEST
        Procedure text:
        {SP_BODY}
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
