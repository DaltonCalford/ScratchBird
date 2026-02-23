#coding:utf-8

"""
ID:          gtcs.isql-show-command-ambiguity
TITLE:       SHOW TABLE / VIEW: ambiguity between tables and views
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_22.script

  bug #223513 ambiguity between tables and views
FBTEST:      functional.gtcs.isql_show_command_ambiguity
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table a_table(a int);
    create view "a_table" as select a from a_table;

    create table "a_view"(a int);
    create view a_view as select a from "a_view";

    set echo on;
    show tables;
    show views;

    show table a_view;
    show table "a_view";

    show table a_table;
    show table "a_table";

    show view a_view;
    show view "a_view";
"""

act = isql_act('db', test_script, substitutions=[('=', ''), ('[ \t]+', ' ')])

@pytest.mark.version('>=3')
def test_1(act: Action):

    expected_stdout_3x = f"""
        show tables;
        A_TABLE a_view

        show views;
        A_VIEW a_table

        show table a_view;
        There is no table A_VIEW in this database

        show table "a_view";
        A INTEGER Nullable
        
        show table a_table;
        A INTEGER Nullable
        
        show table "a_table";
        There is no table a_table in this database
        
        show view a_view;
        A INTEGER Nullable
        View Source:
        select a from "a_view"
        
        show view "a_view";
        There is no view a_view in this database
    """

    expected_stdout_5x = f"""
        show tables;
        A_TABLE
        a_view

        show views;
        A_VIEW
        a_table
        
        show table a_view;
        There is no table A_VIEW in this database
        
        show table "a_view";
        A INTEGER Nullable
        
        show table a_table;
        A INTEGER Nullable
        
        show table "a_table";
        There is no table a_table in this database
        
        show view a_view;
        A INTEGER Nullable
        View Source:
        select a from "a_view"
        
        show view "a_view";
        There is no view a_view in this database
    """

    expected_stdout_6x = f"""
        show tables;
        PUBLIC.A_TABLE
        PUBLIC."a_view"

        show views;
        PUBLIC.A_VIEW
        PUBLIC."a_table"

        show table a_view;
        There is no table A_VIEW in this database

        show table "a_view";
        Table: PUBLIC."a_view"
        A INTEGER Nullable

        show table a_table;
        Table: PUBLIC.A_TABLE
        A INTEGER Nullable

        show table "a_table";
        There is no table "a_table" in this database

        show view a_view;
        View: PUBLIC.A_VIEW
        A INTEGER Nullable
        View Source:
        select a from "a_view"

        show view "a_view";
        There is no view "a_view" in this database
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
