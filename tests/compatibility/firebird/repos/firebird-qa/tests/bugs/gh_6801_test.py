#coding:utf-8

"""
ID:          issue-6801
ISSUE:       6801
TITLE:       Error recompiling a package with some combination of nested functions
DESCRIPTION:
  FB crashes on attempt to recompile a package with some combination of nested functions.
  Confirmed crash on 4.0.0.2506; 5.0.0.60
FBTEST:      bugs.gh_6801
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set heading off;
    select 'run-1' as msg from rdb$database;

    set term ^ ;
    create or alter package pkg_test as
    begin
      function get_usr_name()
       returns varchar(30);

      function get_usr_id()
       returns bigint;

      procedure create_usr(p_user_name varchar(30))
        returns (p_id bigint);

    end
    ^

    recreate package body pkg_test as
    begin
      function get_usr_name()
       returns varchar(30)
      as
      begin
        return 'TEST_USER';
      end

      function get_usr_id()
       returns bigint
      as
        declare variable v_sess_usr_name varchar(30);
      begin
        v_sess_usr_name = get_usr_name();
        return 123;

      end

      procedure create_usr(p_user_name varchar(30))
        returns (p_id bigint)
      as
        declare variable v_sess_usr_id bigint;
      begin
        v_sess_usr_id = pkg_test.get_usr_id();
      end

    end
    ^

    select 'run-2' as msg from rdb$database
    ^

    create or alter package pkg_test as
    begin
      function get_usr_name()
       returns varchar(30);

      function get_usr_id()
       returns bigint;

      procedure create_usr(p_user_name varchar(30))
        returns (p_id bigint);

    end^

    recreate package body pkg_test as
    begin
      function get_usr_name()
       returns varchar(30)
      as
      begin
        return 'TEST_USER';
      end

      function get_usr_id()
       returns bigint
      as
        declare variable v_sess_usr_name varchar(30);
      begin
        -- No crash in both cases: with or without package name:
        v_sess_usr_name = get_usr_name();
        return 123;
      end

      procedure create_usr(p_user_name varchar(30))
        returns (p_id bigint)
      as
        declare variable v_sess_usr_id bigint;
      begin
        -- This call led FB to crash, regardless on specifiyng prefix of package ('pkg_test'):
        v_sess_usr_id = pkg_test.get_usr_id();
        -- v_sess_usr_id = get_usr_id();
      end

    end
    ^
    select 'completed' as msg from rdb$database
    ^
"""

act = isql_act('db', test_script)

expected_stdout = """
    run-1
    run-2
    completed
"""

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
