#coding:utf-8

"""
ID:          gtcs.cast-datatypes
TITLE:       Check result or CAST() for misc agruments // from GTCS.
DESCRIPTION: 
    Original tests see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_CAST1_ISQL.script
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_CAST2_ISQL.script
    ...
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROC_CAST10_ISQL.script

    This test uses pre-created script ( <QA_ROOT>/files/gtcs-cast-gen-ddl.sql ) which produces
    syntactically correct SQL code for further applying against test DB. This auto-generated
    SQL code contains of lot of 'CREATE PROCEDURE' statements which create stored procedures
    with accepting single input parameter and making cast of this argument to other type and 
    returning it to caller. All possible combinations of datatypes are covered in such manner.
    Cast is performed mostly IMPLICITLY, with exception for DATE/TIME literals such as 'TODAY',
    'TOMORROW' and 'YESTERDAY'.
    Names of auto-generated stored procedures have form '<FromDatatype>_<ToDatatype>', e.g.:
        create or alter procedure "bigint_char(10)" ( a bigint )
        returns ( b char(10) ) as
        begin
            b = 1608.90*a/100.00;
            suspend;
        end
NOTES:
    Checked on 4.0.1.2692.
"""

import pytest
from firebird.qa import *
import sys

db = db_factory()

act = python_act('db', substitutions=[ ('BLOB_ID.*', ''), ('[ \t]+', ' ') ])

