#coding:utf-8

"""
ID:          issue-2410
ISSUE:       2410
TITLE:       Non-SYSDBA user can change FW mode of database
DESCRIPTION: Original (fbtest+fdb) implementation notes:
  We create common user using Services API and try to establish TWO subsequent connections under his login:
  1) with flag 'forced_write' = 1 and then
  2) with flag 'no_reserve' = 1.
  Note: both values of these flags have to be equal 1 because of some specifics of DPB building inside fdb driver:
  value 0 means 'nothing to add', so no error will be raised in this case (and we DO expect error in this ticket).
  In WI-V2.1.1.17910 one may to specify *ANY* values of these flags and NO error will be raised.
  Fortunately, actual DB state also was not changed.

  Starting from WI-V2.1.2.18118 attempt to specify non-zero flag leads to runtime exception with SQLCODE=-901
  ("unable to perform: you must be either SYSDBA or owner...")
  See also: https://firebirdsql.org/rlsnotesh/rnfb210-apiods.html

  Additional filtering of output is required because of different error message in 4.0: it checks whether current user
  has grant of role with system privilege 'CHANGE_HEADER_SETTINGS'.
  If no then message will be "System privilege CHANGE_HEADER_SETTINGS is missing" (differ from older FB versions).
  If yes then DB header is allowed to be change and NO ERROR at all will be raised on attempt to establish such connections.
  For that reason it was decided to completely suppress output of error detalization ("you must be either SYSDBA" or
  "System privilege CHANGE_HEADER_SETTINGS is missing") and to display only line with SQLCODE.
NOTES:
[20.1.2022] pcisar
  This test is not possible to implement with new Python driver as it does not
  allow to specify `forced_writes` or `reserve_space` options on connect() as tested
  configuration options are passed to DPB only for create_database()!
  This is intentional change in firebird-driver from fdb, as using these DPB options
  on connect has side-effects (changes database option) which was considered dangerous.
JIRA:        CORE-1972
FBTEST:      bugs.core_1972
"""

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect

db = db_factory()

tmp_user = user_factory('db', name='TMP$C1972', password='123')

act = python_act('db', substitutions=[('^((?!Successfully|Trying_to_establish|SQLCODE:).)*$', '')])

expected_stdout = """
    Successfully added non-privileged user

    Trying_to_establish connection with specifying force_write
    - SQLCODE: -901

    Trying_to_establish connection with specifying no_reserve
    - SQLCODE: -901

    Successfully removed non-privileged user
    Successfully finished script
"""

@pytest.mark.skip("Can't be implement with new Python driver")
@pytest.mark.version('>=3')
def test_1(act: Action, tmp_user: User):
    pass

# test_script_1
#---
#
#  import os
#  import fdb
#  from fdb import services
#  os.environ["ISC_USER"] = user_name
#  os.environ["ISC_PASSWORD"] = user_password
#
#  # Obtain engine version:
#  engine = str(db_conn.engine_version) # convert to text because 'float' object has no attribute 'startswith'
#  db_conn.close()
#
#  FB_PORT= '' #  '/3212'
#  A_USER = 'TMP$C1972'
#  A_PSWD = '123'
#
#  con=None
#  try:
#      con = services.connect(host='localhost'+FB_PORT, user='SYSDBA', password='masterkey')
#      usr = services.User(A_USER)
#      usr.password = A_PSWD
#      con.add_user(usr)
#      print('Successfully added non-privileged user')
#  finally:
#      if con:
#          con.close()
#  #-------------------------------------------------------------------------------------------
#
#  # 1. Try to specifying 'force_write' flag: no errors and NO changes in 2.1.1; error in 2.1.2 and above:
#  try:
#      print( 'Trying_to_establish connection with specifying force_write'  )
#      con = fdb.connect(dsn = dsn, user = A_USER, password = A_PSWD, force_write = 1 )
#      print ( 'done', con.firebird_version )
#      cur=con.cursor()
#      cur.execute('select current_user,mon$forced_writes from mon$database')
#      for r in cur:
#         print('whoami:', r[0], '; mon$forced_writes:', r[1])
#  except Exception, e:
#      print(e[0])
#  finally:
#      if con:
#          con.close()
#
#  # 2. Try to specifying 'no_reserve' flag: no errors and NO changes in 2.1.1; error in 2.1.2 and above:
#  try:
#      print( 'Trying_to_establish connection with specifying no_reserve'  )
#      con = fdb.connect(dsn = dsn, user = A_USER, password = A_PSWD, no_reserve = 1 )
#      print ( 'done', con.firebird_version )
#      cur=con.cursor()
#      cur.execute('select current_user,mon$reserve_space from mon$database')
#      for r in cur:
#         print('whoami:', r[0], '; mon$reserve_space:', r[1])
#  except Exception, e:
#      print(e[0])
#  finally:
#      if con:
#          con.close()
#
#  #-------------------------------------------------------------------------------------------
#  try:
#      con = services.connect(host='localhost' + FB_PORT, user='SYSDBA', password='masterkey')
#      con.remove_user(A_USER)
#      print('Successfully removed non-privileged user')
#  finally:
#      if con:
#          con.close()
#
#  print('Successfully finished script')
#
#
#---
