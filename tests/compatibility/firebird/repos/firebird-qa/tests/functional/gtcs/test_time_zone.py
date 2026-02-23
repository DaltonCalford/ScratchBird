#coding:utf-8

"""
ID:          gtcs.time-zone
TITLE:       Miscelaneous time zone tests
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_TIME_ZONE.script
    Checked on 4.0.0.1931.
NOTES:
    [05.05.2020]
        added block for CORE-6271 (from GTCS). Checked on 4.0.0.1954.
    [28.10.2020]
        Old code was completely replaced by source from GTCS.
        It was changed to meet new requirements to format of timezone offset:
        1) it must include SIGN, i.e. + or -;
        2) in must contain BOTH hours and minutes delimited by colon.

        This means that following (old) statements will fail with SQLSTATE = 22009:
        * set time zone '00:00'
          Invalid time zone region: 00:00
          (because of missed sign "+")

        *  ... datediff(hour from timestamp '... -03' to timestamp '... -03')
          Invalid time zone offset: -03 - must use format +/-hours:minutes and be between -14:00 and +14:00

        See: https://github.com/FirebirdSQL/firebird/commit/ff37d445ce844f991242b1e2c1f96b80a5d1636d
    [09.02.2022] pcisar
        Fails on Windows due to differences like:
          - CAST 10:11:12.1345 America/Sao_Paulo
          ?       ^
          + CAST 11:11:12.1345 America/Sao_Paulo
          ?       ^

    [08.04.2022] pzotov
        CAN NOT reproduce fail, used dates before and after daylight saving threshold. Sent letter to pcisar, 03-apr-2022 14:28.
        Checked on FB 4.0.1 Release, 5.0.0.467.

    [05.12.2025] pzotov
        Separated expected ooutput for major versions before/since 6.x because boolean expressions
        are titled as 'BOOL'since #8820 was fixed.
        Checked on 6.0.0.1364, 5.0.4.1737, 4.0.7.3237
"""