expected_stdout = """
    bigint_bigint                   80 
    BLOB_ID                         0:1 
    80.4450 
    bigint_char(10)                 80.4450 
    bigint_date                     2003-04-22 
    bigint_decimal( 4,2)            0.04 
    bigint_decimal( 4,2)            0.05 
    bigint_decimal(10,4)            80.4450 
    bigint_double precision         80.44499999999999 
    bigint_float                    80.445 
    bigint_nchar(10)                80.4450 
    bigint_numeric( 4,2)            0.04 
    bigint_numeric( 4,2)            0.05 
    bigint_numeric(10,4)            80.4450 
    bigint_smallint                 80 
    bigint_time                     01:02:03.0000 
    bigint_timestamp                2003-04-22 11:35:39.0000 
    bigint_varchar(10)              80.4450 
    blob_bigint                     9223372036854775807 
    blob_boolean                    <true> 
    blob_char(30)                   81985529216487135 
    blob_date                       2004-02-29 
    blob_decimal(5,2)               80.45 
    blob_double precision           80.44499999999999 
    blob_float                      80.445 
    blob_int                        -2147483648 
    blob_nchar(30)                  81985529216487135 
    blob_numeric(5,2)               80.45 
    blob_smallint                   32767 
    blob_time                       01:02:03.4560 
    blob_varchar(30)                81985529216487135 
    char(30)_bigint                 9223372036854775807 
    BLOB_ID                         0:1 
    81985529216487135 
    char(30)_boolean                <true> 
    char(30)_date                   2004-02-29 
    char(30)_decimal(5,2)           80.45 
    char(30)_double precision       80.44499999999999 
    char(30)_float                  80.445 
    char(30)_int                    -2147483648 
    char(30)_nchar(30)              81985529216487135 
    char(30)_numeric(5,2)           80.45 
    char(30)_smallint               32767 
    char(30)_time                   01:02:03.4560 
    char(30)_varchar(30)            81985529216487135 
    date_bigint                     147558 
    BLOB_ID                         0:1 
    2004-02-29 
    date_char(10)                   2004-02-29 
    date_decimal(4,2)               2.00 
    date_double precision           2.000000000000000 
    date_float                      2 
    date_int                        147558 
    date_nchar(10)                  2004-02-29 
    date_numeric(4,2)               2.00 
    date_smallint                   1461 
    date_time                       01:02:05.0000 
    date_timestamp                  2003-02-03 01:02:03.0000 
    date_varchar(10)                2004-02-29 
    decimal(4,2)_bigint             80 
    BLOB_ID                         0:1 
    0.05 
    BLOB_ID                         0:3 
    0.06 
    BLOB_ID                         0:5 
    0.08 
    decimal(4,2)_char(10)           0.05 
    decimal(4,2)_char(10)           0.06 
    decimal(4,2)_char(10)           0.08 
    decimal(4,2)_date               2003-04-22 
    decimal(4,2)_decimal(4,2)       0.05 
    decimal(4,2)_decimal(4,2)       0.06 
    decimal(4,2)_decimal(4,2)       0.08 
    decimal(4,2)_double precision   80.45000000000000 
    decimal(4,2)_double precision   0.05000000000000000 
    decimal(4,2)_double precision   0.06000000000000000 
    decimal(4,2)_double precision   0.08000000000000000 
    decimal(4,2)_float              80.449997 
    decimal(4,2)_float              0.050000001 
    decimal(4,2)_float              0.059999999 
    decimal(4,2)_float              0.079999998 
    decimal(4,2)_int                80 
    decimal(4,2)_nchar(10)          0.05 
    decimal(4,2)_nchar(10)          0.06 
    decimal(4,2)_nchar(10)          0.08 
    decimal(4,2)_numeric(4,2)       0.05 
    decimal(4,2)_numeric(4,2)       0.06 
    decimal(4,2)_numeric(4,2)       0.08 
    decimal(4,2)_smallint           80 
    decimal(4,2)_time               01:03:23.4500 
    decimal(4,2)_timestamp          2003-04-22 11:50:03.0000 
    decimal(4,2)_varchar(10)        0.05 
    decimal(4,2)_varchar(10)        0.06 
    decimal(4,2)_varchar(10)        0.08 
    double precision_bigint         80 
    BLOB_ID                         0:1 
    80.44499999999999 
    double precision_char(10)       80.445000 
    double precision_date           2003-04-22 
    ouble precision_decimal(10,4)   80.4450 
    double precision_decimal(4,2)   0.05 
    double precision_decimal(4,2)   0.06 
    double precision_decimal(4,2)   0.08 
    double precision_float          80.445 
    double precision_int            80 
    double precision_nchar(10)      80.445000 
    ouble precision_numeric(10,4)   80.4450 
    double precision_numeric(4,2)   0.05 
    double precision_numeric(4,2)   0.06 
    double precision_numeric(4,2)   0.08 
    double precision_smallint       80 
    double precision_time           01:03:23.4450 
    double precision_timestamp      2003-04-22 11:42:51.0000 
    double precision_varchar(10)    80.445000 
    float_bigint                    80 
    BLOB_ID                         0:1 
    80.445000 
    float_char(10)                  80.445000 
    float_date                      2003-04-22 
    float_decimal(10,4)             80.4450 
    float_decimal(4,2)              0.05 
    float_double precision          80.44499969482422 
    float_int                       80 
    float_nchar(10)                 80.445000 
    float_numeric( 4,2)             0.05 
    float_numeric(10,4)             80.4450 
    float_smallint                  80 
    float_time                      01:03:23.4450 
    float_timestamp                 2003-04-22 11:42:50.9736 
    float_varchar(10)               80.445000 
    int_bigint                      80 
    BLOB_ID                         0:1 
    80.4450 
    int_char(10)                    80.4450 
    int_date                        2003-04-22 
    int_decimal( 4,2)               0.04 
    int_decimal( 4,2)               0.05 
    int_decimal(10,4)               80.4450 
    int_double precision            80.44499999999999 
    int_float                       80.445 
    int_nchar(10)                   80.4450 
    int_numeric( 4,2)               0.04 
    int_numeric( 4,2)               0.05 
    int_numeric(10,4)               80.4450 
    int_smallint                    80 
    int_time                        01:02:03.0000 
    int_timestamp                   2003-04-22 11:35:39.0000 
    int_varchar(10)                 80.4450 
    nchar(30)_bigint                9223372036854775807 
    BLOB_ID                         0:1 
    81985529216487135 
    nchar(30)_boolean               <true> 
    nchar(30)_char(30)              81985529216487135 
    nchar(30)_date                  2004-02-29 
    nchar(30)_decimal(5,2)          80.45 
    nchar(30)_double precision      80.44499999999999 
    nchar(30)_float                 80.445 
    nchar(30)_int                   -2147483648 
    nchar(30)_numeric(5,2)          80.45 
    nchar(30)_smallint              32767 
    nchar(30)_time                  01:02:03.4560 
    nchar(30)_varchar(30)           81985529216487135 
    numeric(4,2)_bigint             80 
    BLOB_ID                         0:1 
    0.05 
    BLOB_ID                         0:3 
    0.06 
    BLOB_ID                         0:5 
    0.08 
    numeric(4,2)_char(10)           0.05 
    numeric(4,2)_char(10)           0.06 
    numeric(4,2)_char(10)           0.08 
    numeric(4,2)_date               2003-04-22 
    numeric(4,2)_decimal(4,2)       0.05 
    numeric(4,2)_decimal(4,2)       0.06 
    numeric(4,2)_decimal(4,2)       0.08 
    numeric(4,2)_double precision   80.45000000000000 
    numeric(4,2)_double precision   0.05000000000000000 
    numeric(4,2)_double precision   0.06000000000000000 
    numeric(4,2)_double precision   0.08000000000000000 
    numeric(4,2)_float              80.449997 
    numeric(4,2)_float              0.050000001 
    numeric(4,2)_float              0.059999999 
    numeric(4,2)_float              0.079999998 
    numeric(4,2)_int                80 
    numeric(4,2)_nchar(10)          0.05 
    numeric(4,2)_nchar(10)          0.06 
    numeric(4,2)_nchar(10)          0.08 
    numeric(4,2)_numeric(4,2)       0.05 
    numeric(4,2)_numeric(4,2)       0.06 
    numeric(4,2)_numeric(4,2)       0.08 
    numeric(4,2)_smallint           80 
    numeric(4,2)_time               01:03:23.4500 
    numeric(4,2)_timestamp          2003-04-22 11:50:03.0000 
    numeric(4,2)_varchar(10)        0.05 
    numeric(4,2)_varchar(10)        0.06 
    numeric(4,2)_varchar(10)        0.08 
    smallint_bigint                 10922 
    BLOB_ID                         0:1 
    80.4450 
    smallint_char(10)               80.4450 
    smallint_date                   2003-11-19 
    smallint_decimal( 4,2)          80.45 
    smallint_decimal(10,4)          80.4450 
    smallint_double precision       80.44499999999999 
    smallint_float                  80.445 
    smallint_int                    -10922 
    smallint_int                    10922 
    smallint_nchar(10)              80.4450 
    smallint_numeric( 4,2)          80.45 
    smallint_numeric(10,4)          80.4450 
    smallint_time                   01:06:55.0000 
    smallint_timestamp              2003-11-21 01:02:03.0000 
    smallint_varchar(10)            80.4450 
    time_bigint                     82677 
    BLOB_ID                         0:1 
    01:02:03.0000 
    time_char(13)                   01:02:03.0000 
    time_date                       2003-02-01 
    time_decimal(10,2)              82676.67 
    time_double precision           82676.66600000000 
    time_float                      82676.664 
    time_int                        82677 
    time_nchar(13)                  01:02:03.0000 
    time_numeric(10,2)              82676.67 
    time_smallint                   3661 
    time_timestamp                  2003-02-01 01:02:03.0000 
    time_varchar(13)                01:02:03.0000 
    timestamp_bigint                1 
    BLOB_ID                         0:1 
    2004-02-29 01:02:03.4560 
    timestamp_char(30)              2004-02-29 01:02:03.4560 
    timestamp_date                  2004-02-29 
    timestamp_decimal(10,2)         0.58 
    timestamp_double precision      0.5755401160000000 
    timestamp_float                 0.57554013 
    timestamp_int                   1 
    timestamp_nchar(30)             2004-02-29 01:02:03.4560 
    timestamp_numeric(10,2)         0.58 
    timestamp_smallint              0 
    timestamp_time                  01:02:03.0000 
    timestamp_varchar(30)           2004-02-29 01:02:03.4560 
    varchar(30)_bigint              -268435456 
    varchar(30)_bigint              4026531840 
    varchar(30)_bigint              9223372036854775807 
    varchar(30)_bigint              -1 
    BLOB_ID                         0:1
    81985529216487135 
    varchar(30)_boolean             <true> 
    varchar(30)_char(30)            81985529216487135 
    varchar(30)_date                2004-02-29 
    varchar(30)_decimal(5,2)        80.45 
    varchar(30)_double precision    80.44499999999999 
    varchar(30)_float               80.445 
    varchar(30)_int                 -2147483648 
    varchar(30)_nchar(30)           81985529216487135 
    varchar(30)_numeric(5,2)        80.45
    varchar(30)_smallint            32767 
    varchar(30)_time                01:02:03.4560 
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    
    act.isql(switches=['-q'], input_file = act.files_dir / 'gtcs-cast-gen-ddl.sql')
    init_script = act.stdout
    init_err = act.stderr
    assert init_err == ''
    act.reset()
    
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = init_script, combine_output= True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
