#coding:utf-8

"""
ID:          issue-5941
ISSUE:       5941
TITLE:       isc_vax_integer() and isc_portable_integer() work wrongly with short negative numbers
DESCRIPTION:
    Confirmed bug on4.0.0.800.
    Works fine on:
         FB25SC, build 2.5.8.27089: OK, 0.422s.
         FB30SS, build 3.0.3.32876: OK, 1.484s.
         FB40SS, build 4.0.0.852: OK, 1.156s.

    NB. It seems that some bug exists in function _renderSizedIntegerForSPB from fdb package (services.py):
       iRaw = struct.pack(myformat, i)
       iConv = api.isc_vax_integer(iRaw, len(iRaw))
    This function cuts off high 4 bytes when we pass to it bugint values greater than 2^31, i.e.:
    2147483648L  ==> reversed = b'\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00'
    -2147483649L ==> reversed = b'\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00'

    For this reason it was decided currently to limit scope by specifying numbers with abs() less than 2^31 - untill fdb driver will be fixed.
    See letter from dimitr 08-jan-2018 20:56

    25.08.2020: adjusted name of function from services that must work here:
    its name is "_render_sized_integer_for_spb" rather than old "_renderSizedIntegerForSPB".
    Checked on 4.0.0.2173; 3.0.7.33357; 2.5.9.27152.
NOTES:
[25.1.2022] pcisar
  Required function _renderSizedIntegerForSPB() is not present in firebird-driver.
JIRA:        CORE-5675
FBTEST:      bugs.core_5675
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    Try revert bytes in decimal value:            1  using struct format:  "b" ; result:  01
    Try revert bytes in decimal value:           -1  using struct format:  "b" ; result:  ff
    Try revert bytes in decimal value:          127  using struct format:  "b" ; result:  7f
    Try revert bytes in decimal value:         -128  using struct format:  "b" ; result:  80
    Try revert bytes in decimal value:          128  using struct format:  "b" ; result:  byte format requires -128 <= number <= 127
    Try revert bytes in decimal value:         -256  using struct format:  "B" ; result:  ubyte format requires 0 <= number <= 255
    Try revert bytes in decimal value:          255  using struct format:  "B" ; result:  ubyte format requires 0 <= number <= 255
    Try revert bytes in decimal value:       -32768  using struct format:  "h" ; result:  0080
    Try revert bytes in decimal value:        32767  using struct format:  "h" ; result:  ff7f
    Try revert bytes in decimal value:        32768  using struct format:  "h" ; result:  short format requires SHRT_MIN <= number <= SHRT_MAX
    Try revert bytes in decimal value:       -65536  using struct format:  "H" ; result:  ushort format requires 0 <= number <= USHRT_MAX
    Try revert bytes in decimal value:        65535  using struct format:  "H" ; result:  ushort format requires 0 <= number <= USHRT_MAX
    Try revert bytes in decimal value:        65536  using struct format:  "H" ; result:  ushort format requires 0 <= number <= USHRT_MAX
"""

@pytest.mark.skip('FIXME: See notes')
@pytest.mark.version('>=3')
def test_1(act: Action):
    pytest.fail("Not IMPLEMENTED")

# test_script_1
#---
#
#  from __future__ import print_function
#  import os
#  import binascii
#  from fdb import services
#  os.environ["ISC_USER"] = user_name
#  os.environ["ISC_PASSWORD"] = user_password
#  db_conn.close()
#  con = services.connect(host='localhost', user='sysdba', password='masterkey')
#  #print( con.get_server_version() )
#
#  dec_values=(  1,     -1,   127,  -128,   128,  -256, 255,  -32768, 32767, 32768, -65536, 65535, 65536 ) #,    32767, -32768, 32768, -32769,  2147483647, -2147483648, 2147483648, -2147483649, 3000000000, 3000000000000, 9223372036854775807 )
#  num_ctypes=( 'b',   'b',   'b',   'b',   'b',   'B',  'B',    'h',   'h',   'h',    'H',   'H',   'H' ) #,     'i',    'i',   'i',    'i',         'i',         'i',         'q',        'q',        'q',          'q',                  'q' )
#
#  #dec_values=(  1,     -1,   127,  -128,   128,  -256, 255,  -32768, 32767, 32768, -65536, 65535, 65536 ,    32767, -32768, 32768, -32769,  2147483647, -2147483648, 2147483648, -2147483649, 3000000000, 3000000000000, 9223372036854775807 )
#  #num_ctypes=( 'b',   'b',   'b',   'b',   'b',   'B',  'B',    'h',   'h',   'h',    'H',   'H',   'H' ,      'i',    'i',   'i',    'i',         'i',         'i',        'q',         'q',        'q',           'q',                  'q')
#
#
#  for i in range(0, len(dec_values)):
#     num = dec_values[i]
#     fmt = num_ctypes[i]
#     msg = ['Try revert bytes in decimal value:', "{:12d}".format(num), ' using struct format: ', '"'+fmt+'"']
#     try:
#         # OLD name of function: (numericFormat, numericBytes) = services._renderSizedIntegerForSPB(num, fmt)
#         (numericFormat, numericBytes) = services._render_sized_integer_for_spb(num, fmt)
#         rev = binascii.hexlify(numericBytes)
#     except Exception, e:
#         rev = e[0]
#     finally:
#         print( ' '.join( (msg + ['; result: ',rev,]) ) )
#
#  con.close()
#
#---
