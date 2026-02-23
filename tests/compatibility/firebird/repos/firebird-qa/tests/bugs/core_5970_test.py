#coding:utf-8

"""
ID:          issue-6222
ISSUE:       6222
TITLE:       Built-in cryptographic functions
DESCRIPTION:
  Issues found during implementing this test - see CORE-6185, CORE-6186.
  This test checks only ability to call ENCRYPT()/DECRYPT() functions with different parameters.
  Also, it checks that <source> -> encrypt(<source>) -> decrypt(encrypted_source) gives the same <source>.
JIRA:        CORE-5970
FBTEST:      bugs.core_5970
NOTES:
    [02.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    set list on;
    create or alter procedure sp_block_test(a_alg varchar(30)) as begin end;
    create or alter procedure sp_stream_test(a_alg varchar(30)) as begin end;
    commit;

    recreate table test( crypto_alg varchar(30), source_text blob, crypto_key varchar(128), crypto_iv varchar(128) );

    recreate global temporary table gtt_tmp(
        source_text blob
       ,encrypted_text blob
    ) on commit delete rows;
    commit;


    recreate table secure_table(secret_field varchar(1000), init_vector varchar(16) );
    insert into secure_table(secret_field, init_vector) values( lpad('',1000, 'A'), '1234567890123456');
    commit;

    --set echo on;

    -- Should NOT cause any errors when call encrypt() decrypt() for these params:
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'AES',      lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'ANUBIS',   lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'BLOWFISH', lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'KHAZAD',   lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'RC5',      lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'RC6',      lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( '"SAFER+"', lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'TWOFISH',  lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'XTEA',     lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );

    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'CHACHA20', lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'RC4',      lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), null  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'SOBER128', lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );


    -- Should cause FAILS: invalid length of keys:
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'AES',      lpad('', 65535, gen_uuid()), lpad('',11, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'ANUBIS',   lpad('', 65535, gen_uuid()), lpad('',12, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'BLOWFISH', lpad('', 65535, gen_uuid()), lpad('',13, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'KHAZAD',   lpad('', 65535, gen_uuid()), lpad('',14, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'RC5',      lpad('', 65535, gen_uuid()), lpad('',15, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'RC6',      lpad('', 65535, gen_uuid()), lpad('',17, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( '"SAFER+"', lpad('', 65535, gen_uuid()), lpad('',18, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'TWOFISH',  lpad('', 65535, gen_uuid()), lpad('',19, uuid_to_char( gen_uuid() )), lpad('',16, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'XTEA',     lpad('', 65535, gen_uuid()), lpad('',20, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );

    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'CHACHA20', lpad('', 65535, gen_uuid()), lpad('',21, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'RC4',      lpad('', 65535, gen_uuid()), lpad('',22, uuid_to_char( gen_uuid() )), null  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'SOBER128', lpad('', 65535, gen_uuid()), lpad('',23, uuid_to_char( gen_uuid() )), lpad('', 8, uuid_to_char( gen_uuid() ))  );


    -- Should cause FAILS: invalid length of IVs:
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'AES',      lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',11, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'ANUBIS',   lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',13, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'BLOWFISH', lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',15, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'KHAZAD',   lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',17, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'RC5',      lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',19, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'RC6',      lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',21, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( '"SAFER+"', lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',23, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'TWOFISH',  lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',25, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'XTEA',     lpad('', 65535, gen_uuid()), lpad('',26, uuid_to_char( gen_uuid() )), lpad('',27, uuid_to_char( gen_uuid() ))  );

    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'CHACHA20', lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',29, uuid_to_char( gen_uuid() ))  );
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'RC4',      lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',31, uuid_to_char( gen_uuid() ))   ); -- IV not needed for this alg
    insert into test( crypto_alg, source_text, crypto_key, crypto_iv) values( 'SOBER128', lpad('', 65535, gen_uuid()), lpad('',16, uuid_to_char( gen_uuid() )), lpad('',33, uuid_to_char( gen_uuid() ))  );

    commit;

    set term ^;
    create or alter procedure sp_block_test(a_alg varchar(30))
        returns(
            encryption_algorithm varchar(30)
            ,encryption_mode varchar(10)
            ,enc_key_octet_length int
            ,enc_init_vector_octet_length int
            ,encrypted_equals_to_decrypted boolean
            ,encryption_finish_gdscode int
    ) as
        declare v_encrypted blob;
        declare v_encrypt_sttm blob;
        declare v_decrypt_sttm blob;
        declare s_source_text blob;
        declare s_decrypted_text blob;
    begin
        delete from gtt_tmp;
        for
            select
                 t.source_text
                ,t.crypto_alg
                ,t.crypto_key
                ,t.crypto_iv
            from test t
            where upper( t.crypto_alg ) = upper( :a_alg )
            as cursor c
        do begin

            encryption_algorithm = c.crypto_alg;
            enc_key_octet_length = octet_length( c.crypto_key );
            enc_init_vector_octet_length = octet_length( c.crypto_iv );

            --    block_cipher ::= { AES | ANUBIS | BLOWFISH | KHAZAD | RC5 | RC6 | SAFER+ | TWOFISH | XTEA }
            --    mode ::= { CBC | CFB | CTR | ECB | OFB }

            for
                select 'CBC' as mode from rdb$database union all
                select 'CFB' from rdb$database union all -- AES
                select 'CTR' from rdb$database union all -- AES
                select 'ECB' from rdb$database union all
                select 'OFB' from rdb$database           -- AES
            as cursor cm
            do begin

                encryption_mode = cm.mode;
                encrypted_equals_to_decrypted = null;
                encryption_finish_gdscode = null;
                begin

                    -- Mode should be specified for block ciphers.
                    -- Initialization vector (IV) should be specified for block ciphers in all modes except ECB and all stream ciphers except RC4.

                    insert into gtt_tmp(source_text) values(c.source_text);
                    s_source_text = c.source_text;

                    -- This caused crash when length of string was 65535; sent letter to Alex et al, 11.11.2019:
                    -- v_encrypt_sttm = 'select encrypt( q''{' || c.source_text || '}'' using ' || c.crypto_alg || ' mode ofb key q''{' || c.crypto_key || '}'' iv q''{' || c.crypto_iv || '}'' ) from rdb$database';


                    v_encrypt_sttm = 'select encrypt( t.source_text using ' || c.crypto_alg || ' mode ' || cm.mode || ' key q''{' || c.crypto_key || '}'' iv q''{' || c.crypto_iv || '}'' ) from gtt_tmp t';
                    execute statement v_encrypt_sttm into v_encrypted;

                    update gtt_tmp t set t.encrypted_text = :v_encrypted;

                    v_decrypt_sttm = 'select decrypt( t.encrypted_text using ' || c.crypto_alg || ' mode ' || cm.mode || ' key q''{' || c.crypto_key || '}'' iv q''{' || c.crypto_iv || '}'' ) from gtt_tmp t';
                    execute statement v_decrypt_sttm into s_decrypted_text;


                    encrypted_equals_to_decrypted = false;
                    if ( hash(s_source_text) = hash(s_decrypted_text) ) then
                        if (s_source_text = s_decrypted_text) then
                            encrypted_equals_to_decrypted = true;

                when any do
                    begin
                        -- 335545230 : TomCrypt library error: Invalid argument provided.
                        -- 335545234 : Encrypting in CBC mode

                        -- 335545224 : Initialization vector (IV) makes no sense for chosen cipher and/or mode
                        encryption_finish_gdscode = gdscode;
                    end
                end

                suspend;

                delete from gtt_tmp;

            end
        end

    end
    ^


    create or alter procedure sp_stream_test(a_alg varchar(30))
        returns(
            encryption_algorithm varchar(30)
            ,enc_key_octet_length int
            ,enc_init_vector_octet_length int
            ,encrypted_equals_to_decrypted boolean
            ,encryption_finish_gdscode int
    ) as
        declare v_encrypted blob;
        declare v_encrypt_sttm blob;
        declare v_decrypt_sttm blob;
        declare s_source_text blob;
        declare s_decrypted_text blob;
        declare iv_suffix blob;
    begin
        delete from gtt_tmp;
        for
            select
                 t.source_text
                ,t.crypto_alg
                ,t.crypto_key
                ,t.crypto_iv
            from test t
            where upper( t.crypto_alg ) = upper( :a_alg )
            as cursor c
        do begin

            --    stream_cipher ::= { CHACHA20 | RC4 | SOBER128 }

            encryption_algorithm = c.crypto_alg;
            enc_key_octet_length = octet_length( c.crypto_key );
            encryption_finish_gdscode = null;
            begin

                -- Mode should be specified for block ciphers.
                -- Initialization vector (IV) should be specified for block ciphers in all modes except ECB and all stream ciphers except RC4.

                insert into gtt_tmp(source_text) values(c.source_text);
                s_source_text = c.source_text;

                enc_init_vector_octet_length = 0;
                if ( upper( :a_alg ) = upper('RC4') ) then
                    iv_suffix= '';
                else
                    begin
                        iv_suffix= ' iv q''{' || c.crypto_iv || '}'' ';
                        enc_init_vector_octet_length = octet_length(c.crypto_iv);
                    end

                v_encrypt_sttm = 'select encrypt( t.source_text using ' || c.crypto_alg || ' key q''{' || c.crypto_key || '}'' ' || iv_suffix || ') from gtt_tmp t';
                execute statement v_encrypt_sttm into v_encrypted;

                update gtt_tmp t set t.encrypted_text = :v_encrypted;

                v_decrypt_sttm = 'select decrypt( t.encrypted_text using ' || c.crypto_alg || ' key q''{' || c.crypto_key || '}'' ' || iv_suffix || ') from gtt_tmp t';
                execute statement v_decrypt_sttm into s_decrypted_text;


                encrypted_equals_to_decrypted = false;
                if ( hash(s_source_text) = hash(s_decrypted_text) ) then
                    if (s_source_text = s_decrypted_text) then
                        encrypted_equals_to_decrypted = true;

            when any do
                begin
                    encryption_finish_gdscode = gdscode;
                end
            end

            suspend;
            delete from gtt_tmp;

        end

    end
    ^
    set term ;^
    commit;

    ---------------------------------------

    set bail off;

    -- 1. Main checks:
    -- ###############
    -- 1.1 Block cipher:
    select * from sp_block_test('aes');
    select * from sp_block_test('anubis');
    select * from sp_block_test('blowfish');
    select * from sp_block_test('khazad');
    select * from sp_block_test('rc5');
    select * from sp_block_test('rc6');
    select * from sp_block_test('"safer+"');
    select * from sp_block_test('twofish');
    select * from sp_block_test('xtea');

    -- 1.2 Stream cipher:
    select * from sp_stream_test('chacha20');
    select * from sp_stream_test('rc4');
    select * from sp_stream_test('sober128');


    -- 2. Auxiliary checks:
    -- ####################
    -- 2.1. "Counter length (CTR_LENGTH, bytes) may be specified only in CTR mode, default is the size of IV."
    select encrypt( 'fooriobar' using AES mode CTR key q'{A8586F1E-DB13-4D}' iv q'{D2FF255D-EDE3-44}' CTR_LENGTH -123 ) as ctr_clause_case_1 from rdb$database;
    select encrypt( 'fooriobar' using AES mode CTR key q'{A8586F1E-DB13-4D}' iv q'{D2FF255D-EDE3-44}' CTR_LENGTH    0 ) as ctr_clause_case_2 from rdb$database;
    select encrypt( 'fooriobar' using AES mode CTR key q'{A8586F1E-DB13-4D}' iv q'{D2FF255D-EDE3-44}' CTR_LENGTH   16 ) as ctr_clause_case_3 from rdb$database;
    select encrypt( 'fooriobar' using AES mode CTR key q'{A8586F1E-DB13-4D}' iv q'{D2FF255D-EDE3-44}' CTR_LENGTH  123 ) as ctr_clause_case_4 from rdb$database;
    select encrypt( 'fooriobar' using AES mode OFB key q'{A8586F1E-DB13-4D}' iv q'{D2FF255D-EDE3-44}' CTR_LENGTH   16 ) as ctr_clause_case_5 from rdb$database;

    -- 2.2. "Initial counter value (COUNTER) may be specified only for CHACHA20 cipher, default is 0."
    select encrypt( 'fooriobar' using CHACHA20 key q'{1110FB89-AD32-4E}'  iv q'{114E811E}' counter  0 ) from rdb$database;
    -- lead to crash, letter 11.11.2019 15:35 --> select encrypt( 'fooriobar' using CHACHA20 key q'{1110FB89-AD32-4E}'  iv q'{114E811E}' counter  cast(null as bigint) ) from rdb$database;
    select encrypt( 'fooriobar' using CHACHA20 key q'{1110FB89-AD32-4E}'  iv q'{114E811E}' counter  1 ) from rdb$database;
    select encrypt( 'fooriobar' using CHACHA20 key q'{1110FB89-AD32-4E}'  iv q'{114E811E}' counter -9223372036854775808 ) from rdb$database;
    select encrypt( 'fooriobar' using CHACHA20 key q'{1110FB89-AD32-4E}'  iv q'{114E811E}' counter 9223372036854775807 ) from rdb$database;


    -- 2.3. Following query led to crash, see letter to Alex, 30.12.2018 00:15
    -- Expected STDERR:
    -- Statement failed, SQLSTATE = 22023
    -- Invalid key length 9, need 16 or 32
    select encrypt('QweRtYUioP' using chacha20 key '192837465' iv '777555333') as invalid_params from rdb$database;


    -- 4. "Functions return BLOB when first argument is blob and varbinary for all text types."
    set sqlda_display on;
    with
    d as (
      select
          cast('Functions return BLOB when first argument is blob and varbinary for all text types.' as blob) as d_blob
         ,cast('Functions return BLOB when first argument is blob and varbinary for all text types.' as varchar(255) ) as d_char
         ,x'0154090759DF' as e_bin
      from rdb$database
    )
    select
         encrypt( d.d_blob using AES mode CTR key q'{A8586F1E-DB13-4D}' iv q'{D2FF255D-EDE3-44}' ) as e_blob
        ,encrypt( d.d_char using AES mode CTR key q'{A8586F1E-DB13-4D}' iv q'{D2FF255D-EDE3-44}' ) as e_char
        ,decrypt( d.e_bin using sober128 key 'AbcdAbcdAbcdAbcd' iv '01234567') as d_bin
    from d
    rows 0;
    set sqlda_display off;

"""

