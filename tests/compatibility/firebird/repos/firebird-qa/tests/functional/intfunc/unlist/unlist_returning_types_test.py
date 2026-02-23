#coding:utf-8

"""
ID:          issue-8418
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8418
TITLE:       UNLIST function. Check output for different returning types
DESCRIPTION: Provided by red-soft. Original file name: "unlist.test_returning_types.py"
NOTES:
    [09.04.2025] pzotov
    Checked on 6.0.0.725

    [28.12.2025] pzotov
    Changed substitutions list: value +/-0e0 can be displayed with 16 digits after decimal point.
    We have to suppress "excessive" 16th digit (zero).
    Detected on Intel Xeon W-2123 ("Intel64 Family 6 Model 85 Stepping 4") // Windows-10
"""

import pytest
from firebird.qa import *

db = db_factory()

SELECTED_TIMEZONE = 'Indian/Cocos'
test_script = f"""
    set list on;
    set blob all;
    set time zone '{SELECTED_TIMEZONE}';
    -- bigint
    select * from unlist('-9223372036854775808,9223372036854775807' returning bigint) as a(unlist_bigint_01);
    select * from unlist('-9223372036854775809' returning bigint) as a(unlist_bigint_02);

    -- Must raise two time: SQLSTATE = 22003 / ... / -numeric value is out of range:
    select * from unlist('9223372036854775808' returning bigint) as a(unlist_bigint_03);

    -- boolean
    select * from unlist('true,false' returning boolean) as a(unlist_boolean_01);

    -- SQLSTATE = 22018 / conversion error from string "right":
    select * from unlist('right' returning boolean) as a(unlist_boolean_02);

    -- binary(n): following must pass:
    select * from unlist('1111,12345678' returning binary(8)) as a(unlist_binary_01);
    select * from unlist('text,texttext' returning binary(8)) as a(unlist_binary_02);

    -- char(n)

    -- must pass:
    select * from unlist('text,texttext,1243' returning char(8)) as a(unlist_char_01);
    
    -- must raise SQLSTATE = 22001 / ... / -string right truncation / -expected length 1, actual 4:
    select * from unlist('text' returning char) as a(unlist_char_02);

    -- must raise SQLSTATE = 42000 / ... / -Positive value expected:
    select * from unlist('text' returning char(0)) as a(unlist_char_03);

    -- date
    select * from unlist('1.01.0001,31.12.9999,8/06/2315,1245-12-01,5555/12/28' returning date) as a(unlist_date_01);

    -- SQLSTATE = 22008 / value exceeds the range for valid dates:
    select * from unlist('1.01.0000' returning date) as a(unlist_date_02);

    -- SQLSTATE = 22018 / conversion error from string "1.01.10000":
    select * from unlist('1.01.10000' returning date) as a(unlist_date_03);

    -- must PASS:
    select extract( day from (select * from unlist('1.01.2201' returning date) as a(unlist_date_04) ) ) from rdb$database;

    -- decimal(n, m)
    select * from unlist('4,2.00,5.83,23.01' returning dec(4,2)) as a(unlist_decimal_01);
    select * from unlist('55555555555555555555555555555555555555' returning dec(38,0)) as a(unlist_decimal_02);

    -- SQLSTATE = HY104 / ... / -Precision must be from 1 to 38    
    select * from unlist('555555555555555555555555555555555555555555555555555555' returning dec(54,0)) as a(unlist_decimal_03);

    -- decfloat (16|34): following must pass:
    select * from unlist('4,2.00,5.83,23.00000000000001,24.000000000000011' returning decfloat(16)) as a(unlist_decfloat16_01);
    select * from unlist('4,2.00,5.83,23.00000000000001000000000000000001,24.000000000000010000000000000000011' returning decfloat(34)) as a(unlist_decfloat34_01);

    -- double precision
    -- TODO LATER: investigate why such strange low limit for least positive number, '9.9e-307':
    select * from unlist('0e0,-0e0,9.9e-307,1.7976931348623157e308,4,2.00,5.83,23.00000000000001, 24.000000000000011' returning double precision) as a(unlist_double_01);

    -- float
    select * from unlist('4,2.00,5.83,23.000001, 24.0000011' returning float) as a(unlist_float_01);

    -- int
    select * from unlist('-2147483648,2147483647' returning int) as a(unlist_int_01);

    -- SQLSTATE = 22003 / ... / -numeric value is out of range:
    select * from unlist('-2147483649' returning int) as a(unlist_int_02);

    -- SQLSTATE = 22003 / ... / -numeric value is out of range:
    select * from unlist('2147483648' returning int) as a(unlist_int_03);


    -- int128
    select * from unlist('-170141183460469231731687303715884105728, 170141183460469231731687303715884105727' returning int128) as a(unlist_int128_01);

    -- SQLSTATE = 22003 / ... / -numeric value is out of range:
    select * from unlist('-170141183460469231731687303715884105729' returning int128) as a(unlist_int128_01);

    -- SQLSTATE = 22003 / ... / -numeric value is out of range:
    select * from unlist('170141183460469231731687303715884105728' returning int128) as a(unlist_int128_01);

    -- national character (n)
    select * from unlist('qwer,asdf,zxcv' returning nchar(8)) as a(unlist_nchar_01);

    -- must raise SQLSTATE = 22001 / ... / -string right truncation / -expected length 1, actual 4:
    select * from unlist('tyui' returning nchar) as a(unlist_nchar_02);

    -- must raise SQLSTATE = 42000 / ... / -Positive value expected:
    select * from unlist('ghjk' returning nchar(0)) as a(unlist_nchar_03);

    -- varchar
    select * from unlist('text,texttext,1243' returning varchar(8)) as a(unlist_varchar_01);

    -- SQLSTATE = 22001 / -string right truncation / -expected length 8, actual 12:
    select * from unlist('texttexttext' returning varchar(8)) as a(unlist_varchar_02);

    -- -Token unknown ... -)
    select * from unlist('text' returning varchar) as a(unlist_varchar_03);

    -- Positive value expected
    select * from unlist('text' returning varchar(0)) as a(unlist_varchar_04);

    -- national character varying (n)
    select * from unlist('text,texttext,1243' returning nchar varying(8)) as a(unlist_nvarchar_01);

    -- SQLSTATE = 22001 / -string right truncation / -expected length 8, actual 12:
    select * from unlist('texttexttext' returning nchar varying(8)) as a(unlist_nvarchar_02);

    -- -Token unknown ... -)
    select * from unlist('text' returning nchar varying) as a(unlist_nvarchar_03);

    -- Positive value expected
    select * from unlist('text' returning nchar varying(0)) as a(unlist_nvarchar_04);

    -- numeric(n, m) = decimal(n, m)

    -- smallint
    select * from unlist('-32768,32767' returning smallint) as a(unlist_smallint_01);
    
    -- SQLSTATE = 22003 / ... / -numeric value is out of range:
    select * from unlist('-32769' returning smallint) as a(unlist_smallint_02);

    -- SQLSTATE = 22003 / ... / -numeric value is out of range:
    select * from unlist('32768' returning smallint) as a(unlist_smallint_03);

    -- time
    select * from unlist('00:00:00.0000,23:59:59.9999' returning time) as a(unlist_time_01);

    -- SQLSTATE = 22018 / conversion error from string "00:00:00.10000"
    select * from unlist('00:00:00.10000' returning time) as a(unlist_time_02);

    -- SQLSTATE = 22018 / conversion error from string "00"
    select * from unlist('00:00:00.10000',':' returning time) as a(unlist_time_03);

    -- SQLSTATE = 22018 / conversion error from string "10000"
    select * from unlist('00:00:00.10000','.' returning time) as a(unlist_time_04);

    -- must PASS:
    select * from unlist('23:59:59' returning time) as a(unlist_time_05);

    -- time with time zone
    -- ::: NB ::: here we do NOT specify name of time zone ==> value from 'set time zone' will be taken:
    select * from unlist('00:00:00.0000,23:59:59.9999' returning time with time zone) as a(unlist_tmtz_01);

    -- SQLSTATE = 22018 / conversion error from string "00:00:00.10000"
    select * from unlist('00:00:00.10000' returning time with time zone) as a(unlist_tmtz_02);

    -- SQLSTATE = 22018 / conversion error from string "00"
    select * from unlist('00:00:00.10000',':' returning time with time zone) as a(unlist_tmtz_03);

    -- SQLSTATE = 22018 / conversion error from string "10000"
    select * from unlist('00:00:00.10000','.' returning time with time zone) as a(unlist_tmtz_04);

    -- must PASS:
    select * from unlist('23:59:59' returning time with time zone) as a(unlist_tmtz_05);
    
    -- must PASS:
    select * from unlist('23:59:59.9999 europe/moscow,23:59:59.9999 -03:00,23:59:59.9999 gmt,23:59:59.9999 aet,23:59:59.9999 art,23:59:59.9999 etc/gmt+5,23:59:59.9999 america/kentucky/monticello' returning time with time zone) as a(unlist_tmtz_06);

    -- timestamp
    -- must PASS:
    select * from unlist('8/06/2315 00:00:00.0000,1245-12-01 23:59:59.9999' returning timestamp) as a(unlist_timestamp_01);

    -- SQLSTATE = 22018 / conversion error from string "00:00:00.0000,1245-12-01"
    select * from unlist('8/06/2315 00:00:00.0000,1245-12-01 23:59:59.9999',' ' returning timestamp) as a(unlist_timestamp_02);

    -- SQLSTATE = 22018 / conversion error from string "8/06/2315"
    select * from unlist('8/06/2315 00:00:00.0000,1245-12-01 23:59:59.9999',':' returning timestamp) as a(unlist_timestamp_03);

    -- SQLSTATE = 22018 / conversion error from string "00:00:00.0000"
    select * from unlist('00:00:00.0000,23:59:59.9999' returning timestamp) as a(unlist_timestamp_04);

    -- timestamp with time zone
    -- must PASS:
    select * from unlist('8/06/2315 00:00:00.0000,1245-12-01 23:59:59.9999' returning timestamp with time zone) as a(unlist_tstz_01);

    -- conversion error from string "00:00:00.0000,1245-12-01"
    select * from unlist('8/06/2315 00:00:00.0000,1245-12-01 23:59:59.9999',' ' returning timestamp with time zone) as a(unlist_tstz_02);

    -- conversion error from string "8/06/2315 00"
    select * from unlist('8/06/2315 00:00:00.0000,1245-12-01 23:59:59.9999',':' returning timestamp with time zone) as a(unlist_tstz_03);

    -- conversion error from string "00:00:00.0000"
    select * from unlist('00:00:00.0000,23:59:59.9999' returning timestamp with time zone) as a(unlist_tstz_04);

    -- must PASS:
    select * from unlist('8/06/2315 23:59:59.9999 europe/moscow,8/06/2315 23:59:59.9999 -03:00,8/06/2315 23:59:59.9999 gmt,8/06/2315 23:59:59.9999 aet,8/06/2315 23:59:59.9999 art,8/06/2315 23:59:59.9999 etc/gmt+5,8/06/2315 23:59:59.9999 america/kentucky/monticello' returning timestamp with time zone) as a(unlist_tstz_05);

    -- varbinary(n)
    -- must PASS:
    select * from unlist('1111,12345678' returning varbinary(8)) as a(unlist_varbin_01);
    -- must PASS:
    select * from unlist('text,texttext' returning varbinary(8)) as a(unlist_varbin_02);

    -- string right truncation / expected length 8, actual 12
    select * from unlist('texttexttext' returning varbinary(8)) as a(unlist_varbin_03);

    -- token unknown / -)
    select * from unlist('text' returning varbinary) as a(unlist_varbin_04);

    -- Positive value expected
    select * from unlist('text' returning varbinary(0)) as a(unlist_varbin_05);

    -- blob
    -- must PASS:
    select * from unlist('1111,12345678,abcdefghijklmnopqrstuvwxyz' returning blob sub_type text) as a(unlist_blob_01);
    -- must PASS:
    select * from unlist(0x13 || ',' || 0x14 returning blob sub_type binary) as a(unlist_blob_02);

    -- token unknown / -)
    select * from unlist('' returning ) as a;

    -- domain
    create domain test_domain as varchar(8);

    -- must PASS:
    select * from unlist('text,texttext,1243' returning test_domain) as a(unlist_domain_01);
    -- string right truncation / expected length 8, actual 12
    select * from unlist('texttexttext' returning test_domain) as a(unlist_domain_02);
"""

