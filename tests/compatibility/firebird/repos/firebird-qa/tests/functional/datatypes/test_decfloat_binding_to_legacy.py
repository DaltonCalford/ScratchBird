#coding:utf-8

"""
ID:          decfloat.binding-to-legacy
TITLE:       Test ability for DECFLOAT values to be represented as other data types using LEGACY keyword.
DESCRIPTION:
  We check here that values from DECFLOAT will be actually converted to legacy datatypes
  according to following table from sql.extensions\\README.set_bind.md:
    ----------------------------------------------------------
    | Native datatype          | Legacy datatype             |
    |--------------------------|-----------------------------|
    | BOOLEAN                  | CHAR(5)                     |
    | DECFLOAT                 | DOUBLE PRECISION            |
    | NUMERIC(38)              | NUMERIC(18)                 |
    | TIME WITH TIME ZONE      | TIME WITHOUT TIME ZONE      |
    | TIMESTAMP WITH TIME ZONE | TIMESTAMP WITHOUT TIME ZONE |
    ----------------------------------------------------------
   SQLDA must contain the same datatypes when we use either explicit rule or LEGACY keyword.
 NOTES:
 [11.03.2020]
   WARNING
   Test verifies binding of TIME WITH TIMEZONE data and uses America/Los_Angeles timezone.
   But there is daylight saving time in the USA, they change clock at the begining of March.

   For this reason query like: "select time '10:00 America/Los_Angeles' from ..." will return
   different values depending on current date. For example, if we are in Moscow timezone then
   returned value will be either 20:00 in February or 21:00 in March.
   Result for other timezone (e.g. Tokyo) will be differ, etc.
   For this reason, special replacement will be done in 'substitution' section: we replace
   value of hours with '??' because it is no matter what's the time there, we have to ensure
   only the ability to work with such time using SET BIND clause.
FBTEST:      functional.datatypes.decfloat_binding_to_legacy
"""

import pytest
from firebird.qa import *

substitutions = [(' \\d{2}:00:00.0000', ' ??:00:00.0000'), ('charset.*', ''),
                 ('.*alias:.*', ''), ('^((?!(sqltype|check_bind_)).)*$', ''), ('[ \t]+', ' ')]

db = db_factory()

test_script = """
    set list on;
    --set bail on;
    --set echo on;
    set sqlda_display on;

    -- Legacy, explicit and implicit:
    set bind of boolean to char(5);
    select not false and true as "check_bind_bool_to_char" from rdb$database;

    set bind of boolean to legacy;
    select not false and true as "check_bind_bool_to_legacy" from rdb$database;


    set bind of decfloat to double precision;
    select 3.141592653589793238462643383279502884197169399375105820974944592307816406286 as "check_bind_decfloat_to_double" from rdb$database;

    set bind of decfloat to legacy;
    select 3.141592653589793238462643383279502884197169399375105820974944592307816406286 as "check_bind_decfloat_to_legacy" from rdb$database;


    set bind of numeric(38) to numeric(18); -- this is mentioned in http://tracker.firebirdsql.org/browse/CORE-6057
    select 3.141592653589793238462643383279502884197169399375105820974944592307816406286 as "check_bind_n38_to_n18" from rdb$database;

    set bind of numeric(38) to legacy;
    select 3.141592653589793238462643383279502884197169399375105820974944592307816406286 as "check_bind_n38_to_legacy" from rdb$database;


    set bind of time with time zone to time without time zone;
    select time '10:00 America/Los_Angeles' as "check_bind_time_with_zone_to_time" from rdb$database;

    set bind of time with time zone to legacy;
    select time '10:00 America/Los_Angeles' as "check_bind_time_with_zone_to_legacy" from rdb$database;


    set bind of timestamp with time zone to timestamp without time zone;
    select timestamp '2018-01-01 12:00 GMT' as "check_bind_timestamp_with_zone_to_timestamp" from rdb$database;

    set bind of timestamp with time zone to legacy;
    select timestamp '2018-01-01 12:00 GMT' as "check_bind_timestamp_with_zone_to_legacy" from rdb$database;


"""

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """
    01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 5
    check_bind_bool_to_char TRUE

    01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 5
    check_bind_bool_to_legacy TRUE

    01: sqltype: 480 DOUBLE scale: 0 subtype: 0 len: 8
    check_bind_decfloat_to_double 3.141592653589793

    01: sqltype: 480 DOUBLE scale: 0 subtype: 0 len: 8
    check_bind_decfloat_to_legacy 3.141592653589793

    01: sqltype: 480 DOUBLE scale: 0 subtype: 0 len: 8
    check_bind_n38_to_n18 3.141592653589793

    01: sqltype: 480 DOUBLE scale: 0 subtype: 0 len: 8
    check_bind_n38_to_legacy 3.141592653589793

    01: sqltype: 560 TIME scale: 0 subtype: 0 len: 4
    check_bind_time_with_zone_to_time ??:00:00.0000

    01: sqltype: 560 TIME scale: 0 subtype: 0 len: 4
    check_bind_time_with_zone_to_legacy ??:00:00.0000

    01: sqltype: 510 TIMESTAMP scale: 0 subtype: 0 len: 8
    check_bind_timestamp_with_zone_to_timestamp 2018-01-01 ??:00:00.0000

    01: sqltype: 510 TIMESTAMP scale: 0 subtype: 0 len: 8
    check_bind_timestamp_with_zone_to_legacy 2018-01-01 ??:00:00.0000
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