substitutions = [ ('table:.*', '') ] # [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'SYSTEM.'
    expected_stdout = f"""
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            11
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            11
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            11
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            11
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            11
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    11
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    11
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    11
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    11
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            AES
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    11
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            12
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            12
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            12
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            12
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            12
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    13
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    13
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    13
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    13
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            ANUBIS
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    13
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            13
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            13
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            13
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            13
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            13
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    15
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    15
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    15
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    15
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            BLOWFISH
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    15
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            14
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            14
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            14
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            14
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            14
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    17
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    17
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    17
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    17
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            KHAZAD
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    17
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            15
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            15
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            15
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            15
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            15
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    19
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    19
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    19
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    19
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            RC5
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    19
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            17
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            17
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            17
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            17
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            17
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    21
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    21
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    21
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    21
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            RC6
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    21
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            18
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            18
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            18
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            18
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            18
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    23
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    23
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    23
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    23
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            "SAFER+"
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    23
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            19
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            19
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            19
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            19
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            19
        ENC_INIT_VECTOR_OCTET_LENGTH    16
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    25
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    25
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    25
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    25
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            TWOFISH
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    25
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            20
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            20
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            20
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            20
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            20
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 CBC
        ENC_KEY_OCTET_LENGTH            26
        ENC_INIT_VECTOR_OCTET_LENGTH    27
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 CFB
        ENC_KEY_OCTET_LENGTH            26
        ENC_INIT_VECTOR_OCTET_LENGTH    27
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 CTR
        ENC_KEY_OCTET_LENGTH            26
        ENC_INIT_VECTOR_OCTET_LENGTH    27
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 ECB
        ENC_KEY_OCTET_LENGTH            26
        ENC_INIT_VECTOR_OCTET_LENGTH    27
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545224
        ENCRYPTION_ALGORITHM            XTEA
        ENCRYPTION_MODE                 OFB
        ENC_KEY_OCTET_LENGTH            26
        ENC_INIT_VECTOR_OCTET_LENGTH    27
        ENCRYPTED_EQUALS_TO_DECRYPTED   <null>
        ENCRYPTION_FINISH_GDSCODE       335545229
        ENCRYPTION_ALGORITHM            CHACHA20
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            CHACHA20
        ENC_KEY_OCTET_LENGTH            21
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       335545250
        ENCRYPTION_ALGORITHM            CHACHA20
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    29
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       335545240
        ENCRYPTION_ALGORITHM            RC4
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    0
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC4
        ENC_KEY_OCTET_LENGTH            22
        ENC_INIT_VECTOR_OCTET_LENGTH    0
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            RC4
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    0
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            SOBER128
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       <null>
        ENCRYPTION_ALGORITHM            SOBER128
        ENC_KEY_OCTET_LENGTH            23
        ENC_INIT_VECTOR_OCTET_LENGTH    8
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       335545230
        ENCRYPTION_ALGORITHM            SOBER128
        ENC_KEY_OCTET_LENGTH            16
        ENC_INIT_VECTOR_OCTET_LENGTH    33
        ENCRYPTED_EQUALS_TO_DECRYPTED   <true>
        ENCRYPTION_FINISH_GDSCODE       335545230
        Statement failed, SQLSTATE = 22023
        Too big counter value -123, maximum 16 can be used
        CTR_CLAUSE_CASE_2               E813A50C069FC418AA
        CTR_CLAUSE_CASE_3               E813A50C069FC418AA
        Statement failed, SQLSTATE = 22023
        Too big counter value 123, maximum 16 can be used
        Statement failed, SQLSTATE = 22023
        Counter length/value parameter is not used with mode OFB
        ENCRYPT                         8E709DDA89912F172C
        ENCRYPT                         BC3604C147B53D3BDD
        ENCRYPT                         C8051FB1A2581EA9A1
        ENCRYPT                         2E2298CF4C2B81AD54
        Statement failed, SQLSTATE = 22023
        Invalid key length 9, need 16 or 32
        INPUT message field count: 0
        OUTPUT message field count: 3
        01: sqltype: 520 BLOB scale: 0 subtype: 0 len: 8
        :  name: ENCRYPT  alias: E_BLOB
        : table:   owner:
        02: sqltype: 448 VARYING scale: 0 subtype: 0 len: 255 charset: 1 {SQL_SCHEMA_PREFIX}OCTETS
        :  name: ENCRYPT  alias: E_CHAR
        : table:   owner:
        03: sqltype: 448 VARYING scale: 0 subtype: 0 len: 6 charset: 1 {SQL_SCHEMA_PREFIX}OCTETS
        :  name: DECRYPT  alias: D_BIN
        : table:   owner:
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
