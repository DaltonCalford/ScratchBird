#coding:utf-8

"""
ID:          n/a
TITLE:       Support for SINGULAR <subquery>
DESCRIPTION:
  Original test see in:
      https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/C_SQL_PRED_0.script
NOTES:
    [29.09.2025] pzotov
    Test uses pre-created script $QA_ROOT/files/gtcs-shtest-ddl-and-data.sql 
    Original database ($QA_ROOT/backups/gtcs_sh_test.fbk) has problems in RDB$ tables:
    relation 'sales' after restore will have duplicates in rdb$relation_fields.rdb$field_position.
    Query "select * from sales ..." will produce columns in DIFFERENT ORDER on 3.x vs 4.x+.
    Also, this DB was created in dialect 1 which is not actual nowadays.

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
        select * from customers c1
        where singular (select * from customers c2 where c1.phone_no = c2.phone_no)
        order by c1.custno
        ;
        set count off;
        select 'point-01' as msg from rdb$database;

        set count on;
        select * from customers c1
        where 1 =  (select count(*) from customers c2 where c1.phone_no = c2.phone_no)
        order by c1.custno
        ;
        set count off;
        select 'point-02' as msg from rdb$database;
    """

    act.expected_stdout = """
        MSG point-00
        CUSTNO 100
        CUSTOMER Thomas Novelty Co.
        CONTACT George Thomas
        ADDRESS 101 Tremont St
        CITY Boston
        STATE MA
        ZIP_CODE 02108
        PHONE_NO 6179291181
        ON_HOLD <null>
        CUSTNO 101
        CUSTOMER Scott Bros Auto Care
        CONTACT Winfield Scott
        ADDRESS 109 Bittersweet Lane
        CITY Randolph
        STATE MA
        ZIP_CODE 02368
        PHONE_NO 6179632219
        ON_HOLD *
        CUSTNO 102
        CUSTOMER JL Music Corp
        CONTACT Ambrose Burnside
        ADDRESS 7801 Escanaba Ave S
        CITY Chicago
        STATE IL
        ZIP_CODE 60649
        PHONE_NO 3123429982
        ON_HOLD <null>
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
        CUSTNO 107
        CUSTOMER Haus auf Pizza
        CONTACT Bill Sherman
        ADDRESS 1234 Main Street
        CITY New Haven
        STATE CT
        ZIP_CODE 06508
        PHONE_NO 2039187728
        ON_HOLD <null>
        CUSTNO 108
        CUSTOMER Carbone Machine Tools
        CONTACT Fritz Siegal
        ADDRESS 902 Bedford Street
        CITY Burlington
        STATE MA
        ZIP_CODE 01803
        PHONE_NO 6177271181
        ON_HOLD <null>
        CUSTNO 109
        CUSTOMER Czbrynik Meat Packer
        CONTACT George Meade
        ADDRESS 3 Meat Market
        CITY Green Bay
        STATE WI
        ZIP_CODE 54305
        PHONE_NO 4142983288
        ON_HOLD <null>
        CUSTNO 110
        CUSTOMER Sporting Life Health Bars
        CONTACT George Custer
        ADDRESS 1302 Wawona Street
        CITY San Francisco
        STATE CA
        ZIP_CODE 94116
        PHONE_NO 4152092283
        ON_HOLD <null>
        CUSTNO 112
        CUSTOMER LOM Incorporated
        CONTACT Don Carlos Buell
        ADDRESS 1812 Dearborn Street
        CITY Detroit
        STATE MI
        ZIP_CODE 48209
        PHONE_NO 3132320900
        ON_HOLD <null>
        CUSTNO 113
        CUSTOMER Nature's Food Co-op
        CONTACT John Logan
        ADDRESS Harvard Square
        CITY Cambridge
        STATE MA
        ZIP_CODE 02139
        PHONE_NO 6172839229
        ON_HOLD <null>
        CUSTNO 114
        CUSTOMER 1st Canine Bank
        CONTACT George McClellan
        ADDRESS 1342 Massachusetts Avenue
        CITY Boston
        STATE MA
        ZIP_CODE 02113
        PHONE_NO 6179291622
        ON_HOLD <null>
        Records affected: 14
        MSG point-01
        CUSTNO 100
        CUSTOMER Thomas Novelty Co.
        CONTACT George Thomas
        ADDRESS 101 Tremont St
        CITY Boston
        STATE MA
        ZIP_CODE 02108
        PHONE_NO 6179291181
        ON_HOLD <null>
        CUSTNO 101
        CUSTOMER Scott Bros Auto Care
        CONTACT Winfield Scott
        ADDRESS 109 Bittersweet Lane
        CITY Randolph
        STATE MA
        ZIP_CODE 02368
        PHONE_NO 6179632219
        ON_HOLD *
        CUSTNO 102
        CUSTOMER JL Music Corp
        CONTACT Ambrose Burnside
        ADDRESS 7801 Escanaba Ave S
        CITY Chicago
        STATE IL
        ZIP_CODE 60649
        PHONE_NO 3123429982
        ON_HOLD <null>
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
        CUSTNO 107
        CUSTOMER Haus auf Pizza
        CONTACT Bill Sherman
        ADDRESS 1234 Main Street
        CITY New Haven
        STATE CT
        ZIP_CODE 06508
        PHONE_NO 2039187728
        ON_HOLD <null>
        CUSTNO 108
        CUSTOMER Carbone Machine Tools
        CONTACT Fritz Siegal
        ADDRESS 902 Bedford Street
        CITY Burlington
        STATE MA
        ZIP_CODE 01803
        PHONE_NO 6177271181
        ON_HOLD <null>
        CUSTNO 109
        CUSTOMER Czbrynik Meat Packer
        CONTACT George Meade
        ADDRESS 3 Meat Market
        CITY Green Bay
        STATE WI
        ZIP_CODE 54305
        PHONE_NO 4142983288
        ON_HOLD <null>
        CUSTNO 110
        CUSTOMER Sporting Life Health Bars
        CONTACT George Custer
        ADDRESS 1302 Wawona Street
        CITY San Francisco
        STATE CA
        ZIP_CODE 94116
        PHONE_NO 4152092283
        ON_HOLD <null>
        CUSTNO 112
        CUSTOMER LOM Incorporated
        CONTACT Don Carlos Buell
        ADDRESS 1812 Dearborn Street
        CITY Detroit
        STATE MI
        ZIP_CODE 48209
        PHONE_NO 3132320900
        ON_HOLD <null>
        CUSTNO 113
        CUSTOMER Nature's Food Co-op
        CONTACT John Logan
        ADDRESS Harvard Square
        CITY Cambridge
        STATE MA
        ZIP_CODE 02139
        PHONE_NO 6172839229
        ON_HOLD <null>
        CUSTNO 114
        CUSTOMER 1st Canine Bank
        CONTACT George McClellan
        ADDRESS 1342 Massachusetts Avenue
        CITY Boston
        STATE MA
        ZIP_CODE 02113
        PHONE_NO 6179291622
        ON_HOLD <null>
        Records affected: 14
        MSG point-02
    """
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ), combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
