#coding:utf-8

"""
ID:          issue-4756
ISSUE:       4756
TITLE:       Support for different hash algorithms in HASH system function
DESCRIPTION:
  Test verifies only:
  1) ability to use syntax: hash(<string> using <algo>)
  2) non-equality of hash results for sha1, sha256 and sha512 using _TRIVIAL_ sample from ticket.
  build 4.0.0.713: OK, 1.094s.

  Note that for strings:
  '20177527e04e05d5e7b448c1ab2b872f86831d0b' and '20177527e04e05d5e7b448c1ab2b872f86831d0b'
  - current imlpementation of SHA1, SHA256 and SHA512 gives the same hash value.
  (See: https://stackoverflow.com/questions/3475648/sha1-collision-demo-example )
NOTES:
[27.08.2020]
  Since build 4.0.0.2180 we have to use *new* function for cryptograpic hashes
  when addition argument for algorithm is used: crypt_hash().
JIRA:        CORE-4436
FBTEST:      bugs.core_4436
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table test(s varchar(32765));
    commit;
    insert into test(s) values('4-20100433-01775-LOTES');
    insert into test(s) values('1-20100433-01765-LOTES');
    commit;

    select -(count(s) - count(distinct hash(s))) as hash_default_result from test;

    select -(count(s) - count(distinct crypt_hash(s using sha1))) as hash_sha1_result from test;
    select -(count(s) - count(distinct crypt_hash(s using sha256))) as hash_sha256_result from test;
    select -(count(s) - count(distinct crypt_hash(s using sha512))) as hash_sha512_result from test;

    /*
    Used before 27.08.2020:
    select -(count(s) - count(distinct hash(s using sha1))) as hash_sha1_result from test;
    select -(count(s) - count(distinct hash(s using sha256))) as hash_sha256_result from test;
    select -(count(s) - count(distinct hash(s using sha512))) as hash_sha512_result from test;
    */
"""

act = isql_act('db', test_script)

expected_stdout = """
    HASH_DEFAULT_RESULT             -1
    HASH_SHA1_RESULT                0
    HASH_SHA256_RESULT              0
    HASH_SHA512_RESULT              0
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

