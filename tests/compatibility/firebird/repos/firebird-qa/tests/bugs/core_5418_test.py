#coding:utf-8

"""
ID:          issue-5691
ISSUE:       5691
TITLE:       Inconsistent output when retrieving the server log via Services API
DESCRIPTION:
  Test gets FB home directory and copies firebird.log to *.tmp.
  Then it makes firebird.log empty and retrieves it via services API. Output should be empty.
  Finally, it opens firebird.log and adds to it several empty newlines.
  Repeat retrieveing content - it also should not contain any characters except newline.
NOTES:
[7.12.2021] pcisar
  Test requires direct changes to firebird.log which could require elevated privileges!
JIRA:        CORE-5418
FBTEST:      bugs.core_5418
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.skip('FIXME: direct write to firebird.log')
@pytest.mark.version('>=3')
def test_1(act: Action):
    pytest.fail("Not IMPLEMENTED")

# test_script_1
#---
#
#  import os
#  from fdb import services
#  import subprocess
#  import time
#  import shutil
#  import difflib
#  import re
#
#  os.environ["ISC_USER"] = user_name
#  os.environ["ISC_PASSWORD"] = user_password
#
#  engine = str(db_conn.engine_version) # convert to text because 'float' object has no attribute 'startswith'
#  db_conn.close()
#
#  svc = services.connect(host='localhost')
#  fb_home=svc.get_home_directory()
#  svc.close()
#
#  #--------------------------------------------
#
#  def flush_and_close( file_handle ):
#      # https://docs.python.org/2/library/os.html#os.fsync
#      # If you're starting with a Python file object f,
#      # first do f.flush(), and
#      # then do os.fsync(f.fileno()), to ensure that all internal buffers associated with f are written to disk.
#      global os
#
#      file_handle.flush()
#      if file_handle.mode not in ('r', 'rb') and file_handle.name != os.devnull:
#          # otherwise: "OSError: [Errno 9] Bad file descriptor"!
#          os.fsync(file_handle.fileno())
#      file_handle.close()
#
#  #--------------------------------------------
#
#  def cleanup( f_names_list ):
#      global os
#      for i in range(len( f_names_list )):
#         if type(f_names_list[i]) == file:
#            del_name = f_names_list[i].name
#         elif type(f_names_list[i]) == str:
#            del_name = f_names_list[i]
#         else:
#            print('Unrecognized type of element:', f_names_list[i], ' - can not be treated as file.')
#            del_name = None
#
#         if del_name and os.path.isfile( del_name ):
#             os.remove( del_name )
#
#  #--------------------------------------------
#
#  if engine.startswith('2.5'):
#      get_firebird_log_key='action_get_ib_log'
#  else:
#      get_firebird_log_key='action_get_fb_log'
#
#  fb_log = os.path.join( fb_home, 'firebird.log' )
#  fb_bak = os.path.join( fb_home, 'firebird.tmp' )
#  shutil.copy2( fb_log, fb_bak )
#  open(fb_log, 'w').close()
#
#  f_init_log = open( os.path.join(context['temp_directory'],'tmp_5418_old.log'), 'w')
#  subprocess.call( [ context['fbsvcmgr_path'],"localhost:service_mgr",  get_firebird_log_key ], stdout = f_init_log, stderr = subprocess.STDOUT)
#  flush_and_close( f_init_log )
#
#  f = open(fb_log, 'w')
#  for i in range(0,10):
#      f.write(os.linesep)
#  flush_and_close( f )
#
#  f_curr_log = open( os.path.join(context['temp_directory'],'tmp_5418_new.log'), 'w')
#  subprocess.call( [ context['fbsvcmgr_path'],"localhost:service_mgr",  get_firebird_log_key ], stdout = f_curr_log, stderr = subprocess.STDOUT )
#  flush_and_close( f_curr_log )
#
#  shutil.move( fb_bak, fb_log )
#
#  f_init_log=open(f_init_log.name, 'r')
#  f_curr_log=open(f_curr_log.name, 'r')
#  difftext = ''.join(difflib.unified_diff(
#      f_init_log.readlines(),
#      f_curr_log.readlines()
#    ))
#  f_init_log.close()
#  f_curr_log.close()
#
#  f_diff_txt=open( os.path.join(context['temp_directory'],'tmp_5418_diff.txt'), 'w')
#  f_diff_txt.write(difftext)
#  flush_and_close( f_diff_txt )
#
#  p = re.compile('\\+\\s?\\S+')
#  with open( f_diff_txt.name,'r') as f:
#      for line in f:
#          if line.startswith('+') and line.strip() != '+++' and p.search(line):
#              print( 'UNEXPECTED content in firebird.log: %(line)s' % locals() )
#
#  # Cleanup.
#  ##########
#  time.sleep(1)
#  cleanup( (f_init_log,f_curr_log,f_diff_txt) )
#
#
#---
