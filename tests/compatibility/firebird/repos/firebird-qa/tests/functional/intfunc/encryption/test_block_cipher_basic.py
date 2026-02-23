#coding:utf-8

"""
ID:          intfunc.encryption.block-cipher-basic
TITLE:       ENCRYPT/DECRYPT built-in functions
DESCRIPTION:
  Verify block crypto algorithms that are implemented in ENCRYPT/DECRYPT built-in functions.
  See doc/sql.extensions/README.builtin_functions.txt for details.
FBTEST:      functional.intfunc.encryption.block_cipher_basic
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set blob all;

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

    -- AES supports three key sizes: 128 bits, 192 bits, and 256 bits.
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
        '0101010101010101',
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
        lpad('',16, replace(uuid_to_char(gen_uuid()),'-','') ),
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
        'ecb',
        lpad('', cast(rdb$get_context('USER_SESSION', 'DATA_LEN') as int),uuid_to_char(gen_uuid()) ), -- NB!! data_len must be divided to 16 w/o remainder for ECB!
        lpad('',16, replace(uuid_to_char(gen_uuid()),'-','') ),
        null
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
        lpad('',16, replace(uuid_to_char(gen_uuid()),'-','') ),
        lpad('',16, uuid_to_char( gen_uuid() ))
    );

    ------------------------------------------------------

    insert into test_char
    select
        'ANUBIS',
        mode,
        source_text,
        crypto_key,
        crypto_iv
    from test_char
    where crypto_alg = 'AES'
    ;


    insert into test_char
    select
        'BLOWFISH',
        mode,
        source_text,
        crypto_key,
        left(crypto_iv,8)
    from test_char
    where crypto_alg = 'AES'
    ;


    insert into test_char
    select
        'KHAZAD',
        mode,
        source_text,
        crypto_key,
        left(crypto_iv,8)
    from test_char
    where crypto_alg = 'AES'
    ;


    insert into test_char
    select
        'RC5',
        mode,
        source_text,
        crypto_key,
        left(crypto_iv,8)
    from test_char
    where crypto_alg = 'AES'
    ;


    insert into test_char
    select
        'RC6',
        mode,
        source_text,
        crypto_key,
        lpad('', 16, crypto_iv)
    from test_char
    where crypto_alg = 'AES'
    ;


    insert into test_char
    select
        '"SAFER+"',
        mode,
        source_text,
        crypto_key,
        lpad('', 16, crypto_iv)
    from test_char
    where crypto_alg = 'AES'
    ;


    insert into test_char
    select
        'TWOFISH',
        mode,
        source_text,
        crypto_key,
        lpad('', 16, crypto_iv)
    from test_char
    where crypto_alg = 'AES'
    ;

    insert into test_char
    select
        'XTEA',
        mode,
        source_text,
        crypto_key,
        lpad('', 8, crypto_iv)
    from test_char
    where crypto_alg = 'AES'
    ;



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

            --v_decrypt_sttm = 'select decrypt( q''{' || v_encrypted || '}'' using ' || c.crypto_alg || coalesce( ' mode ' || c.mode , '' ) || ' key q''{' || c.crypto_key || '}''' || coalesce(' iv q''{' || c.crypto_iv || '}'' ', '') || ') from rdb$database';
            --v_decrypt_sttm = 'select decrypt( x''' || v_encrypted || ''' using ' || c.crypto_alg || coalesce( ' mode ' || c.mode , '' ) || ' key q''{' || c.crypto_key || '}''' || coalesce(' iv q''{' || c.crypto_iv || '}'' ', '') || ') from rdb$database';
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
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      AES
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      ANUBIS
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      ANUBIS
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      ANUBIS
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      ANUBIS
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      BLOWFISH
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      BLOWFISH
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      BLOWFISH
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      BLOWFISH
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      KHAZAD
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      KHAZAD
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      KHAZAD
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      KHAZAD
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      RC5
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      RC5
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      RC5
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      RC5
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      RC6
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      RC6
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      RC6
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      RC6
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      "SAFER+"
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      "SAFER+"
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      "SAFER+"
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      "SAFER+"
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      TWOFISH
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      TWOFISH
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      TWOFISH
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      TWOFISH
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      XTEA
    MODE                            CFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      XTEA
    MODE                            CTR
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      XTEA
    MODE                            ECB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>

    CRYPTO_ALG                      XTEA
    MODE                            OFB
    RESULT_MSG                      Source and decrypted strings are identical.
    SRC_TEXT                        <null>
    DECRYPTED_TEXT                  <null>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
