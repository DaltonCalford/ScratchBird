#coding:utf-8

"""
ID:          intfunc.encryption.block-cipher-special
TITLE:       ENCRYPT/DECRYPT built-in functions
DESCRIPTION:
  Verify block crypto algorithms that are implemented in ENCRYPT/DECRYPT built-in functions.
  Additional tests for key length = 192 and 256 bits.
  See doc/sql.extensions/README.builtin_functions.txt for details.
FBTEST:      functional.intfunc.encryption.block_cipher_special
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create or alter procedure sp_char_block_test(a_alg varchar(30)) as begin end;
    commit;

    recreate table test_char( crypto_alg varchar(30), mode varchar(30), source_text varchar(32765), crypto_key varchar(128), crypto_iv varchar(16) );
    commit;

    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION', 'DATA_LEN', 31744);
    end
    ^
    set term ;^


    --############################ AES mode OFB ##########################
    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key,
        crypto_iv )
    values(
        'AES',
        'ofb',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 24, '01'), -- 192 bits
        lpad('',16, uuid_to_char( gen_uuid() ))
    );

    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key,
        crypto_iv )
    values(
        'AES',
        'ofb',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 32, '01'), -- 256 bits
        lpad('',16, uuid_to_char( gen_uuid() ))
    );



    --############################ AES mode CFB ##########################
    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key,
        crypto_iv )
    values(
        'AES',
        'cfb',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 24, '01'), -- 192 bits
        lpad('',16, uuid_to_char( gen_uuid() ))
    );

    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key,
        crypto_iv )
    values(
        'AES',
        'cfb',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 32, '01'), -- 256 bits
        lpad('',16, uuid_to_char( gen_uuid() ))
    );


    --############################ AES mode CTR ##########################
    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key,
        crypto_iv )
    values(
        'AES',
        'ctr',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 24, '01'), -- 192 bits
        lpad('',16, uuid_to_char( gen_uuid() ))
    );

    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key,
        crypto_iv )
    values(
        'AES',
        'ctr',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 32, '01'), -- 256 bits
        lpad('',16, uuid_to_char( gen_uuid() ))
    );


    --############################ AES mode ECB ##########################
    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key )
    values(
        'AES',
        'ecb',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 24, '01') -- 192 bits
    );

    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key )
    values(
        'AES',
        'ecb',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 32, '01') -- 256 bits
    );


    --############################ AES mode CBC ##########################
    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key,
        crypto_iv )
    values(
        'AES',
        'cbc',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 24, '01'), -- 192 bits
        lpad('', 16, uuid_to_char( gen_uuid() ))
    );

    insert into test_char(
        crypto_alg,
        mode,
        source_text,
        crypto_key,
        crypto_iv )
    values(
        'AES',
        'cbc',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ),
        lpad('', 32, '01'),  -- 256 bits
        lpad('',16, uuid_to_char( gen_uuid() ))
    );



/*
select encrypt('897897' using aes mode cfb key 'AbcdAbcdAbcdAbcd' iv '0123456789012345') from rdb$database;
select encrypt('897897' using aes mode ctr key 'AbcdAbcdAbcdAbcd' iv '0123456789012345') from rdb$database;
select encrypt( lpad('', 16, 'A') using aes mode ecb key '123456789012345678901234') from rdb$database;
select encrypt( lpad('', 16, 'A') using aes mode ecb key '12345678901234567890123456789012') from rdb$database;
select encrypt( lpad('', 16, 'A') using aes mode cbc key '123456789012345678901234' iv '1234567812345678') from rdb$database;
select encrypt( lpad('', 16, 'A') using aes mode cbc key '12345678901234567890123456789012' iv '1234567812345678') from rdb$database;

*/

    commit;

    set term ^;
    create or alter procedure sp_char_block_test
    returns(
        crypto_alg type of column test_char.crypto_alg
        ,mode type of column test_char.mode
        ,result_msg varchar(80)
        ,src_text  type of column test_char.source_text
        ,decrypted_text type of column test_char.source_text
    ) as
        declare v_encrypted varchar(32700);
        declare v_decrypted varchar(32700);
        declare v_encrypt_sttm varchar(32700);
        declare v_decrypt_sttm varchar(32700);
    begin
        for
            select
                 t.source_text
                ,t.crypto_alg
                ,t.mode
                ,t.crypto_key
                ,t.crypto_iv
            from test_char t
            as cursor c
        do begin
            v_encrypt_sttm = 'select encrypt( q''{' || c.source_text || '}'' using ' || c.crypto_alg || coalesce( ' mode ' || c.mode , '' ) || ' key q''{' || c.crypto_key || '}''' || coalesce(' iv q''{' || c.crypto_iv || '}'' ', '') || ') from rdb$database';
            execute statement v_encrypt_sttm into v_encrypted;

            v_decrypt_sttm = 'select decrypt( cast(? as varbinary(32700)) using ' || c.crypto_alg || coalesce( ' mode ' || c.mode , '' ) || ' key q''{' || c.crypto_key || '}''' || coalesce(' iv q''{' || c.crypto_iv || '}'' ', '') || ') from rdb$database';
            execute statement ( v_decrypt_sttm ) ( v_encrypted )  into v_decrypted;

            crypto_alg = upper(c.crypto_alg);
            mode = upper(c.mode);
            result_msg = 'Source and decrypted strings ' || iif ( v_decrypted = c.source_text, 'are identical.', 'DIFFERS (ERROR!)' );
            if ( v_decrypted is distinct from c.source_text ) then
            begin
                src_text =  c.source_text;
                decrypted_text = v_decrypted;
            end
            suspend;
        end
    end
    ^
    set term ;^
    commit;

    select * from sp_char_block_test;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CRYPTO_ALG                      AES
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            CBC
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            CBC
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