substitutions= [('[\t ]+', ' '), ('.0000000000000000', '.000000000000000')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = f"""
    UNLIST_BIGINT_01 -9223372036854775808
    UNLIST_BIGINT_01 9223372036854775807
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    UNLIST_BOOLEAN_01 <true>
    UNLIST_BOOLEAN_01 <false>
    Statement failed, SQLSTATE = 22018
    conversion error from string "right"
    UNLIST_BINARY_01 3131313100000000
    UNLIST_BINARY_01 3132333435363738
    UNLIST_BINARY_02 7465787400000000
    UNLIST_BINARY_02 7465787474657874
    UNLIST_CHAR_01 text
    UNLIST_CHAR_01 texttext
    UNLIST_CHAR_01 1243
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 1, actual 4
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -842
    -Positive value expected
    -At line 2, column 48
    UNLIST_DATE_01 0001-01-01
    UNLIST_DATE_01 9999-12-31
    UNLIST_DATE_01 2315-08-06
    UNLIST_DATE_01 1245-12-01
    UNLIST_DATE_01 5555-12-28
    Statement failed, SQLSTATE = 22008
    value exceeds the range for valid dates
    Statement failed, SQLSTATE = 22018
    conversion error from string "1.01.10000"
    EXTRACT 1
    UNLIST_DECIMAL_01 4.00
    UNLIST_DECIMAL_01 2.00
    UNLIST_DECIMAL_01 5.83
    UNLIST_DECIMAL_01 23.01
    UNLIST_DECIMAL_02 55555555555555555555555555555555555555
    Statement failed, SQLSTATE = HY104
    Dynamic SQL Error
    -SQL error code = -842
    -Precision must be from 1 to 38
    -At line 2, column 97
    UNLIST_DECFLOAT16_01 4
    UNLIST_DECFLOAT16_01 2.00
    UNLIST_DECFLOAT16_01 5.83
    UNLIST_DECFLOAT16_01 23.00000000000001
    UNLIST_DECFLOAT16_01 24.00000000000001
    UNLIST_DECFLOAT34_01 4
    UNLIST_DECFLOAT34_01 2.00
    UNLIST_DECFLOAT34_01 5.83
    UNLIST_DECFLOAT34_01 23.00000000000001000000000000000001
    UNLIST_DECFLOAT34_01 24.00000000000001000000000000000001
    UNLIST_DOUBLE_01 0.000000000000000
    UNLIST_DOUBLE_01 -0.000000000000000
    UNLIST_DOUBLE_01 9.900000000000000e-307
    UNLIST_DOUBLE_01 1.797693134862316e+308
    UNLIST_DOUBLE_01 4.000000000000000
    UNLIST_DOUBLE_01 2.000000000000000
    UNLIST_DOUBLE_01 5.830000000000000
    UNLIST_DOUBLE_01 23.00000000000001
    UNLIST_DOUBLE_01 24.00000000000001
    UNLIST_FLOAT_01 4
    UNLIST_FLOAT_01 2
    UNLIST_FLOAT_01 5.8299999
    UNLIST_FLOAT_01 23.000002
    UNLIST_FLOAT_01 24.000002
    UNLIST_INT_01 -2147483648
    UNLIST_INT_01 2147483647
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    UNLIST_INT128_01 -170141183460469231731687303715884105728
    UNLIST_INT128_01 170141183460469231731687303715884105727
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    UNLIST_NCHAR_01 qwer
    UNLIST_NCHAR_01 asdf
    UNLIST_NCHAR_01 zxcv
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 1, actual 4
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -842
    -Positive value expected
    -At line 2, column 49
    UNLIST_VARCHAR_01 text
    UNLIST_VARCHAR_01 texttext
    UNLIST_VARCHAR_01 1243
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 8, actual 12
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 2, column 50
    -)
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -842
    -Positive value expected
    -At line 2, column 51
    UNLIST_NVARCHAR_01 text
    UNLIST_NVARCHAR_01 texttext
    UNLIST_NVARCHAR_01 1243
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 8, actual 12
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 2, column 56
    -)
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -842
    -Positive value expected
    -At line 2, column 57
    UNLIST_SMALLINT_01 -32768
    UNLIST_SMALLINT_01 32767
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    UNLIST_TIME_01 00:00:00.0000
    UNLIST_TIME_01 23:59:59.9999
    Statement failed, SQLSTATE = 22018
    conversion error from string "00:00:00.10000"
    Statement failed, SQLSTATE = 22018
    conversion error from string "00"
    Statement failed, SQLSTATE = 22018
    conversion error from string "10000"
    UNLIST_TIME_05 23:59:59.0000
    UNLIST_TMTZ_01 00:00:00.0000 {SELECTED_TIMEZONE}
    UNLIST_TMTZ_01 23:59:59.9999 {SELECTED_TIMEZONE}
    Statement failed, SQLSTATE = 22018
    conversion error from string "00:00:00.10000"
    Statement failed, SQLSTATE = 22018
    conversion error from string "00"
    Statement failed, SQLSTATE = 22018
    conversion error from string "10000"
    UNLIST_TMTZ_05 23:59:59.0000 {SELECTED_TIMEZONE}
    UNLIST_TMTZ_06 23:59:59.9999 Europe/Moscow
    UNLIST_TMTZ_06 23:59:59.9999 -03:00
    UNLIST_TMTZ_06 23:59:59.9999 GMT
    UNLIST_TMTZ_06 23:59:59.9999 AET
    UNLIST_TMTZ_06 23:59:59.9999 ART
    UNLIST_TMTZ_06 23:59:59.9999 Etc/GMT+5
    UNLIST_TMTZ_06 23:59:59.9999 America/Kentucky/Monticello
    UNLIST_TIMESTAMP_01 2315-08-06 00:00:00.0000
    UNLIST_TIMESTAMP_01 1245-12-01 23:59:59.9999
    Statement failed, SQLSTATE = 22018
    conversion error from string "00:00:00.0000,1245-12-01"
    Statement failed, SQLSTATE = 22018
    conversion error from string "8/06/2315 00"
    Statement failed, SQLSTATE = 22018
    conversion error from string "00:00:00.0000"
    UNLIST_TSTZ_01 2315-08-06 00:00:00.0000 {SELECTED_TIMEZONE}
    UNLIST_TSTZ_01 1245-12-01 23:59:59.9999 {SELECTED_TIMEZONE}
    Statement failed, SQLSTATE = 22018
    conversion error from string "00:00:00.0000,1245-12-01"
    Statement failed, SQLSTATE = 22018
    conversion error from string "8/06/2315 00"
    Statement failed, SQLSTATE = 22018
    conversion error from string "00:00:00.0000"
    UNLIST_TSTZ_05 2315-08-06 23:59:59.9999 Europe/Moscow
    UNLIST_TSTZ_05 2315-08-06 23:59:59.9999 -03:00
    UNLIST_TSTZ_05 2315-08-06 23:59:59.9999 GMT
    UNLIST_TSTZ_05 2315-08-06 23:59:59.9999 AET
    UNLIST_TSTZ_05 2315-08-06 23:59:59.9999 ART
    UNLIST_TSTZ_05 2315-08-06 23:59:59.9999 Etc/GMT+5
    UNLIST_TSTZ_05 2315-08-06 23:59:59.9999 America/Kentucky/Monticello
    UNLIST_VARBIN_01 31313131
    UNLIST_VARBIN_01 3132333435363738
    UNLIST_VARBIN_02 74657874
    UNLIST_VARBIN_02 7465787474657874
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 8, actual 12
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 2, column 52
    -)
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -842
    -Positive value expected
    -At line 2, column 53
    UNLIST_BLOB_01 0:1
    1111
    UNLIST_BLOB_01 0:2
    12345678
    UNLIST_BLOB_01 0:3
    abcdefghijklmnopqrstuvwxyz
    UNLIST_BLOB_02 0:7
    19
    UNLIST_BLOB_02 0:8
    20
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 2, column 39
    -)
    UNLIST_DOMAIN_01 text
    UNLIST_DOMAIN_01 texttext
    UNLIST_DOMAIN_01 1243
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 8, actual 12
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
