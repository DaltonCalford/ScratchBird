    set list on;
    
    -- binary literal ::=  { x | X } <quote> [ { <hexit> <hexit> }... ] <quote>

    select x'1' from rdb$database; -- raises: token unknown because length is odd

    select x'11' from rdb$database; -- must raise: token unknown because length is odd

    select x'0123456789' from rdb$database;

    select x'01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789' from rdb$database;

    -- must raise: token unknown because last char is not hexit
    select x'0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678x' from rdb$database;

    select uuid_to_char(x'BA1749B583BF9146B360F54E25FE583E') from rdb$database;

    
    -- ##############################################################################

    -- Numeric literal: { 0x | 0X } <hexit> [ <hexit>... ]
    -- https://firebirdsql.org/refdocs/langrefupd25-bigint.html
    recreate view v_test as
    select
        +-0x1              "-1(a)"
       ,-+-0xf             "+15"
       ,0x7FFF             "32767"
       ,0x8000             "32768"
       ,0xFFFF             "65535"
       ,0x10000            "65536(a)"
       ,0x000000000010000  "65536(b)"
       ,0x80000000         "-2147483648"
       ,0x080000000        "+2147483648(a)"
       ,0x000000080000000  "+2147483648(b)"
       ,0XFFFFFFFF         "-1(b)"
       ,0X0FFFFFFFF        "+4294967295"
       ,0x100000000        "+4294967296(a)"
       ,0x0000000100000000 "+4294967296(b)"
       ,0X7FFFFFFFFFFFFFFF "9223372036854775807"
       ,0x8000000000000000 "-9223372036854775808"
       ,0x8000000000000001 "-9223372036854775807"
       ,0x8000000000000002 "-9223372036854775806"
       ,0xffffffffffffffff "-1(c)"
    from rdb$database;

    select * from v_test;

    -- If the number of <hexit> is greater than 8, the constant data type is a signed BIGINT
    -- If it's less or equal than 8, the data type is a signed INTEGER
    set sqlda_display on;
    select * from v_test rows 0;
    set sqlda_display off;
