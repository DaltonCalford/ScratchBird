#coding:utf-8

"""
ID:          issue-6930
ISSUE:       6930
TITLE:       Segfault when calling crypto functions
DESCRIPTION:
  Using NULL or empty string as a KEY parameter in RSA-functions leads to segfault.
FBTEST:      bugs.gh_6930
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
	set list on;

	recreate table rsa(
		text_unencrypted varchar(256)
	   ,k_prv varbinary(16384)
	   ,k_pub varbinary(8192)
	   ,test_encrypted varbinary(16384)
	   ,text_rsa_sign varchar(8192)
	   ,text_rsa_vrfy boolean
	   ,text_encrypted varchar(256)
	   ,text_decrypted varchar(256)
	);
	commit;

	insert into rsa(text_unencrypted) values('lorem ipsum');

	update rsa set k_prv = rsa_private(256);
	update rsa set k_pub = rsa_public(k_prv);

	update rsa set test_encrypted = crypt_hash(text_unencrypted using sha256);

	set term ^;

	----------------------------------- r s a _ s i g n _ h a s h ------------------------------

	execute block returns( rsa_sign_sqlstate_list varchar(255), rsa_sign_gdscode_list varchar(255), rsa_sign_octet_length int ) as
	begin

		rsa_sign_sqlstate_list = '';
		rsa_sign_gdscode_list = '';

		begin
			-- Following statement caused FB crash; expected: "SQLSTATE = 22023 / Empty or NULL ... is not accepted"
			update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha256) key null hash sha256);
			when any do
				begin
					rsa_sign_sqlstate_list = rsa_sign_sqlstate_list || sqlstate || ',';
					rsa_sign_gdscode_list = rsa_sign_gdscode_list || gdscode || ',' ;
				end
		end

		begin
			-- Following statement caused FB crash; expected: "SQLSTATE = 22023 / Empty or NULL ... is not accepted"
			update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha256) key '' hash sha256);
			when any do
				begin
					rsa_sign_sqlstate_list = rsa_sign_sqlstate_list || sqlstate || ',';
					rsa_sign_gdscode_list = rsa_sign_gdscode_list || gdscode || ',' ;
				end
		end

		-- This must execute normally:
		update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha256) key k_prv hash sha256)
		returning octet_length(text_rsa_sign) into rsa_sign_octet_length
		;

		suspend;
	end
	^

	------------------------------------- r s a _ v e r i f y ------------------------------

	execute block returns( rsa_verify_sqlstate_list varchar(255), rsa_verify_gdscode_list varchar(255), text_rsa_vrfy boolean ) as
	begin

		rsa_verify_sqlstate_list = '';
		rsa_verify_gdscode_list = '';

		begin
			-- Following statement caused FB crash; expected: "SQLSTATE = 22023 / Empty or NULL ... is not accepted"
			update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha256) signature text_rsa_sign key null hash sha256);
			when any do
				begin
					rsa_verify_sqlstate_list = rsa_verify_sqlstate_list || sqlstate || ',';
					rsa_verify_gdscode_list = rsa_verify_gdscode_list || gdscode || ',' ;
				end
		end

		begin
			-- Following statement caused FB crash; expected: "SQLSTATE = 22023 / Empty or NULL ... is not accepted"
			update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha256) signature text_rsa_sign key '' hash sha256);
			when any do
				begin
					rsa_verify_sqlstate_list = rsa_verify_sqlstate_list || sqlstate || ',';
					rsa_verify_gdscode_list = rsa_verify_gdscode_list || gdscode || ',' ;
				end
		end

		-- This must execute normally:
		update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha256) signature text_rsa_sign key k_pub hash sha256)
		returning text_rsa_vrfy into text_rsa_vrfy
		;

		suspend;
	end
	^

	------------------------------------- r s a _ e n c r y p t ----------------------------

	execute block returns( rsa_encrypt_sqlstate_list varchar(255), rsa_encrypt_gdscode_list varchar(255), rsa_encrypted_octet_length int ) as
	begin

		rsa_encrypt_sqlstate_list = '';
		rsa_encrypt_gdscode_list = '';

		begin
			-- Following statement caused FB crash; expected: "SQLSTATE = 22023 / Empty or NULL ... is not accepted"
			update rsa set text_encrypted = rsa_encrypt(text_unencrypted key null hash sha256);
			when any do
				begin
					rsa_encrypt_sqlstate_list = rsa_encrypt_sqlstate_list || sqlstate || ',';
					rsa_encrypt_gdscode_list = rsa_encrypt_gdscode_list || gdscode || ',' ;
				end
		end

		begin
			-- Following statement caused FB crash; expected: "SQLSTATE = 22023 / Empty or NULL ... is not accepted"
			update rsa set text_encrypted = rsa_encrypt(text_unencrypted key '' hash sha256);
			when any do
				begin
					rsa_encrypt_sqlstate_list = rsa_encrypt_sqlstate_list || sqlstate || ',';
					rsa_encrypt_gdscode_list = rsa_encrypt_gdscode_list || gdscode || ',' ;
				end
		end

		-- This must execute normally:
		update rsa set text_encrypted = rsa_encrypt(text_unencrypted key k_pub hash sha256)
			returning octet_length(text_encrypted) into rsa_encrypted_octet_length
		;

		suspend;
	end
	^

	------------------------------------- r s a _ d e c r y p t ----------------------------

	execute block returns( rsa_decrypt_sqlstate_list varchar(255), rsa_decrypt_gdscode_list varchar(255), rsa_text_decrypted type of column rsa.text_unencrypted ) as
	begin

		rsa_decrypt_sqlstate_list = '';
		rsa_decrypt_gdscode_list = '';

		begin
			-- Following statement caused FB crash; expected: "SQLSTATE = 22023 / Empty or NULL ... is not accepted"
			update rsa set text_decrypted = rsa_decrypt(text_encrypted key null hash sha256);
			when any do
				begin
					rsa_decrypt_sqlstate_list = rsa_decrypt_sqlstate_list || sqlstate || ',';
					rsa_decrypt_gdscode_list = rsa_decrypt_gdscode_list || gdscode || ',' ;
				end
		end

		begin
			-- Following statement caused FB crash; expected: "SQLSTATE = 22023 / Empty or NULL ... is not accepted"
			update rsa set text_decrypted = rsa_decrypt(text_encrypted key '' hash sha256);
			when any do
				begin
					rsa_decrypt_sqlstate_list = rsa_decrypt_sqlstate_list || sqlstate || ',';
					rsa_decrypt_gdscode_list = rsa_decrypt_gdscode_list || gdscode || ',' ;
				end
		end

		-- This must execute normally:
		update rsa set text_decrypted = rsa_decrypt(text_encrypted key k_prv hash sha256)
			returning text_decrypted into rsa_text_decrypted
		;

		suspend;
	end
	^
	set term ;^

	select text_unencrypted, text_decrypted from rsa;

"""

act = isql_act('db', test_script)

expected_stdout = """
	RSA_SIGN_SQLSTATE_LIST          22023,22023,
	RSA_SIGN_GDSCODE_LIST           335545276,335545276,
	RSA_SIGN_OCTET_LENGTH           256

	RSA_VERIFY_SQLSTATE_LIST        22023,22023,
	RSA_VERIFY_GDSCODE_LIST         335545276,335545276,
	TEXT_RSA_VRFY                   <true>

	RSA_ENCRYPT_SQLSTATE_LIST       22023,22023,
	RSA_ENCRYPT_GDSCODE_LIST        335545276,335545276,
	RSA_ENCRYPTED_OCTET_LENGTH      256

	RSA_DECRYPT_SQLSTATE_LIST       22023,22023,
	RSA_DECRYPT_GDSCODE_LIST        335545276,335545276,
	RSA_TEXT_DECRYPTED              lorem ipsum

	TEXT_UNENCRYPTED                lorem ipsum
	TEXT_DECRYPTED                  lorem ipsum
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
