#coding:utf-8

"""
ID:          n/a
TITLE:       SINGULAR - subquery uses CONTAINING
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/C_SQL_PRED_22.script
NOTES:
    [30.09.2025] pzotov
    Checked on 6.0.0.1282; 5.0.4.1715; 4.0.7.3235; 3.0.14.33826.
"""
import os
import pytest
from firebird.qa import *

db = db_factory()

substitutions = [ ('[ \t]+', ' '), ]
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    sql_init = (act.files_dir / 'gtcs-shtest-ddl-and-data.sql').read_text()
    sql_addi = """
        set list on;
        select 'point-00' as msg from rdb$database;
        set count on;

        select *
        from customers c1
        where
            singular
            (
                select * from sales s1
                where s1.custno = c1.custno and s1.ponumb containing '8902'
            )
        order by c1.custno;

        set count off;
        select 'point-01' as msg from rdb$database;
        set count on;

        select *
        from customers c1
        where
            1 =
            (
                select count(*) from sales s1
                where s1.custno = c1.custno and s1.ponumb containing '8902'
            )
        order by c1.custno;

        set count off;
        select 'point-02' as msg from rdb$database;
    """
    
    act.expected_stdout = """
        MSG point-00
        CUSTNO 103
        CUSTOMER The Groton Company
        CONTACT Phil Sheridan
        ADDRESS Great Road
        CITY Groton
        STATE MA
        ZIP_CODE 01450
        PHONE_NO 5086436627
        ON_HOLD <null>
        CUSTNO 104
        CUSTOMER Pope & Saunders Ltd
        CONTACT John Pope
        ADDRESS 1172 Seminole Drive
        CITY Apalachiola
        STATE FL
        ZIP_CODE 32320
        PHONE_NO 3052983744
        ON_HOLD *
        CUSTNO 105
        CUSTOMER 1st Litonian Bank
        CONTACT Joe Hooker
        ADDRESS PO Box 6000, Teutonia Sta
        CITY Milwaukee
        STATE WI
        ZIP_CODE 53206
        PHONE_NO 4142320000
        ON_HOLD <null>
        CUSTNO 106
        CUSTOMER The Bike Mart
        CONTACT Sam Grant
        ADDRESS 499 Hamlin Avenue
        CITY Chicago
        STATE IL
        ZIP_CODE 60624
        PHONE_NO 3123828888
        ON_HOLD <null>
        Records affected: 4
        MSG point-01
        CUSTNO 103
        CUSTOMER The Groton Company
        CONTACT Phil Sheridan
        ADDRESS Great Road
        CITY Groton
        STATE MA
        ZIP_CODE 01450
        PHONE_NO 5086436627
        ON_HOLD <null>
        CUSTNO 104
        CUSTOMER Pope & Saunders Ltd
        CONTACT John Pope
        ADDRESS 1172 Seminole Drive
        CITY Apalachiola
        STATE FL
        ZIP_CODE 32320
        PHONE_NO 3052983744
        ON_HOLD *
        CUSTNO 105
        CUSTOMER 1st Litonian Bank
        CONTACT Joe Hooker
        ADDRESS PO Box 6000, Teutonia Sta
        CITY Milwaukee
        STATE WI
        ZIP_CODE 53206
        PHONE_NO 4142320000
        ON_HOLD <null>
        CUSTNO 106
        CUSTOMER The Bike Mart
        CONTACT Sam Grant
        ADDRESS 499 Hamlin Avenue
        CITY Chicago
        STATE IL
        ZIP_CODE 60624
        PHONE_NO 3123828888
        ON_HOLD <null>
        Records affected: 4
        MSG point-02
    """
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ), combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