import pytest
import platform
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    set time zone '+00:00';

    select cast('01:23:45' as time with time zone),
           cast('2018-01-01 01:23:45' as timestamp with time zone),
           extract(timezone_hour from cast('01:23:45' as time with time zone)),
           extract(timezone_minute from cast('01:23:45' as time with time zone)),
           extract(timezone_hour from cast('2018-01-01 01:23:45' as timestamp with time zone)),
           extract(timezone_minute from cast('2018-01-01 01:23:45' as timestamp with time zone)),
           cast(cast('01:23:45' as time with time zone) as time),
           cast(cast('2018-01-01 01:23:45' as timestamp with time zone) as timestamp)
      from rdb$database;

    select cast('01:23:45 +02:00' as time with time zone),
           cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone),
           extract(timezone_hour from cast('01:23:45 +02:00' as time with time zone)),
           extract(timezone_minute from cast('01:23:45 +02:00' as time with time zone)),
           extract(timezone_hour from cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone)),
           extract(timezone_minute from cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone)),
           cast(cast('01:23:45 +02:00' as time with time zone) as time),
           cast(cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone) as timestamp)
      from rdb$database;

    select time '01:23:45 +02:00',
           timestamp '2018-01-01 01:23:45 +02:00',
           extract(timezone_hour from time '01:23:45 +02:00'),
           extract(timezone_minute from time '01:23:45 +02:00'),
           extract(timezone_hour from timestamp '2018-01-01 01:23:45 +02:00'),
           extract(timezone_minute from timestamp '2018-01-01 01:23:45 +02:00'),
           cast(time '01:23:45 +02:00' as time),
           cast(timestamp '2018-01-01 01:23:45 +02:00' as timestamp)
      from rdb$database;


    ---


    set time zone '-02:00';

    select cast('01:23:45' as time with time zone),
           cast('2018-01-01 01:23:45' as timestamp with time zone),
           extract(timezone_hour from cast('01:23:45' as time with time zone)),
           extract(timezone_minute from cast('01:23:45' as time with time zone)),
           extract(timezone_hour from cast('2018-01-01 01:23:45' as timestamp with time zone)),
           extract(timezone_minute from cast('2018-01-01 01:23:45' as timestamp with time zone)),
           cast(cast('01:23:45' as time with time zone) as time),
           cast(cast('2018-01-01 01:23:45' as timestamp with time zone) as timestamp)
      from rdb$database;

    select cast('01:23:45 +02:00' as time with time zone),
           cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone),
           extract(timezone_hour from cast('01:23:45 +02:00' as time with time zone)),
           extract(timezone_minute from cast('01:23:45 +02:00' as time with time zone)),
           extract(timezone_hour from cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone)),
           extract(timezone_minute from cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone)),
           cast(cast('01:23:45 +02:00' as time with time zone) as time),
           cast(cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone) as timestamp)
      from rdb$database;


    ---


    select extract(hour from timestamp '2018-01-02 03:04:05.6789 -03:00'),
           extract(minute from timestamp '2018-01-02 03:04:05.6789 -03:00'),
           extract(second from timestamp '2018-01-02 03:04:05.6789 -03:00'),
           extract(millisecond from timestamp '2018-01-02 03:04:05.6789 -03:00'),
           extract(year from timestamp '2018-01-02 03:04:05.6789 -03:00'),
           extract(month from timestamp '2018-01-02 03:04:05.6789 -03:00'),
           extract(day from timestamp '2018-01-02 03:04:05.6789 -03:00')
      from rdb$database;

    select extract(hour from time '03:04:05.6789 -03:00'),
           extract(minute from time '03:04:05.6789 -03:00'),
           extract(second from time '03:04:05.6789 -03:00'),
           extract(millisecond from time '03:04:05.6789 -03:00')
      from rdb$database;


    ---


    -- DST starts at 2017-10-15 00:00 in America/Sao_Paulo

    select timestamp '2017-10-14 22:00 America/Sao_Paulo', extract(timezone_hour from timestamp '2017-10-14 22:00 America/Sao_Paulo'),
           timestamp '2017-10-14 22:00 America/Sao_Paulo' + 1, extract(timezone_hour from timestamp '2017-10-14 22:00 America/Sao_Paulo' + 1),
           timestamp '2017-10-14 22:00 America/Sao_Paulo' + 2, extract(timezone_hour from timestamp '2017-10-14 22:00 America/Sao_Paulo' + 2),
           timestamp '2017-10-14 22:00 America/Sao_Paulo' + 3, extract(timezone_hour from timestamp '2017-10-14 22:00 America/Sao_Paulo' + 3),
           timestamp '2017-10-16 22:00 America/Sao_Paulo', extract(timezone_hour from timestamp '2017-10-16 22:00 America/Sao_Paulo'),
           timestamp '2017-10-16 22:00 America/Sao_Paulo' - 1, extract(timezone_hour from timestamp '2017-10-16 22:00 America/Sao_Paulo' - 1),
           timestamp '2017-10-16 22:00 America/Sao_Paulo' - 2, extract(timezone_hour from timestamp '2017-10-16 22:00 America/Sao_Paulo' - 2),
           timestamp '2017-10-16 22:00 America/Sao_Paulo' - 3, extract(timezone_hour from timestamp '2017-10-16 22:00 America/Sao_Paulo' - 3)
      from rdb$database;

    select dateadd(1 hour to timestamp '2017-10-14 20:00 America/Sao_Paulo'),
           dateadd(2 hour to timestamp '2017-10-14 20:00 America/Sao_Paulo'),
           dateadd(3 hour to timestamp '2017-10-14 20:00 America/Sao_Paulo'),
           dateadd(4 hour to timestamp '2017-10-14 20:00 America/Sao_Paulo'),
           dateadd(5 hour to timestamp '2017-10-14 20:00 America/Sao_Paulo')
      from rdb$database;


    -- DST ends at 2018-02-18 00:00 in America/Sao_Paulo

    select timestamp '2018-02-17 22:00 America/Sao_Paulo', extract(timezone_hour from timestamp '2018-02-17 22:00 America/Sao_Paulo'),
           timestamp '2018-02-17 22:00 America/Sao_Paulo' + 1, extract(timezone_hour from timestamp '2018-02-17 22:00 America/Sao_Paulo' + 1),
           timestamp '2018-02-17 22:00 America/Sao_Paulo' + 2, extract(timezone_hour from timestamp '2018-02-17 22:00 America/Sao_Paulo' + 2),
           timestamp '2018-02-17 22:00 America/Sao_Paulo' + 3, extract(timezone_hour from timestamp '2018-02-17 22:00 America/Sao_Paulo' + 3),
           timestamp '2018-02-19 22:00 America/Sao_Paulo', extract(timezone_hour from timestamp '2018-02-19 22:00 America/Sao_Paulo'),
           timestamp '2018-02-19 22:00 America/Sao_Paulo' - 1, extract(timezone_hour from timestamp '2018-02-19 22:00 America/Sao_Paulo' - 1),
           timestamp '2018-02-19 22:00 America/Sao_Paulo' - 2, extract(timezone_hour from timestamp '2018-02-19 22:00 America/Sao_Paulo' - 2),
           timestamp '2018-02-19 22:00 America/Sao_Paulo' - 3, extract(timezone_hour from timestamp '2018-02-19 22:00 America/Sao_Paulo' - 3)
      from rdb$database;

    select dateadd(1 hour to timestamp '2018-02-17 22:00 America/Sao_Paulo'),
           dateadd(2 hour to timestamp '2018-02-17 22:00 America/Sao_Paulo'),
           dateadd(3 hour to timestamp '2018-02-17 22:00 America/Sao_Paulo'),
           dateadd(4 hour to timestamp '2018-02-17 22:00 America/Sao_Paulo')
      from rdb$database;

    select dateadd(-1 hour to timestamp '2018-02-18 01:00 America/Sao_Paulo'),
           dateadd(-2 hour to timestamp '2018-02-18 01:00 America/Sao_Paulo'),
           dateadd(-3 hour to timestamp '2018-02-18 01:00 America/Sao_Paulo'),
           dateadd(-4 hour to timestamp '2018-02-18 01:00 America/Sao_Paulo')
      from rdb$database;


    ---

    /*
      28.10.2020:
      ... datediff(hour from timestamp '... -03' to timestamp '... -03')
      Statement failed, SQLSTATE = 22009
      Invalid time zone offset: -03 - must use format +/-hours:minutes and be between -14:00 and +14:00
    */

    select datediff(hour from timestamp '2018-04-01 10:00:00' to timestamp '2018-04-01 10:00:00 -03:00'),
           datediff(hour from timestamp '2018-04-01 10:00:00 -03:0' to timestamp '2018-04-01 10:00:00 -03:0'),
           datediff(hour from timestamp '2018-04-01 10:00:00' to timestamp '2018-04-01 10:00:00 -2:0')
      from rdb$database;


    ---


    set time zone '-02:20';

    select cast('01:23:45' as time with time zone),
           cast('2018-01-01 01:23:45' as timestamp with time zone),
           extract(timezone_hour from cast('01:23:45' as time with time zone)),
           extract(timezone_minute from cast('01:23:45' as time with time zone)),
           extract(timezone_hour from cast('2018-01-01 01:23:45' as timestamp with time zone)),
           extract(timezone_minute from cast('2018-01-01 01:23:45' as timestamp with time zone)),
           cast(cast('01:23:45' as time with time zone) as time),
           cast(cast('2018-01-01 01:23:45' as timestamp with time zone) as timestamp)
      from rdb$database;

    select cast('01:23:45 +02:00' as time with time zone),
           cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone),
           extract(timezone_hour from cast('01:23:45 +02:00' as time with time zone)),
           extract(timezone_minute from cast('01:23:45 +02:00' as time with time zone)),
           extract(timezone_hour from cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone)),
           extract(timezone_minute from cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone)),
           cast(cast('01:23:45 +02:00' as time with time zone) as time),
           cast(cast('2018-01-01 01:23:45 +02:00' as timestamp with time zone) as timestamp)
      from rdb$database;

    select extract(timezone_hour from time '03:04:05.6789 -03:00'),
           extract(timezone_minute from time '03:04:05.6789 -03:00'),
           extract(timezone_hour from timestamp '2018-01-01 03:04:05.6789 -03:00'),
           extract(timezone_minute from timestamp '2018-01-01 03:04:05.6789 -03:00')
      from rdb$database;


    ---


    select extract(timezone_hour from cast('now' as timestamp with time zone)) = -2,
           extract(timezone_minute from cast('now' as timestamp with time zone)) = -20
      from rdb$database;


    ---


    set time zone '-02:00';

    select cast('01:23:45 -1:0' as time),
           cast('01:23:45 +1:0' as time),
           cast('2018-01-01 01:23:45 -01:00' as timestamp),
           cast('2018-01-01 01:23:45 +01:00' as timestamp)
      from rdb$database;

    select cast(cast('01:23:45' as time with time zone) as timestamp with time zone) = cast(current_date || ' 01:23:45' as timestamp with time zone)
      from rdb$database;

    select cast(cast('2018-01-01 01:23:45' as timestamp with time zone) as time with time zone)
      from rdb$database;

    select cast(cast('2018-01-01 01:23:45 -04:00' as timestamp with time zone) as time with time zone)
      from rdb$database;

    select cast(cast('2018-01-01 01:23:45' as timestamp with time zone) as date)
      from rdb$database;

    select cast(cast('2018-01-01 01:23:45 -04:00' as timestamp with time zone) as date)
      from rdb$database;

    select cast(cast('2018-01-01 01:23:45' as timestamp with time zone) as time)
      from rdb$database;

    select cast(cast('2018-01-01 01:23:45 -04:00' as timestamp with time zone) as time)
      from rdb$database;

    select cast(cast('2018-01-01 01:23:45' as timestamp with time zone) as timestamp)
      from rdb$database;

    select cast(cast('2018-01-01 01:23:45 -04:00' as timestamp with time zone) as timestamp)
      from rdb$database;

    select cast(timestamp '2018-01-01 01:23:45' as timestamp with time zone)
      from rdb$database;

    select cast(timestamp '2018-01-01 01:23:45' as time with time zone)
      from rdb$database;

    select cast(cast('01:23:45' as time) as timestamp with time zone) = cast(cast('01:23:45' as time with time zone) as timestamp with time zone)
      from rdb$database;

    select cast(cast('01:23:45' as time) as time with time zone) = '01:23:45 -02:00'
      from rdb$database;

    select cast(cast(cast('01:23:45 -03:00' as time with time zone) as timestamp) as time) = '02:23:45'
      from rdb$database;

    select cast(cast('01:23:45 -03:00' as time with time zone) as time) = '02:23:45'
      from rdb$database;

    -- Error
    select cast(cast('01:23:45 -03:00' as time with time zone) as date)
      from rdb$database;

    select cast(date '2018-01-01' as timestamp with time zone)
      from rdb$database;

    -- Error
    select cast(date '2018-01-01' as time with time zone)
      from rdb$database;


    ---


    select timestamp '2018-02-03 America/Sao_Paulo'
      from rdb$database;


    ---


    select cast('23:23:34 +05:00' as time with time zone) + 1,
           cast('23:23:34 +05:00' as time with time zone) - 1
      from rdb$database;

    select cast('2018-01-01 23:23:34 +05:00' as timestamp with time zone) + 1,
           cast('2018-01-01 23:23:34 +05:00' as timestamp with time zone) - 1
      from rdb$database;

    select date '2018-01-01' + cast('23:23:34 +05:00' as time with time zone),
           cast('23:23:34 +05:00' as time with time zone) + date '2018-01-01',
           date '2018-01-01' + cast('23:23:34 +05:00' as time),
           cast('23:23:34 +05:00' as time) + date '2018-01-01'
      from rdb$database;

    select cast('12:00:00 +01:00' as time with time zone) - cast('12:00:00 +00:00' as time with time zone),
           cast('23:00:00 -01:00' as time with time zone) - cast('23:00:00 +00:00' as time with time zone),
           time '12:00:00' - cast('12:00:00 +00:00' as time with time zone),
           cast('12:00:00 +00:00' as time with time zone) - time '12:00:00'
      from rdb$database;

    select cast('2018-01-01 12:00:00 +01:00' as timestamp with time zone) - cast('2018-01-01 12:00:00 +00:00' as timestamp with time zone),
           cast('2018-01-01 23:00:00 -01:00' as timestamp with time zone) - cast('2018-01-01 23:00:00 +00:00' as timestamp with time zone),
           timestamp '2018-01-01 12:00:00' - cast('2018-01-01 12:00:00 +00:00' as timestamp with time zone),
           cast('2018-01-01 12:00:00 +00:00' as timestamp with time zone) - timestamp '2018-01-01 12:00:00'
      from rdb$database;


    ---


    select cast('10:00:00 +05:00' as time with time zone) = cast('10:00:00 +05:00' as time with time zone),
           cast('10:00:00 +05:00' as time with time zone) = cast('11:00:00 +06:00' as time with time zone),
           cast('10:00:00 +12:00' as time with time zone) = cast('10:00:00 -12:00' as time with time zone),
           cast('10:00:00 -02:00' as time with time zone) = cast('10:00:00' as time with time zone),
           cast('10:00:00 -02:00' as time with time zone) = cast('10:00:00' as time),
           cast('10:00:00' as time) = cast('10:00:00 -02:00' as time with time zone),
           cast('10:00:00 +05:00' as time with time zone) = cast('10:00:00 +06:00' as time with time zone),
           cast('10:00:00 +05:00' as time with time zone) < cast('10:00:00 +06:00' as time with time zone),
           cast('10:00:00 +05:00' as time with time zone) > cast('10:00:00 +06:00' as time with time zone),
           localtime = current_time at time zone '-02:00'
      from rdb$database;

    select cast('2018-01-01 10:00:00 +05:00' as timestamp with time zone) = cast('2018-01-01 10:00:00 +05:00' as timestamp with time zone),
           cast('2018-01-01 10:00:00 +05:00' as timestamp with time zone) = cast('2018-01-01 11:00:00 +06:00' as timestamp with time zone),
           cast('2018-01-01 10:00:00 +12:00' as timestamp with time zone) = cast('2018-01-01 10:00:00 -12:00' as timestamp with time zone),
           cast('2018-01-01 10:00:00 -02:00' as timestamp with time zone) = cast('2018-01-01 10:00:00' as timestamp with time zone),
           cast('2018-01-01 10:00:00 -02:00' as timestamp with time zone) = cast('2018-01-01 10:00:00' as timestamp),
           cast('2018-01-01 10:00:00' as timestamp) = cast('2018-01-01 10:00:00 -02:00' as timestamp with time zone),
           cast('2018-01-01 10:00:00 +05:00' as timestamp with time zone) = cast('2018-01-01 10:00:00 +06:00' as timestamp with time zone),
           cast('2018-01-01 10:00:00 +05:00' as timestamp with time zone) < cast('2018-01-01 10:00:00 +06:00' as timestamp with time zone),
           cast('2018-01-01 10:00:00 +05:00' as timestamp with time zone) > cast('2018-01-01 10:00:00 +06:00' as timestamp with time zone),
           localtimestamp = current_timestamp at time zone '-02:00'
      from rdb$database;


    ---


    set time zone '-03:00';


    select cast(time '10:11:12.1345' as time),
           cast(time '10:11:12.1345' as time with time zone),
           substring(cast(time '10:11:12.1345' as timestamp) from 12),
           substring(cast(time '10:11:12.1345' as timestamp with time zone) from 12) from rdb$database;

    select cast(timestamp '2020-05-20 10:11:12.1345' as time),
           cast(timestamp '2020-05-20 10:11:12.1345' as time with time zone),
           cast(timestamp '2020-05-20 10:11:12.1345' as timestamp),
           cast(timestamp '2020-05-20 10:11:12.1345' as timestamp with time zone) from rdb$database;

    select cast(time '10:11:12.1345 america/sao_paulo' as time),
           cast(time '10:11:12.1345 america/sao_paulo' as time with time zone),
           substring(cast(time '10:11:12.1345 america/sao_paulo' as timestamp) from 12),
           substring(cast(time '10:11:12.1345 america/sao_paulo' as timestamp with time zone) from 12) from rdb$database;

    select cast(timestamp '2020-05-20 10:11:12.1345 america/sao_paulo' as time),
           cast(timestamp '2020-05-20 10:11:12.1345 america/sao_paulo' as time with time zone),
           cast(timestamp '2020-05-20 10:11:12.1345 america/sao_paulo' as timestamp),
           cast(timestamp '2020-05-20 10:11:12.1345 america/sao_paulo' as timestamp with time zone) from rdb$database;


    ---


    set time zone '-02:00';


    select time '20:01:02 -05:00' at time zone '-05:00',
           time '20:01:02 -05:00' at time zone '-02:00',
           time '20:01:02 -05:00' at time zone '+03:00',
           time '20:01:02' at time zone '-05:00',
           time '20:01:02' at time zone '-02:00',
           time '20:01:02' at time zone '+03:00',
           time '20:01:02 -05:00' at local,
           time '20:01:02' at local
      from rdb$database;

    select timestamp '2018-01-01 20:01:02 -05:00' at time zone '-05:00',
           timestamp '2018-01-01 20:01:02 -05:00' at time zone '-02:00',
           timestamp '2018-01-01 20:01:02 -05:00' at time zone '+03:00',
           timestamp '2018-01-01 20:01:02' at time zone '-05:00',
           timestamp '2018-01-01 20:01:02' at time zone '-02:00',
           timestamp '2018-01-01 20:01:02' at time zone '+03:00',
           timestamp '2018-01-01 20:01:02 -05:00' at local,
           timestamp '2018-01-01 20:01:02' at local
      from rdb$database;


    ---


    select timestamp '2018-05-01 20:01:02 America/Sao_Paulo' at time zone 'America/Los_Angeles',
           timestamp '2018-04-01 20:01:02 America/Sao_Paulo' at time zone 'America/Los_Angeles',
           timestamp '2018-03-01 20:01:02 America/Sao_Paulo' at time zone 'America/Los_Angeles',
           timestamp '2018-02-01 20:01:02 America/Sao_Paulo' at time zone 'America/Los_Angeles',
           timestamp '2018-01-01 20:01:02 America/Sao_Paulo' at time zone 'America/Los_Angeles',
           timestamp '2018-01-01 20:01:02 America/Sao_Paulo' at time zone 'America/Los_Angeles' + 1,
           1 + timestamp '2018-01-01 20:01:02 America/Sao_Paulo' at time zone 'America/Los_Angeles',
           1 + timestamp '2018-01-01 20:01:02 America/Sao_Paulo' at time zone 'America/Los_Angeles' + 1
      from rdb$database;


    ---


    select first_day(of year from timestamp '2018-03-08 10:11:12 America/Sao_Paulo'),
           first_day(of month from timestamp '2018-03-08 10:11:12 America/Sao_Paulo'),
           first_day(of week from timestamp '2018-03-08 10:11:12 America/Sao_Paulo'),
           last_day(of year from timestamp '2018-03-08 10:11:12 America/Sao_Paulo'),
           last_day(of month from timestamp '2018-03-08 10:11:12 America/Sao_Paulo'),
           last_day(of week from timestamp '2018-03-08 10:11:12 America/Sao_Paulo')
      from rdb$database;


    ---


    select timestamp '2017-03-12 02:30 America/New_York' t1,
           timestamp '2017-03-12 02:30 America/New_York' at time zone '-05:00' t2,
           dateadd(-1 minute to timestamp '2017-03-12 02:30 America/New_York') t3,
           dateadd(1 minute to timestamp '2017-03-12 02:30 America/New_York') t4,
           dateadd(-1 hour to timestamp '2017-03-12 02:30 America/New_York') t5,
           dateadd(1 hour to timestamp '2017-03-12 02:30 America/New_York') t6
      from rdb$database;

    select timestamp '2017-11-05 01:30 America/New_York' t1,
           timestamp '2017-11-05 01:30 America/New_York' at time zone '-04:00' t2,
           dateadd(-1 minute to timestamp '2017-11-05 01:30 America/New_York') t3,
           dateadd(1 minute to timestamp '2017-11-05 01:30 America/New_York') t4,
           dateadd(-1 hour to timestamp '2017-11-05 01:30 America/New_York') t5,
           dateadd(1 hour to timestamp '2017-11-05 01:30 America/New_York') t6
      from rdb$database;


    ---


    set bind of timestamp with time zone to legacy;
    set bind of time with time zone to legacy;
    set sqlda_display on;

    select timestamp '2018-05-01 20:01:02 America/Sao_Paulo',
           dateadd(extract(timezone_hour from time '20:01:02 America/Sao_Paulo') + 3 hour to time '20:01:02 America/Sao_Paulo')
      from rdb$database;

    /*
    disabled; strange dependency of error message on previous statement; sent letter to dimitr etc, 28.10.2020 09:10
    select 1
      from rdb$database
      where current_timestamp = ? and
            current_time = ?;
    */

    set sqlda_display off;
    set bind of timestamp with time zone to native;
    set bind of time with time zone to native;



    ---


    create table timetz (n integer, v time with time zone);
    create unique index timetz_uk on timetz (v);

    insert into timetz values (0, '11:33:33 America/Sao_Paulo');
    insert into timetz values (1, '11:33:33.456');
    insert into timetz values (2, '11:33:33.456 -1:0');
    insert into timetz values (3, '11:33:33.456 +1:0');
    insert into timetz values (4, '11:33:33');
    insert into timetz values (5, '11:33:33 -1:0');
    insert into timetz values (6, '11:33:33 +1:0');

    -- Duplicate in UTC.
    insert into timetz values (7, '12:33:33 +0:0');
    insert into timetz values (8, '13:33:33 +1:0');
    insert into timetz values (9, '14:33:33 +2:0');
    insert into timetz values (10, '11:33:33 -03:00');

    select n, v, cast(v as time) from timetz order by v, n;

    select n, v from timetz order by v + 0, n;

    commit;
    drop index timetz_uk;
    create index timetz_idx on timetz (v);

    -- Duplicate in UTC.
    insert into timetz values (7, '12:33:33 +0:0');
    insert into timetz values (8, '13:33:33 +1:0');
    insert into timetz values (9, '14:33:33 +2:0');

    select n, v, cast(v as time) from timetz order by cast(v as time), n;
    select n, v, cast(v as time) from timetz order by v, n;

    select n, v from timetz order by v + 0, n;

    commit;
    drop index timetz_idx;

    select n, v, cast(v as time) from timetz order by v, n;

    delete from timetz;
    insert into timetz values (1, '11:33:33.456');


    ---


    create table timestamptz (n integer, v timestamp with time zone);
    create unique index timestamptz_uk on timestamptz (v);

    insert into timestamptz values (1, '2018-01-01 11:33:33.456');
    insert into timestamptz values (2, '2018-01-01 11:33:33.456 -1:0');
    insert into timestamptz values (3, '2018-01-01 11:33:33.456 +1:0');
    insert into timestamptz values (4, '2018-01-01 11:33:33');
    insert into timestamptz values (5, '2018-01-01 11:33:33 -1:0');
    insert into timestamptz values (6, '2018-01-01 11:33:33 +1:0');

    -- Duplicate in UTC.
    insert into timestamptz values (7, '2018-01-01 12:33:33 +0:0');
    insert into timestamptz values (8, '2018-01-01 13:33:33 +1:0');
    insert into timestamptz values (9, '2018-01-01 14:33:33 +2:0');

    select n, v, cast(v as timestamp) from timestamptz order by v, n;

    select n, v from timestamptz order by v + 0, n;

    commit;
    drop index timestamptz_uk;
    create index timestamptz_idx on timestamptz (v);

    -- Duplicate in UTC.
    insert into timestamptz values (7, '2018-01-01 12:33:33 +0:0');
    insert into timestamptz values (8, '2018-01-01 13:33:33 +1:0');
    insert into timestamptz values (9, '2018-01-01 14:33:33 +2:0');

    select n, v, cast(v as timestamp) from timestamptz order by cast(v as timestamp), n;
    select n, v, cast(v as timestamp) from timestamptz order by v, n;

    select n, v from timestamptz order by v + 0, n;

    commit;
    drop index timestamptz_idx;

    select n, v, cast(v as timestamp) from timestamptz order by v, n;

    delete from timestamptz;
    insert into timestamptz values (1, '2018-01-01 11:33:33.456');


    ---


    select t.*,
           extract(timezone_hour from rdb$start_timestamp at time zone 'America/Los_Angeles') start_tzh,
           extract(timezone_minute from rdb$start_timestamp at time zone 'America/Los_Angeles') start_tzm,
           extract(timezone_hour from rdb$end_timestamp at time zone 'America/Los_Angeles') end_tzh,
           extract(timezone_minute from rdb$end_timestamp at time zone 'America/Los_Angeles') end_tzm
      from rdb$time_zone_util.transitions(
        'America/Los_Angeles',
        timestamp '2015-01-01 GMT',
        timestamp '2019-01-01 GMT') t;


    --------------------------
    select * from timetz;
    select * from timestamptz;
    --------------------------

    set term ^;

    set time zone 'America/Sao_Paulo'^

    select substring(current_timestamp from 26) from rdb$database^

    execute block returns (t1 varchar(30), t2 varchar(30))
    as
        declare procedure p0 returns (t1 varchar(30), t2 varchar(30))
        as
        begin
            set time zone 'America/New_York';
            t1 = substring(current_timestamp from 26);
            set time zone local;
            t2 = substring(current_timestamp from 26);
        end

        declare procedure p1 returns (t1 varchar(30), t2 varchar(30))
        as
        begin
            set time zone 'America/Los_Angeles';

            execute procedure p0 returning_values t1, t2;
        end
    begin
        execute procedure p1 returning_values t1, t2;
        suspend;
    end^

    select substring(current_timestamp from 26) from rdb$database^


    set time zone 'America/Sao_Paulo'^

    execute block returns (n integer, t1 varchar(30), t2 varchar(30), t3 varchar(30))
    as
        declare procedure p1 returns (n integer, t1 varchar(30), t2 varchar(30), t3 varchar(30))
        as
        begin
            n = 0;

            while (n <= 4)
            do
            begin
                t1 = substring(current_timestamp from 26);
                set time zone 'America/Los_Angeles';
                t2 = substring(current_timestamp from 26);
                if (n <= 2) then
                    set time zone local;
                t3 = substring(current_timestamp from 26);
                suspend;

                n = n + 1;
            end
        end
    begin
        for select n, t1, t2, t3 from p1 into n, t1, t2, t3
        do
        begin
            suspend;
        end
    end^


    set time zone 'America/Sao_Paulo'^

    create table t1 (n integer, tz1 varchar(30), tz2 varchar(30), tz3 varchar(30))^

    create trigger t1_bi before insert on t1
    as
    begin
        new.tz1 = substring(current_timestamp from 26);
        set time zone 'America/New_York';
        new.tz2 = substring(current_timestamp from 26);
        set time zone local;
        new.tz3 = substring(current_timestamp from 26);
    end^

    insert into t1 (n) values (1)^
    select * from t1^

    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' '), (': table:.*', '')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_INDEX_TMTZ = '"TIMETZ_UK"' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TIMETZ_UK"'
    TEST_INDEX_TSTZ = '"TIMESTAMPTZ_UK"' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TIMESTAMPTZ_UK"'

    expected_stdout_5x = f"""
        CAST 01:23:45.0000 +00:00
        CAST 2018-01-01 01:23:45.0000 +00:00
        EXTRACT 0
        EXTRACT 0
        EXTRACT 0
        EXTRACT 0
        CAST 01:23:45.0000
        CAST 2018-01-01 01:23:45.0000
        CAST 01:23:45.0000 +02:00
        CAST 2018-01-01 01:23:45.0000 +02:00
        EXTRACT 2
        EXTRACT 0
        EXTRACT 2
        EXTRACT 0
        CAST 23:23:45.0000
        CAST 2017-12-31 23:23:45.0000
        CONSTANT 01:23:45.0000 +02:00
        CONSTANT 2018-01-01 01:23:45.0000 +02:00
        EXTRACT 2
        EXTRACT 0
        EXTRACT 2
        EXTRACT 0
        CAST 23:23:45.0000
        CAST 2017-12-31 23:23:45.0000
        CAST 01:23:45.0000 -02:00
        CAST 2018-01-01 01:23:45.0000 -02:00
        EXTRACT -2
        EXTRACT 0
        EXTRACT -2
        EXTRACT 0
        CAST 01:23:45.0000
        CAST 2018-01-01 01:23:45.0000
        CAST 01:23:45.0000 +02:00
        CAST 2018-01-01 01:23:45.0000 +02:00
        EXTRACT 2
        EXTRACT 0
        EXTRACT 2
        EXTRACT 0
        CAST 21:23:45.0000
        CAST 2017-12-31 21:23:45.0000
        EXTRACT 3
        EXTRACT 4
        EXTRACT 5.6789
        EXTRACT 678.9
        EXTRACT 2018
        EXTRACT 1
        EXTRACT 2
        EXTRACT 3
        EXTRACT 4
        EXTRACT 5.6789
        EXTRACT 678.9
        CONSTANT 2017-10-14 22:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        ADD 2017-10-15 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        ADD 2017-10-16 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        ADD 2017-10-17 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        CONSTANT 2017-10-16 22:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        SUBTRACT 2017-10-15 22:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        SUBTRACT 2017-10-14 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        SUBTRACT 2017-10-13 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        DATEADD 2017-10-14 21:00:00.0000 America/Sao_Paulo
        DATEADD 2017-10-14 22:00:00.0000 America/Sao_Paulo
        DATEADD 2017-10-14 23:00:00.0000 America/Sao_Paulo
        DATEADD 2017-10-15 01:00:00.0000 America/Sao_Paulo
        DATEADD 2017-10-15 02:00:00.0000 America/Sao_Paulo
        CONSTANT 2018-02-17 22:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        ADD 2018-02-18 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        ADD 2018-02-19 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        ADD 2018-02-20 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        CONSTANT 2018-02-19 22:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        SUBTRACT 2018-02-18 22:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        SUBTRACT 2018-02-17 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        SUBTRACT 2018-02-16 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        DATEADD 2018-02-17 23:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-17 23:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-18 00:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-18 01:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-18 00:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-17 23:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-17 23:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-17 22:00:00.0000 America/Sao_Paulo
        DATEDIFF 1
        DATEDIFF 0
        DATEDIFF 0
        CAST 01:23:45.0000 -02:20
        CAST 2018-01-01 01:23:45.0000 -02:20
        EXTRACT -2
        EXTRACT -20
        EXTRACT -2
        EXTRACT -20
        CAST 01:23:45.0000
        CAST 2018-01-01 01:23:45.0000
        CAST 01:23:45.0000 +02:00
        CAST 2018-01-01 01:23:45.0000 +02:00
        EXTRACT 2
        EXTRACT 0
        EXTRACT 2
        EXTRACT 0
        CAST 21:03:45.0000
        CAST 2017-12-31 21:03:45.0000
        EXTRACT -3
        EXTRACT 0
        EXTRACT -3
        EXTRACT 0
        <true>
        <true>
        CAST 00:23:45.0000
        CAST 22:23:45.0000
        CAST 2018-01-01 00:23:45.0000
        CAST 2017-12-31 22:23:45.0000
        <true>
        CAST 01:23:45.0000 -02:00
        CAST 01:23:45.0000 -04:00
        CAST 2018-01-01
        CAST 2018-01-01
        CAST 01:23:45.0000
        CAST 03:23:45.0000
        CAST 2018-01-01 01:23:45.0000
        CAST 2018-01-01 03:23:45.0000
        CAST 2018-01-01 01:23:45.0000 -02:00
        CAST 01:23:45.0000 -02:00
        <true>
        <true>
        <true>
        <true>
        Statement failed, SQLSTATE = 22018
        conversion error from string "01:23:45.0000 -03:00"
        CAST 2018-01-01 00:00:00.0000 -02:00
        Statement failed, SQLSTATE = 22018
        conversion error from string "2018-01-01"
        CONSTANT 2018-02-03 00:00:00.0000 America/Sao_Paulo
        ADD 23:23:35.0000 +05:00
        SUBTRACT 23:23:33.0000 +05:00
        ADD 2018-01-02 23:23:34.0000 +05:00
        SUBTRACT 2017-12-31 23:23:34.0000 +05:00
        ADD 2018-01-01 23:23:34.0000 +05:00
        ADD 2018-01-01 23:23:34.0000 +05:00
        ADD 2018-01-01 16:23:34.0000
        ADD 2018-01-01 16:23:34.0000
        SUBTRACT -3600.0000
        SUBTRACT -82800.0000
        SUBTRACT 7200.0000
        SUBTRACT -7200.0000
        SUBTRACT -0.041666667
        SUBTRACT 0.041666667
        SUBTRACT 0.083333333
        SUBTRACT -0.083333333
        <true>
        <true>
        <true>
        <true>
        <true>
        <true>
        <false>
        <false>
        <true>
        <true>
        <true>
        <true>
        <false>
        <true>
        <true>
        <true>
        <false>
        <false>
        <true>
        <true>
        CAST 10:11:12.1345
        CAST 10:11:12.1345 -03:00
        SUBSTRING 10:11:12.1345
        SUBSTRING 10:11:12.1345 -03:00
        CAST 10:11:12.1345
        CAST 10:11:12.1345 -03:00
        CAST 2020-05-20 10:11:12.1345
        CAST 2020-05-20 10:11:12.1345 -03:00
        CAST 10:11:12.1345
        CAST 10:11:12.1345 America/Sao_Paulo
        SUBSTRING 10:11:12.1345
        SUBSTRING 10:11:12.1345 America/Sao_Paulo
        CAST 10:11:12.1345
        CAST 10:11:12.1345 America/Sao_Paulo
        CAST 2020-05-20 10:11:12.1345
        CAST 2020-05-20 10:11:12.1345 America/Sao_Paulo
        AT 20:01:02.0000 -05:00
        AT 23:01:02.0000 -02:00
        AT 04:01:02.0000 +03:00
        AT 17:01:02.0000 -05:00
        AT 20:01:02.0000 -02:00
        AT 01:01:02.0000 +03:00
        AT 23:01:02.0000 -02:00
        AT 20:01:02.0000 -02:00
        AT 2018-01-01 20:01:02.0000 -05:00
        AT 2018-01-01 23:01:02.0000 -02:00
        AT 2018-01-02 04:01:02.0000 +03:00
        AT 2018-01-01 17:01:02.0000 -05:00
        AT 2018-01-01 20:01:02.0000 -02:00
        AT 2018-01-02 01:01:02.0000 +03:00
        AT 2018-01-01 23:01:02.0000 -02:00
        AT 2018-01-01 20:01:02.0000 -02:00
        AT 2018-05-01 16:01:02.0000 America/Los_Angeles
        AT 2018-04-01 16:01:02.0000 America/Los_Angeles
        AT 2018-03-01 15:01:02.0000 America/Los_Angeles
        AT 2018-02-01 14:01:02.0000 America/Los_Angeles
        AT 2018-01-01 14:01:02.0000 America/Los_Angeles
        ADD 2018-01-02 14:01:02.0000 America/Los_Angeles
        ADD 2018-01-02 14:01:02.0000 America/Los_Angeles
        ADD 2018-01-03 14:01:02.0000 America/Los_Angeles
        FIRST_DAY 2018-01-01 10:11:12.0000 America/Sao_Paulo
        FIRST_DAY 2018-03-01 10:11:12.0000 America/Sao_Paulo
        FIRST_DAY 2018-03-04 10:11:12.0000 America/Sao_Paulo
        LAST_DAY 2018-12-31 10:11:12.0000 America/Sao_Paulo
        LAST_DAY 2018-03-31 10:11:12.0000 America/Sao_Paulo
        LAST_DAY 2018-03-10 10:11:12.0000 America/Sao_Paulo
        T1 2017-03-12 03:30:00.0000 America/New_York
        T2 2017-03-12 02:30:00.0000 -05:00
        T3 2017-03-12 03:29:00.0000 America/New_York
        T4 2017-03-12 03:31:00.0000 America/New_York
        T5 2017-03-12 01:30:00.0000 America/New_York
        T6 2017-03-12 04:30:00.0000 America/New_York
        T1 2017-11-05 01:30:00.0000 America/New_York
        T2 2017-11-05 01:30:00.0000 -04:00
        T3 2017-11-05 01:29:00.0000 America/New_York
        T4 2017-11-05 01:31:00.0000 America/New_York
        T5 2017-11-05 00:30:00.0000 America/New_York
        T6 2017-11-05 01:30:00.0000 America/New_York
        INPUT message field count: 0
        OUTPUT message field count: 2
        01: sqltype: 510 TIMESTAMP scale: 0 subtype: 0 len: 8
        : name: CONSTANT alias: CONSTANT
        : table: owner:
        02: sqltype: 560 TIME scale: 0 subtype: 0 len: 4
        : name: DATEADD alias: DATEADD
        : table: owner:
        CONSTANT 2018-05-01 21:01:02.0000
        DATEADD 21:01:02.0000
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {TEST_INDEX_TMTZ}
        -Problematic key value is ("V" = '12:33:33.0000 +00:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {TEST_INDEX_TMTZ}
        -Problematic key value is ("V" = '13:33:33.0000 +01:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {TEST_INDEX_TMTZ}
        -Problematic key value is ("V" = '14:33:33.0000 +02:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {TEST_INDEX_TMTZ}
        -Problematic key value is ("V" = '11:33:33.0000 -03:00')
        N 6
        V 11:33:33.0000 +01:00
        CAST 08:33:33.0000
        N 3
        V 11:33:33.4560 +01:00
        CAST 08:33:33.4560
        N 5
        V 11:33:33.0000 -01:00
        CAST 10:33:33.0000
        N 2
        V 11:33:33.4560 -01:00
        CAST 10:33:33.4560
        N 4
        V 11:33:33.0000 -02:00
        CAST 11:33:33.0000
        N 1
        V 11:33:33.4560 -02:00
        CAST 11:33:33.4560
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        CAST 12:33:33.0000
        N 6
        V 11:33:33.0000 +01:00
        N 3
        V 11:33:33.4560 +01:00
        N 5
        V 11:33:33.0000 -01:00
        N 2
        V 11:33:33.4560 -01:00
        N 4
        V 11:33:33.0000 -02:00
        N 1
        V 11:33:33.4560 -02:00
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        N 6
        V 11:33:33.0000 +01:00
        CAST 08:33:33.0000
        N 3
        V 11:33:33.4560 +01:00
        CAST 08:33:33.4560
        N 5
        V 11:33:33.0000 -01:00
        CAST 10:33:33.0000
        N 7
        V 12:33:33.0000 +00:00
        CAST 10:33:33.0000
        N 8
        V 13:33:33.0000 +01:00
        CAST 10:33:33.0000
        N 9
        V 14:33:33.0000 +02:00
        CAST 10:33:33.0000
        N 2
        V 11:33:33.4560 -01:00
        CAST 10:33:33.4560
        N 4
        V 11:33:33.0000 -02:00
        CAST 11:33:33.0000
        N 1
        V 11:33:33.4560 -02:00
        CAST 11:33:33.4560
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        CAST 12:33:33.0000
        N 6
        V 11:33:33.0000 +01:00
        CAST 08:33:33.0000
        N 3
        V 11:33:33.4560 +01:00
        CAST 08:33:33.4560
        N 5
        V 11:33:33.0000 -01:00
        CAST 10:33:33.0000
        N 7
        V 12:33:33.0000 +00:00
        CAST 10:33:33.0000
        N 8
        V 13:33:33.0000 +01:00
        CAST 10:33:33.0000
        N 9
        V 14:33:33.0000 +02:00
        CAST 10:33:33.0000
        N 2
        V 11:33:33.4560 -01:00
        CAST 10:33:33.4560
        N 4
        V 11:33:33.0000 -02:00
        CAST 11:33:33.0000
        N 1
        V 11:33:33.4560 -02:00
        CAST 11:33:33.4560
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        CAST 12:33:33.0000
        N 6
        V 11:33:33.0000 +01:00
        N 3
        V 11:33:33.4560 +01:00
        N 5
        V 11:33:33.0000 -01:00
        N 7
        V 12:33:33.0000 +00:00
        N 8
        V 13:33:33.0000 +01:00
        N 9
        V 14:33:33.0000 +02:00
        N 2
        V 11:33:33.4560 -01:00
        N 4
        V 11:33:33.0000 -02:00
        N 1
        V 11:33:33.4560 -02:00
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        N 6
        V 11:33:33.0000 +01:00
        CAST 08:33:33.0000
        N 3
        V 11:33:33.4560 +01:00
        CAST 08:33:33.4560
        N 5
        V 11:33:33.0000 -01:00
        CAST 10:33:33.0000
        N 7
        V 12:33:33.0000 +00:00
        CAST 10:33:33.0000
        N 8
        V 13:33:33.0000 +01:00
        CAST 10:33:33.0000
        N 9
        V 14:33:33.0000 +02:00
        CAST 10:33:33.0000
        N 2
        V 11:33:33.4560 -01:00
        CAST 10:33:33.4560
        N 4
        V 11:33:33.0000 -02:00
        CAST 11:33:33.0000
        N 1
        V 11:33:33.4560 -02:00
        CAST 11:33:33.4560
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        CAST 12:33:33.0000
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {TEST_INDEX_TSTZ}
        -Problematic key value is ("V" = '2018-01-01 12:33:33.0000 +00:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {TEST_INDEX_TSTZ}
        -Problematic key value is ("V" = '2018-01-01 13:33:33.0000 +01:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index {TEST_INDEX_TSTZ}
        -Problematic key value is ("V" = '2018-01-01 14:33:33.0000 +02:00')
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        CAST 2018-01-01 08:33:33.0000
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        CAST 2018-01-01 08:33:33.4560
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        CAST 2018-01-01 10:33:33.0000
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        CAST 2018-01-01 10:33:33.4560
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        CAST 2018-01-01 11:33:33.0000
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        CAST 2018-01-01 11:33:33.4560
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        CAST 2018-01-01 08:33:33.0000
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        CAST 2018-01-01 08:33:33.4560
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        CAST 2018-01-01 10:33:33.0000
        N 7
        V 2018-01-01 12:33:33.0000 +00:00
        CAST 2018-01-01 10:33:33.0000
        N 8
        V 2018-01-01 13:33:33.0000 +01:00
        CAST 2018-01-01 10:33:33.0000
        N 9
        V 2018-01-01 14:33:33.0000 +02:00
        CAST 2018-01-01 10:33:33.0000
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        CAST 2018-01-01 10:33:33.4560
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        CAST 2018-01-01 11:33:33.0000
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        CAST 2018-01-01 11:33:33.4560
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        CAST 2018-01-01 08:33:33.0000
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        CAST 2018-01-01 08:33:33.4560
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        CAST 2018-01-01 10:33:33.0000
        N 7
        V 2018-01-01 12:33:33.0000 +00:00
        CAST 2018-01-01 10:33:33.0000
        N 8
        V 2018-01-01 13:33:33.0000 +01:00
        CAST 2018-01-01 10:33:33.0000
        N 9
        V 2018-01-01 14:33:33.0000 +02:00
        CAST 2018-01-01 10:33:33.0000
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        CAST 2018-01-01 10:33:33.4560
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        CAST 2018-01-01 11:33:33.0000
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        CAST 2018-01-01 11:33:33.4560
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        N 7
        V 2018-01-01 12:33:33.0000 +00:00
        N 8
        V 2018-01-01 13:33:33.0000 +01:00
        N 9
        V 2018-01-01 14:33:33.0000 +02:00
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        CAST 2018-01-01 08:33:33.0000
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        CAST 2018-01-01 08:33:33.4560
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        CAST 2018-01-01 10:33:33.0000
        N 7
        V 2018-01-01 12:33:33.0000 +00:00
        CAST 2018-01-01 10:33:33.0000
        N 8
        V 2018-01-01 13:33:33.0000 +01:00
        CAST 2018-01-01 10:33:33.0000
        N 9
        V 2018-01-01 14:33:33.0000 +02:00
        CAST 2018-01-01 10:33:33.0000
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        CAST 2018-01-01 10:33:33.4560
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        CAST 2018-01-01 11:33:33.0000
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        CAST 2018-01-01 11:33:33.4560
        RDB$START_TIMESTAMP 2014-11-02 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2015-03-08 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        RDB$START_TIMESTAMP 2015-03-08 10:00:00.0000 GMT
        RDB$END_TIMESTAMP 2015-11-01 08:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 60
        RDB$EFFECTIVE_OFFSET -420
        START_TZH -7
        START_TZM 0
        END_TZH -7
        END_TZM 0
        RDB$START_TIMESTAMP 2015-11-01 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2016-03-13 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        RDB$START_TIMESTAMP 2016-03-13 10:00:00.0000 GMT
        RDB$END_TIMESTAMP 2016-11-06 08:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 60
        RDB$EFFECTIVE_OFFSET -420
        START_TZH -7
        START_TZM 0
        END_TZH -7
        END_TZM 0
        RDB$START_TIMESTAMP 2016-11-06 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2017-03-12 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        RDB$START_TIMESTAMP 2017-03-12 10:00:00.0000 GMT
        RDB$END_TIMESTAMP 2017-11-05 08:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 60
        RDB$EFFECTIVE_OFFSET -420
        START_TZH -7
        START_TZM 0
        END_TZH -7
        END_TZM 0
        RDB$START_TIMESTAMP 2017-11-05 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2018-03-11 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        RDB$START_TIMESTAMP 2018-03-11 10:00:00.0000 GMT
        RDB$END_TIMESTAMP 2018-11-04 08:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 60
        RDB$EFFECTIVE_OFFSET -420
        START_TZH -7
        START_TZM 0
        END_TZH -7
        END_TZM 0
        RDB$START_TIMESTAMP 2018-11-04 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2019-03-10 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        N 1
        V 11:33:33.4560 -02:00
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        SUBSTRING America/Sao_Paulo
        T1 America/New_York
        T2 America/Los_Angeles
        SUBSTRING America/Los_Angeles
        N 0
        T1 America/Sao_Paulo
        T2 America/Los_Angeles
        T3 America/Sao_Paulo
        N 1
        T1 America/Sao_Paulo
        T2 America/Los_Angeles
        T3 America/Sao_Paulo
        N 2
        T1 America/Sao_Paulo
        T2 America/Los_Angeles
        T3 America/Sao_Paulo
        N 3
        T1 America/Sao_Paulo
        T2 America/Los_Angeles
        T3 America/Los_Angeles
        N 4
        T1 America/Los_Angeles
        T2 America/Los_Angeles
        T3 America/Los_Angeles
        N 1
        TZ1 America/Sao_Paulo
        TZ2 America/New_York
        TZ3 America/Sao_Paulo
    """

    expected_stdout_6x = f"""
        CAST 01:23:45.0000 +00:00
        CAST 2018-01-01 01:23:45.0000 +00:00
        EXTRACT 0
        EXTRACT 0
        EXTRACT 0
        EXTRACT 0
        CAST 01:23:45.0000
        CAST 2018-01-01 01:23:45.0000
        CAST 01:23:45.0000 +02:00
        CAST 2018-01-01 01:23:45.0000 +02:00
        EXTRACT 2
        EXTRACT 0
        EXTRACT 2
        EXTRACT 0
        CAST 23:23:45.0000
        CAST 2017-12-31 23:23:45.0000
        CONSTANT 01:23:45.0000 +02:00
        CONSTANT 2018-01-01 01:23:45.0000 +02:00
        EXTRACT 2
        EXTRACT 0
        EXTRACT 2
        EXTRACT 0
        CAST 23:23:45.0000
        CAST 2017-12-31 23:23:45.0000
        CAST 01:23:45.0000 -02:00
        CAST 2018-01-01 01:23:45.0000 -02:00
        EXTRACT -2
        EXTRACT 0
        EXTRACT -2
        EXTRACT 0
        CAST 01:23:45.0000
        CAST 2018-01-01 01:23:45.0000
        CAST 01:23:45.0000 +02:00
        CAST 2018-01-01 01:23:45.0000 +02:00
        EXTRACT 2
        EXTRACT 0
        EXTRACT 2
        EXTRACT 0
        CAST 21:23:45.0000
        CAST 2017-12-31 21:23:45.0000
        EXTRACT 3
        EXTRACT 4
        EXTRACT 5.6789
        EXTRACT 678.9
        EXTRACT 2018
        EXTRACT 1
        EXTRACT 2
        EXTRACT 3
        EXTRACT 4
        EXTRACT 5.6789
        EXTRACT 678.9
        CONSTANT 2017-10-14 22:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        ADD 2017-10-15 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        ADD 2017-10-16 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        ADD 2017-10-17 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        CONSTANT 2017-10-16 22:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        SUBTRACT 2017-10-15 22:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        SUBTRACT 2017-10-14 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        SUBTRACT 2017-10-13 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        DATEADD 2017-10-14 21:00:00.0000 America/Sao_Paulo
        DATEADD 2017-10-14 22:00:00.0000 America/Sao_Paulo
        DATEADD 2017-10-14 23:00:00.0000 America/Sao_Paulo
        DATEADD 2017-10-15 01:00:00.0000 America/Sao_Paulo
        DATEADD 2017-10-15 02:00:00.0000 America/Sao_Paulo
        CONSTANT 2018-02-17 22:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        ADD 2018-02-18 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        ADD 2018-02-19 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        ADD 2018-02-20 21:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        CONSTANT 2018-02-19 22:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        SUBTRACT 2018-02-18 22:00:00.0000 America/Sao_Paulo
        EXTRACT -3
        SUBTRACT 2018-02-17 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        SUBTRACT 2018-02-16 23:00:00.0000 America/Sao_Paulo
        EXTRACT -2
        DATEADD 2018-02-17 23:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-17 23:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-18 00:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-18 01:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-18 00:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-17 23:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-17 23:00:00.0000 America/Sao_Paulo
        DATEADD 2018-02-17 22:00:00.0000 America/Sao_Paulo
        DATEDIFF 1
        DATEDIFF 0
        DATEDIFF 0
        CAST 01:23:45.0000 -02:20
        CAST 2018-01-01 01:23:45.0000 -02:20
        EXTRACT -2
        EXTRACT -20
        EXTRACT -2
        EXTRACT -20
        CAST 01:23:45.0000
        CAST 2018-01-01 01:23:45.0000
        CAST 01:23:45.0000 +02:00
        CAST 2018-01-01 01:23:45.0000 +02:00
        EXTRACT 2
        EXTRACT 0
        EXTRACT 2
        EXTRACT 0
        CAST 21:03:45.0000
        CAST 2017-12-31 21:03:45.0000
        EXTRACT -3
        EXTRACT 0
        EXTRACT -3
        EXTRACT 0
        BOOL <true>
        BOOL <true>
        CAST 00:23:45.0000
        CAST 22:23:45.0000
        CAST 2018-01-01 00:23:45.0000
        CAST 2017-12-31 22:23:45.0000
        BOOL <true>
        CAST 01:23:45.0000 -02:00
        CAST 01:23:45.0000 -04:00
        CAST 2018-01-01
        CAST 2018-01-01
        CAST 01:23:45.0000
        CAST 03:23:45.0000
        CAST 2018-01-01 01:23:45.0000
        CAST 2018-01-01 03:23:45.0000
        CAST 2018-01-01 01:23:45.0000 -02:00
        CAST 01:23:45.0000 -02:00
        BOOL <true>
        BOOL <true>
        BOOL <true>
        BOOL <true>
        Statement failed, SQLSTATE = 22018
        conversion error from string "01:23:45.0000 -03:00"
        CAST 2018-01-01 00:00:00.0000 -02:00
        Statement failed, SQLSTATE = 22018
        conversion error from string "2018-01-01"
        CONSTANT 2018-02-03 00:00:00.0000 America/Sao_Paulo
        ADD 23:23:35.0000 +05:00
        SUBTRACT 23:23:33.0000 +05:00
        ADD 2018-01-02 23:23:34.0000 +05:00
        SUBTRACT 2017-12-31 23:23:34.0000 +05:00
        ADD 2018-01-01 23:23:34.0000 +05:00
        ADD 2018-01-01 23:23:34.0000 +05:00
        ADD 2018-01-01 16:23:34.0000
        ADD 2018-01-01 16:23:34.0000
        SUBTRACT -3600.0000
        SUBTRACT -82800.0000
        SUBTRACT 7200.0000
        SUBTRACT -7200.0000
        SUBTRACT -0.041666667
        SUBTRACT 0.041666667
        SUBTRACT 0.083333333
        SUBTRACT -0.083333333
        BOOL <true>
        BOOL <true>
        BOOL <true>
        BOOL <true>
        BOOL <true>
        BOOL <true>
        BOOL <false>
        BOOL <false>
        BOOL <true>
        BOOL <true>
        BOOL <true>
        BOOL <true>
        BOOL <false>
        BOOL <true>
        BOOL <true>
        BOOL <true>
        BOOL <false>
        BOOL <false>
        BOOL <true>
        BOOL <true>
        CAST 10:11:12.1345
        CAST 10:11:12.1345 -03:00
        SUBSTRING 10:11:12.1345
        SUBSTRING 10:11:12.1345 -03:00
        CAST 10:11:12.1345
        CAST 10:11:12.1345 -03:00
        CAST 2020-05-20 10:11:12.1345
        CAST 2020-05-20 10:11:12.1345 -03:00
        CAST 10:11:12.1345
        CAST 10:11:12.1345 America/Sao_Paulo
        SUBSTRING 10:11:12.1345
        SUBSTRING 10:11:12.1345 America/Sao_Paulo
        CAST 10:11:12.1345
        CAST 10:11:12.1345 America/Sao_Paulo
        CAST 2020-05-20 10:11:12.1345
        CAST 2020-05-20 10:11:12.1345 America/Sao_Paulo
        AT 20:01:02.0000 -05:00
        AT 23:01:02.0000 -02:00
        AT 04:01:02.0000 +03:00
        AT 17:01:02.0000 -05:00
        AT 20:01:02.0000 -02:00
        AT 01:01:02.0000 +03:00
        AT 23:01:02.0000 -02:00
        AT 20:01:02.0000 -02:00
        AT 2018-01-01 20:01:02.0000 -05:00
        AT 2018-01-01 23:01:02.0000 -02:00
        AT 2018-01-02 04:01:02.0000 +03:00
        AT 2018-01-01 17:01:02.0000 -05:00
        AT 2018-01-01 20:01:02.0000 -02:00
        AT 2018-01-02 01:01:02.0000 +03:00
        AT 2018-01-01 23:01:02.0000 -02:00
        AT 2018-01-01 20:01:02.0000 -02:00
        AT 2018-05-01 16:01:02.0000 America/Los_Angeles
        AT 2018-04-01 16:01:02.0000 America/Los_Angeles
        AT 2018-03-01 15:01:02.0000 America/Los_Angeles
        AT 2018-02-01 14:01:02.0000 America/Los_Angeles
        AT 2018-01-01 14:01:02.0000 America/Los_Angeles
        ADD 2018-01-02 14:01:02.0000 America/Los_Angeles
        ADD 2018-01-02 14:01:02.0000 America/Los_Angeles
        ADD 2018-01-03 14:01:02.0000 America/Los_Angeles
        FIRST_DAY 2018-01-01 10:11:12.0000 America/Sao_Paulo
        FIRST_DAY 2018-03-01 10:11:12.0000 America/Sao_Paulo
        FIRST_DAY 2018-03-04 10:11:12.0000 America/Sao_Paulo
        LAST_DAY 2018-12-31 10:11:12.0000 America/Sao_Paulo
        LAST_DAY 2018-03-31 10:11:12.0000 America/Sao_Paulo
        LAST_DAY 2018-03-10 10:11:12.0000 America/Sao_Paulo
        T1 2017-03-12 03:30:00.0000 America/New_York
        T2 2017-03-12 02:30:00.0000 -05:00
        T3 2017-03-12 03:29:00.0000 America/New_York
        T4 2017-03-12 03:31:00.0000 America/New_York
        T5 2017-03-12 01:30:00.0000 America/New_York
        T6 2017-03-12 04:30:00.0000 America/New_York
        T1 2017-11-05 01:30:00.0000 America/New_York
        T2 2017-11-05 01:30:00.0000 -04:00
        T3 2017-11-05 01:29:00.0000 America/New_York
        T4 2017-11-05 01:31:00.0000 America/New_York
        T5 2017-11-05 00:30:00.0000 America/New_York
        T6 2017-11-05 01:30:00.0000 America/New_York
        INPUT message field count: 0
        OUTPUT message field count: 2
        01: sqltype: 510 TIMESTAMP scale: 0 subtype: 0 len: 8
        : name: CONSTANT alias: CONSTANT
        02: sqltype: 560 TIME scale: 0 subtype: 0 len: 4
        : name: DATEADD alias: DATEADD
        CONSTANT 2018-05-01 21:01:02.0000
        DATEADD 21:01:02.0000
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."TIMETZ_UK"
        -Problematic key value is ("V" = '12:33:33.0000 +00:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."TIMETZ_UK"
        -Problematic key value is ("V" = '13:33:33.0000 +01:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."TIMETZ_UK"
        -Problematic key value is ("V" = '14:33:33.0000 +02:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."TIMETZ_UK"
        -Problematic key value is ("V" = '11:33:33.0000 -03:00')
        N 6
        V 11:33:33.0000 +01:00
        CAST 08:33:33.0000
        N 3
        V 11:33:33.4560 +01:00
        CAST 08:33:33.4560
        N 5
        V 11:33:33.0000 -01:00
        CAST 10:33:33.0000
        N 2
        V 11:33:33.4560 -01:00
        CAST 10:33:33.4560
        N 4
        V 11:33:33.0000 -02:00
        CAST 11:33:33.0000
        N 1
        V 11:33:33.4560 -02:00
        CAST 11:33:33.4560
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        CAST 12:33:33.0000
        N 6
        V 11:33:33.0000 +01:00
        N 3
        V 11:33:33.4560 +01:00
        N 5
        V 11:33:33.0000 -01:00
        N 2
        V 11:33:33.4560 -01:00
        N 4
        V 11:33:33.0000 -02:00
        N 1
        V 11:33:33.4560 -02:00
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        N 6
        V 11:33:33.0000 +01:00
        CAST 08:33:33.0000
        N 3
        V 11:33:33.4560 +01:00
        CAST 08:33:33.4560
        N 5
        V 11:33:33.0000 -01:00
        CAST 10:33:33.0000
        N 7
        V 12:33:33.0000 +00:00
        CAST 10:33:33.0000
        N 8
        V 13:33:33.0000 +01:00
        CAST 10:33:33.0000
        N 9
        V 14:33:33.0000 +02:00
        CAST 10:33:33.0000
        N 2
        V 11:33:33.4560 -01:00
        CAST 10:33:33.4560
        N 4
        V 11:33:33.0000 -02:00
        CAST 11:33:33.0000
        N 1
        V 11:33:33.4560 -02:00
        CAST 11:33:33.4560
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        CAST 12:33:33.0000
        N 6
        V 11:33:33.0000 +01:00
        CAST 08:33:33.0000
        N 3
        V 11:33:33.4560 +01:00
        CAST 08:33:33.4560
        N 5
        V 11:33:33.0000 -01:00
        CAST 10:33:33.0000
        N 7
        V 12:33:33.0000 +00:00
        CAST 10:33:33.0000
        N 8
        V 13:33:33.0000 +01:00
        CAST 10:33:33.0000
        N 9
        V 14:33:33.0000 +02:00
        CAST 10:33:33.0000
        N 2
        V 11:33:33.4560 -01:00
        CAST 10:33:33.4560
        N 4
        V 11:33:33.0000 -02:00
        CAST 11:33:33.0000
        N 1
        V 11:33:33.4560 -02:00
        CAST 11:33:33.4560
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        CAST 12:33:33.0000
        N 6
        V 11:33:33.0000 +01:00
        N 3
        V 11:33:33.4560 +01:00
        N 5
        V 11:33:33.0000 -01:00
        N 7
        V 12:33:33.0000 +00:00
        N 8
        V 13:33:33.0000 +01:00
        N 9
        V 14:33:33.0000 +02:00
        N 2
        V 11:33:33.4560 -01:00
        N 4
        V 11:33:33.0000 -02:00
        N 1
        V 11:33:33.4560 -02:00
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        N 6
        V 11:33:33.0000 +01:00
        CAST 08:33:33.0000
        N 3
        V 11:33:33.4560 +01:00
        CAST 08:33:33.4560
        N 5
        V 11:33:33.0000 -01:00
        CAST 10:33:33.0000
        N 7
        V 12:33:33.0000 +00:00
        CAST 10:33:33.0000
        N 8
        V 13:33:33.0000 +01:00
        CAST 10:33:33.0000
        N 9
        V 14:33:33.0000 +02:00
        CAST 10:33:33.0000
        N 2
        V 11:33:33.4560 -01:00
        CAST 10:33:33.4560
        N 4
        V 11:33:33.0000 -02:00
        CAST 11:33:33.0000
        N 1
        V 11:33:33.4560 -02:00
        CAST 11:33:33.4560
        N 0
        V 11:33:33.0000 America/Sao_Paulo
        CAST 12:33:33.0000
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."TIMESTAMPTZ_UK"
        -Problematic key value is ("V" = '2018-01-01 12:33:33.0000 +00:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."TIMESTAMPTZ_UK"
        -Problematic key value is ("V" = '2018-01-01 13:33:33.0000 +01:00')
        Statement failed, SQLSTATE = 23000
        attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."TIMESTAMPTZ_UK"
        -Problematic key value is ("V" = '2018-01-01 14:33:33.0000 +02:00')
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        CAST 2018-01-01 08:33:33.0000
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        CAST 2018-01-01 08:33:33.4560
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        CAST 2018-01-01 10:33:33.0000
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        CAST 2018-01-01 10:33:33.4560
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        CAST 2018-01-01 11:33:33.0000
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        CAST 2018-01-01 11:33:33.4560
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        CAST 2018-01-01 08:33:33.0000
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        CAST 2018-01-01 08:33:33.4560
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        CAST 2018-01-01 10:33:33.0000
        N 7
        V 2018-01-01 12:33:33.0000 +00:00
        CAST 2018-01-01 10:33:33.0000
        N 8
        V 2018-01-01 13:33:33.0000 +01:00
        CAST 2018-01-01 10:33:33.0000
        N 9
        V 2018-01-01 14:33:33.0000 +02:00
        CAST 2018-01-01 10:33:33.0000
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        CAST 2018-01-01 10:33:33.4560
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        CAST 2018-01-01 11:33:33.0000
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        CAST 2018-01-01 11:33:33.4560
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        CAST 2018-01-01 08:33:33.0000
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        CAST 2018-01-01 08:33:33.4560
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        CAST 2018-01-01 10:33:33.0000
        N 7
        V 2018-01-01 12:33:33.0000 +00:00
        CAST 2018-01-01 10:33:33.0000
        N 8
        V 2018-01-01 13:33:33.0000 +01:00
        CAST 2018-01-01 10:33:33.0000
        N 9
        V 2018-01-01 14:33:33.0000 +02:00
        CAST 2018-01-01 10:33:33.0000
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        CAST 2018-01-01 10:33:33.4560
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        CAST 2018-01-01 11:33:33.0000
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        CAST 2018-01-01 11:33:33.4560
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        N 7
        V 2018-01-01 12:33:33.0000 +00:00
        N 8
        V 2018-01-01 13:33:33.0000 +01:00
        N 9
        V 2018-01-01 14:33:33.0000 +02:00
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        N 6
        V 2018-01-01 11:33:33.0000 +01:00
        CAST 2018-01-01 08:33:33.0000
        N 3
        V 2018-01-01 11:33:33.4560 +01:00
        CAST 2018-01-01 08:33:33.4560
        N 5
        V 2018-01-01 11:33:33.0000 -01:00
        CAST 2018-01-01 10:33:33.0000
        N 7
        V 2018-01-01 12:33:33.0000 +00:00
        CAST 2018-01-01 10:33:33.0000
        N 8
        V 2018-01-01 13:33:33.0000 +01:00
        CAST 2018-01-01 10:33:33.0000
        N 9
        V 2018-01-01 14:33:33.0000 +02:00
        CAST 2018-01-01 10:33:33.0000
        N 2
        V 2018-01-01 11:33:33.4560 -01:00
        CAST 2018-01-01 10:33:33.4560
        N 4
        V 2018-01-01 11:33:33.0000 -02:00
        CAST 2018-01-01 11:33:33.0000
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        CAST 2018-01-01 11:33:33.4560
        RDB$START_TIMESTAMP 2014-11-02 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2015-03-08 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        RDB$START_TIMESTAMP 2015-03-08 10:00:00.0000 GMT
        RDB$END_TIMESTAMP 2015-11-01 08:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 60
        RDB$EFFECTIVE_OFFSET -420
        START_TZH -7
        START_TZM 0
        END_TZH -7
        END_TZM 0
        RDB$START_TIMESTAMP 2015-11-01 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2016-03-13 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        RDB$START_TIMESTAMP 2016-03-13 10:00:00.0000 GMT
        RDB$END_TIMESTAMP 2016-11-06 08:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 60
        RDB$EFFECTIVE_OFFSET -420
        START_TZH -7
        START_TZM 0
        END_TZH -7
        END_TZM 0
        RDB$START_TIMESTAMP 2016-11-06 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2017-03-12 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        RDB$START_TIMESTAMP 2017-03-12 10:00:00.0000 GMT
        RDB$END_TIMESTAMP 2017-11-05 08:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 60
        RDB$EFFECTIVE_OFFSET -420
        START_TZH -7
        START_TZM 0
        END_TZH -7
        END_TZM 0
        RDB$START_TIMESTAMP 2017-11-05 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2018-03-11 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        RDB$START_TIMESTAMP 2018-03-11 10:00:00.0000 GMT
        RDB$END_TIMESTAMP 2018-11-04 08:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 60
        RDB$EFFECTIVE_OFFSET -420
        START_TZH -7
        START_TZM 0
        END_TZH -7
        END_TZM 0
        RDB$START_TIMESTAMP 2018-11-04 09:00:00.0000 GMT
        RDB$END_TIMESTAMP 2019-03-10 09:59:59.9999 GMT
        RDB$ZONE_OFFSET -480
        RDB$DST_OFFSET 0
        RDB$EFFECTIVE_OFFSET -480
        START_TZH -8
        START_TZM 0
        END_TZH -8
        END_TZM 0
        N 1
        V 11:33:33.4560 -02:00
        N 1
        V 2018-01-01 11:33:33.4560 -02:00
        SUBSTRING America/Sao_Paulo
        T1 America/New_York
        T2 America/Los_Angeles
        SUBSTRING America/Los_Angeles
        N 0
        T1 America/Sao_Paulo
        T2 America/Los_Angeles
        T3 America/Sao_Paulo
        N 1
        T1 America/Sao_Paulo
        T2 America/Los_Angeles
        T3 America/Sao_Paulo
        N 2
        T1 America/Sao_Paulo
        T2 America/Los_Angeles
        T3 America/Sao_Paulo
        N 3
        T1 America/Sao_Paulo
        T2 America/Los_Angeles
        T3 America/Los_Angeles
        N 4
        T1 America/Los_Angeles
        T2 America/Los_Angeles
        T3 America/Los_Angeles
        N 1
        TZ1 America/Sao_Paulo
        TZ2 America/New_York
        TZ3 America/Sao_Paulo
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
