#coding:utf-8

"""
ID:          issue-7749
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7749
TITLE:       Fix character set and collation output
DESCRIPTION:
    Discussed with Alex, Dmitry Yemanov and Dimitry Sibiryakov, 
    sibject relates to https://github.com/FirebirdSQL/firebird/pull/7763
    See commit https://github.com/FirebirdSQL/firebird/commit/8dbdc2ae3f14104f3778cb2e61e680769a105df8
    ("Separate charset output: full for SHOW and minimal for EXTRACT").
NOTES:
    [03.10.2023] pzotov
        Checked on 6.0.0.66 (Intermediate build).
    [14.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

substitutions = [ ('^((?!(SQLSTATE|IMPLICIT|EXPLICIT|BINARY)).)*$', ''), ]
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    # test N1: check output of SHOW commands:
    # --------
    test_sql = """
        set bail on;

        ALTER CHARACTER SET UTF8 SET DEFAULT COLLATION unicode_ci;
        COMMIT;

        set term ^;

        create domain dm_vc_default_cset_implicit_coll as varchar(10)
        ^
        create domain dm_vc_default_cset_explicit_coll varchar(10) collate utf8
        ^
        create domain dm_vc_nondef_cset_implicit_coll as varchar(10) character set win1251
        ^
        create domain dm_vc_nondef_cset_explicit_coll as varchar(10) character set win1251 collate pxw_cyrl
        ^
        create domain dm_nc_fixed_char_implicit_coll nchar(10)
        ^
        create domain dm_nc_fixed_char_explicit_coll nchar(10) collate fr_fr
        ^
        create domain dm_bt_default_cset_implicit_coll as blob sub_type text
        ^
        create domain dm_bt_default_cset_explicit_coll blob sub_type text collate utf8
        ^
        create domain dm_bt_nondef_cset_implicit_coll as blob sub_type text character set win1251
        ^
        create domain dm_bt_nondef_cset_explicit_coll as blob sub_type text character set win1251 collate pxw_cyrl
        ^
        create domain dm_blob_binary blob sub_type binary
        ^
        
        ------------------------------------------------------------------------------------------------------------------
        recreate table test (
            vc_default_cset_implicit_coll varchar(10) -- default charset, default collation
            ,vc_default_cset_explicit_coll varchar(10) collate utf8 -- default charset, non-default collation
            ,vc_nondef_cset_implicit_coll varchar(10) character set win1251 -- non-default charset, default collation
            ,vc_nondef_cset_explicit_coll varchar(10) character set win1251 collate pxw_cyrl -- non-default charset, non-default collation
            ,nc_fixed_char_implicit_coll nchar(10)
            ,nc_fixed_char_explicit_coll nchar(10) collate fr_fr
            ,bt_default_cset_implicit_coll blob sub_type text
            ,bt_default_cset_explicit_coll blob sub_type text collate utf8
            ,bt_nondef_cset_implicit_coll blob sub_type text character set win1251
            ,bt_nondef_cset_explicit_coll blob sub_type text character set win1251 collate pxw_cyrl
            ,blob_binary blob sub_type binary
        )
        ^
        ------------------------------------------------------------------------------------------------------------------
        create procedure sp_test(
             a_vc_default_cset_implicit_coll varchar(10) -- default charset, default collation
            ,a_vc_default_cset_explicit_coll varchar(10) collate utf8 -- default charset, non-default collation
            ,a_vc_nondef_cset_implicit_coll varchar(10) character set win1251 -- non-default charset, default collation
            ,a_vc_nondef_cset_explicit_coll varchar(10) character set win1251 collate pxw_cyrl -- non-default charset, non-default collation
            ,a_nc_fixed_char_implicit_coll nchar(10)
            ,a_nc_fixed_char_explicit_coll nchar(10) collate fr_fr
            ,a_bt_default_cset_implicit_coll blob sub_type text
            ,a_bt_default_cset_explicit_coll blob sub_type text collate utf8
            ,a_bt_nondef_cset_implicit_coll blob sub_type text character set win1251
            ,a_bt_nondef_cset_explicit_coll blob sub_type text character set win1251 collate pxw_cyrl
            ,a_blob_binary blob sub_type binary
        ) returns (
             o_vc_default_cset_implicit_coll varchar(10) -- default charset, default collation
            ,o_vc_default_cset_explicit_coll varchar(10) collate utf8 -- default charset, non-default collation
            ,o_vc_nondef_cset_implicit_coll varchar(10) character set win1251 -- non-default charset, default collation
            ,o_vc_nondef_cset_explicit_coll varchar(10) character set win1251 collate pxw_cyrl -- non-default charset, non-default collation
            ,o_nc_fixed_char_implicit_coll nchar(10)
            ,o_nc_fixed_char_explicit_coll nchar(10) collate fr_fr
            ,o_bt_default_cset_implicit_coll blob sub_type text
            ,o_bt_default_cset_explicit_coll blob sub_type text collate utf8
            ,o_bt_nondef_cset_implicit_coll blob sub_type text character set win1251
            ,o_bt_nondef_cset_explicit_coll blob sub_type text character set win1251 collate pxw_cyrl
            ,o_blob_binary blob sub_type binary
        )
        as
        begin
        end
        ^
        ------------------------------------------------------------------------------------------------------------------
        create function fn_test(
             a_vc_default_cset_implicit_coll varchar(10) -- default charset, default collation
            ,a_vc_default_cset_explicit_coll varchar(10) collate utf8 -- default charset, non-default collation
            ,a_vc_nondef_cset_implicit_coll varchar(10) character set win1251 -- non-default charset, default collation
            ,a_vc_nondef_cset_explicit_coll varchar(10) character set win1251 collate pxw_cyrl -- non-default charset, non-default collation
            ,a_nc_fixed_char_implicit_coll nchar(10)
            ,a_nc_fixed_char_explicit_coll nchar(10) collate fr_fr
            ,a_bt_default_cset_implicit_coll blob sub_type text
            ,a_bt_default_cset_explicit_coll blob sub_type text collate utf8
            ,a_bt_nondef_cset_implicit_coll blob sub_type text character set win1251
            ,a_bt_nondef_cset_explicit_coll blob sub_type text character set win1251 collate pxw_cyrl
            ,a_blob_binary blob sub_type binary
        ) returns dm_vc_default_cset_explicit_coll collate unicode_ci_ai as
        begin
            return 1;
        end
        ^
        ------------------------------------------------------------------------------------------------------------------
        create package pg_test as
        begin
            procedure pg_sp (
                 a_vc_default_cset_implicit_coll varchar(10) -- default charset, default collation
                ,a_vc_default_cset_explicit_coll varchar(10) collate utf8 -- default charset, non-default collation
                ,a_vc_nondef_cset_implicit_coll varchar(10) character set win1251 -- non-default charset, default collation
                ,a_vc_nondef_cset_explicit_coll varchar(10) character set win1251 collate pxw_cyrl -- non-default charset, non-default collation
                ,a_nc_fixed_char_implicit_coll nchar(10)
                ,a_nc_fixed_char_explicit_coll nchar(10) collate fr_fr
                ,a_bt_default_cset_implicit_coll blob sub_type text
                ,a_bt_default_cset_explicit_coll blob sub_type text collate utf8
                ,a_bt_nondef_cset_implicit_coll blob sub_type text character set win1251
                ,a_bt_nondef_cset_explicit_coll blob sub_type text character set win1251 collate pxw_cyrl
                ,a_blob_binary blob sub_type binary
            ) returns (
                 o_vc_default_cset_implicit_coll varchar(10) -- default charset, default collation
                ,o_vc_default_cset_explicit_coll varchar(10) collate utf8 -- default charset, non-default collation
                ,o_vc_nondef_cset_implicit_coll varchar(10) character set win1251 -- non-default charset, default collation
                ,o_vc_nondef_cset_explicit_coll varchar(10) character set win1251 collate pxw_cyrl -- non-default charset, non-default collation
                ,o_nc_fixed_char_implicit_coll nchar(10)
                ,o_nc_fixed_char_explicit_coll nchar(10) collate fr_fr
                ,o_bt_default_cset_implicit_coll blob sub_type text
                ,o_bt_default_cset_explicit_coll blob sub_type text collate utf8
                ,o_bt_nondef_cset_implicit_coll blob sub_type text character set win1251
                ,o_bt_nondef_cset_explicit_coll blob sub_type text character set win1251 collate pxw_cyrl
                ,o_blob_binary blob sub_type binary
            );
        end
        ^
        create package body pg_test as
        begin
            procedure pg_sp (
                 a_vc_default_cset_implicit_coll varchar(10) -- default charset, default collation
                ,a_vc_default_cset_explicit_coll varchar(10) collate utf8 -- default charset, non-default collation
                ,a_vc_nondef_cset_implicit_coll varchar(10) character set win1251 -- non-default charset, default collation
                ,a_vc_nondef_cset_explicit_coll varchar(10) character set win1251 collate pxw_cyrl -- non-default charset, non-default collation
                ,a_nc_fixed_char_implicit_coll nchar(10)
                ,a_nc_fixed_char_explicit_coll nchar(10) collate fr_fr
                ,a_bt_default_cset_implicit_coll blob sub_type text
                ,a_bt_default_cset_explicit_coll blob sub_type text collate utf8
                ,a_bt_nondef_cset_implicit_coll blob sub_type text character set win1251
                ,a_bt_nondef_cset_explicit_coll blob sub_type text character set win1251 collate pxw_cyrl
                ,a_blob_binary blob sub_type binary
            ) returns (
                 o_vc_default_cset_implicit_coll varchar(10) -- default charset, default collation
                ,o_vc_default_cset_explicit_coll varchar(10) collate utf8 -- default charset, non-default collation
                ,o_vc_nondef_cset_implicit_coll varchar(10) character set win1251 -- non-default charset, default collation
                ,o_vc_nondef_cset_explicit_coll varchar(10) character set win1251 collate pxw_cyrl -- non-default charset, non-default collation
                ,o_nc_fixed_char_implicit_coll nchar(10)
                ,o_nc_fixed_char_explicit_coll nchar(10) collate fr_fr
                ,o_bt_default_cset_implicit_coll blob sub_type text
                ,o_bt_default_cset_explicit_coll blob sub_type text collate utf8
                ,o_bt_nondef_cset_implicit_coll blob sub_type text character set win1251
                ,o_bt_nondef_cset_explicit_coll blob sub_type text character set win1251 collate pxw_cyrl
                ,o_blob_binary blob sub_type binary
            ) as
            begin
            end
        end
        ^
        commit
        ^
        --#####################################################################################
        show domain dm_vc_default_cset_explicit_coll
        ^
        show domain dm_vc_default_cset_implicit_coll
        ^
        show domain dm_vc_nondef_cset_implicit_coll
        ^
        show domain dm_vc_nondef_cset_explicit_coll
        ^
        show table test
        ^
        show procedure sp_test
        ^
        show function fn_test
        ^
        show package pg_test
        ^
    """

    isql_show_expected_stdout_6x = """
        PUBLIC.DM_VC_DEFAULT_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8 Nullable
        PUBLIC.DM_VC_DEFAULT_CSET_IMPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI Nullable
        PUBLIC.DM_VC_NONDEF_CSET_IMPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 Nullable
        PUBLIC.DM_VC_NONDEF_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL Nullable
        VC_DEFAULT_CSET_IMPLICIT_COLL   VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI Nullable
        VC_DEFAULT_CSET_EXPLICIT_COLL   VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8 Nullable
        VC_NONDEF_CSET_IMPLICIT_COLL    VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 Nullable
        VC_NONDEF_CSET_EXPLICIT_COLL    VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL Nullable
        NC_FIXED_CHAR_IMPLICIT_COLL     CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 Nullable
        NC_FIXED_CHAR_EXPLICIT_COLL     CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR Nullable
        BT_DEFAULT_CSET_IMPLICIT_COLL   BLOB segment 80, subtype TEXT CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI Nullable
        BT_DEFAULT_CSET_EXPLICIT_COLL   BLOB segment 80, subtype TEXT CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8 Nullable
        BT_NONDEF_CSET_IMPLICIT_COLL    BLOB segment 80, subtype TEXT CHARACTER SET SYSTEM.WIN1251 Nullable
        BT_NONDEF_CSET_EXPLICIT_COLL    BLOB segment 80, subtype TEXT CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL Nullable
        BLOB_BINARY                     BLOB segment 80, subtype BINARY Nullable
        A_VC_DEFAULT_CSET_IMPLICIT_COLL   INPUT VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI
        A_VC_DEFAULT_CSET_EXPLICIT_COLL   INPUT VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8
        A_VC_NONDEF_CSET_IMPLICIT_COLL    INPUT VARCHAR(10) CHARACTER SET SYSTEM.WIN1251
        A_VC_NONDEF_CSET_EXPLICIT_COLL    INPUT VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL
        A_NC_FIXED_CHAR_IMPLICIT_COLL     INPUT CHAR(10) CHARACTER SET SYSTEM.ISO8859_1
        A_NC_FIXED_CHAR_EXPLICIT_COLL     INPUT CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR
        A_BT_DEFAULT_CSET_IMPLICIT_COLL   INPUT BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI
        A_BT_DEFAULT_CSET_EXPLICIT_COLL   INPUT BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8
        A_BT_NONDEF_CSET_IMPLICIT_COLL    INPUT BLOB CHARACTER SET SYSTEM.WIN1251
        A_BT_NONDEF_CSET_EXPLICIT_COLL    INPUT BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL
        A_BLOB_BINARY                     INPUT BLOB CHARACTER SET SYSTEM.NONE
        O_VC_DEFAULT_CSET_IMPLICIT_COLL   OUTPUT VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI
        O_VC_DEFAULT_CSET_EXPLICIT_COLL   OUTPUT VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8
        O_VC_NONDEF_CSET_IMPLICIT_COLL    OUTPUT VARCHAR(10) CHARACTER SET SYSTEM.WIN1251
        O_VC_NONDEF_CSET_EXPLICIT_COLL    OUTPUT VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL
        O_NC_FIXED_CHAR_IMPLICIT_COLL     OUTPUT CHAR(10) CHARACTER SET SYSTEM.ISO8859_1
        O_NC_FIXED_CHAR_EXPLICIT_COLL     OUTPUT CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR
        O_BT_DEFAULT_CSET_IMPLICIT_COLL   OUTPUT BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI
        O_BT_DEFAULT_CSET_EXPLICIT_COLL   OUTPUT BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8
        O_BT_NONDEF_CSET_IMPLICIT_COLL    OUTPUT BLOB CHARACTER SET SYSTEM.WIN1251
        O_BT_NONDEF_CSET_EXPLICIT_COLL    OUTPUT BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL
        O_BLOB_BINARY                     OUTPUT BLOB CHARACTER SET SYSTEM.NONE
        OUTPUT (PUBLIC.DM_VC_DEFAULT_CSET_EXPLICIT_COLL) VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI_AI
        A_VC_DEFAULT_CSET_IMPLICIT_COLL   INPUT VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI
        A_VC_DEFAULT_CSET_EXPLICIT_COLL   INPUT VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8
        A_VC_NONDEF_CSET_IMPLICIT_COLL    INPUT VARCHAR(10) CHARACTER SET SYSTEM.WIN1251
        A_VC_NONDEF_CSET_EXPLICIT_COLL    INPUT VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL
        A_NC_FIXED_CHAR_IMPLICIT_COLL     INPUT CHAR(10) CHARACTER SET SYSTEM.ISO8859_1
        A_NC_FIXED_CHAR_EXPLICIT_COLL     INPUT CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR
        A_BT_DEFAULT_CSET_IMPLICIT_COLL   INPUT BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UNICODE_CI
        A_BT_DEFAULT_CSET_EXPLICIT_COLL   INPUT BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8
        A_BT_NONDEF_CSET_IMPLICIT_COLL    INPUT BLOB CHARACTER SET SYSTEM.WIN1251
        A_BT_NONDEF_CSET_EXPLICIT_COLL    INPUT BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL
        A_BLOB_BINARY                     INPUT BLOB CHARACTER SET SYSTEM.NONE
    """
    act.expected_stdout = isql_show_expected_stdout_6x
    act.isql(input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #-----------------------------------------------------------

    # Test N2: check extracted metadata (result of 'isql -x').
    # -------

    isql_meta_expected_stdout_6x = """
        CREATE DOMAIN PUBLIC.DM_BLOB_BINARY AS BLOB SUB_TYPE 0 SEGMENT SIZE 80;
        CREATE DOMAIN PUBLIC.DM_BT_DEFAULT_CSET_EXPLICIT_COLL AS BLOB SUB_TYPE TEXT SEGMENT SIZE 80 CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8;
        CREATE DOMAIN PUBLIC.DM_BT_DEFAULT_CSET_IMPLICIT_COLL AS BLOB SUB_TYPE TEXT SEGMENT SIZE 80;
        CREATE DOMAIN PUBLIC.DM_BT_NONDEF_CSET_EXPLICIT_COLL AS BLOB SUB_TYPE TEXT SEGMENT SIZE 80 CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL;
        CREATE DOMAIN PUBLIC.DM_BT_NONDEF_CSET_IMPLICIT_COLL AS BLOB SUB_TYPE TEXT SEGMENT SIZE 80 CHARACTER SET SYSTEM.WIN1251;
        CREATE DOMAIN PUBLIC.DM_NC_FIXED_CHAR_EXPLICIT_COLL AS CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR;
        CREATE DOMAIN PUBLIC.DM_NC_FIXED_CHAR_IMPLICIT_COLL AS CHAR(10) CHARACTER SET SYSTEM.ISO8859_1;
        CREATE DOMAIN PUBLIC.DM_VC_DEFAULT_CSET_EXPLICIT_COLL AS VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8;
        CREATE DOMAIN PUBLIC.DM_VC_DEFAULT_CSET_IMPLICIT_COLL AS VARCHAR(10);
        CREATE DOMAIN PUBLIC.DM_VC_NONDEF_CSET_EXPLICIT_COLL AS VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL;
        CREATE DOMAIN PUBLIC.DM_VC_NONDEF_CSET_IMPLICIT_COLL AS VARCHAR(10) CHARACTER SET SYSTEM.WIN1251;
        CREATE TABLE PUBLIC.TEST (VC_DEFAULT_CSET_IMPLICIT_COLL VARCHAR(10),
        VC_DEFAULT_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        VC_NONDEF_CSET_IMPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251,
        VC_NONDEF_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        NC_FIXED_CHAR_IMPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1,
        NC_FIXED_CHAR_EXPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR,
        BT_DEFAULT_CSET_IMPLICIT_COLL BLOB SUB_TYPE TEXT SEGMENT SIZE 80,
        BT_DEFAULT_CSET_EXPLICIT_COLL BLOB SUB_TYPE TEXT SEGMENT SIZE 80 CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        BT_NONDEF_CSET_IMPLICIT_COLL BLOB SUB_TYPE TEXT SEGMENT SIZE 80 CHARACTER SET SYSTEM.WIN1251,
        BT_NONDEF_CSET_EXPLICIT_COLL BLOB SUB_TYPE TEXT SEGMENT SIZE 80 CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        BLOB_BINARY BLOB SUB_TYPE 0 SEGMENT SIZE 80);
        CREATE OR ALTER FUNCTION PUBLIC.FN_TEST (A_VC_DEFAULT_CSET_IMPLICIT_COLL VARCHAR(10),
        A_VC_DEFAULT_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        A_VC_NONDEF_CSET_IMPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251,
        A_VC_NONDEF_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        A_NC_FIXED_CHAR_IMPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1,
        A_NC_FIXED_CHAR_EXPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR,
        A_BT_DEFAULT_CSET_IMPLICIT_COLL BLOB,
        A_BT_DEFAULT_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        A_BT_NONDEF_CSET_IMPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251,
        A_BT_NONDEF_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        A_BLOB_BINARY BLOB)
        RETURNS PUBLIC.DM_VC_DEFAULT_CSET_EXPLICIT_COLL COLLATE SYSTEM.UNICODE_CI_AI
        CREATE OR ALTER PROCEDURE PUBLIC.SP_TEST (A_VC_DEFAULT_CSET_IMPLICIT_COLL VARCHAR(10),
        A_VC_DEFAULT_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        A_VC_NONDEF_CSET_IMPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251,
        A_VC_NONDEF_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        A_NC_FIXED_CHAR_IMPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1,
        A_NC_FIXED_CHAR_EXPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR,
        A_BT_DEFAULT_CSET_IMPLICIT_COLL BLOB,
        A_BT_DEFAULT_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        A_BT_NONDEF_CSET_IMPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251,
        A_BT_NONDEF_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        A_BLOB_BINARY BLOB)
        RETURNS (O_VC_DEFAULT_CSET_IMPLICIT_COLL VARCHAR(10),
        O_VC_DEFAULT_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        O_VC_NONDEF_CSET_IMPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251,
        O_VC_NONDEF_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        O_NC_FIXED_CHAR_IMPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1,
        O_NC_FIXED_CHAR_EXPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR,
        O_BT_DEFAULT_CSET_IMPLICIT_COLL BLOB,
        O_BT_DEFAULT_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        O_BT_NONDEF_CSET_IMPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251,
        O_BT_NONDEF_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        O_BLOB_BINARY BLOB)
        ALTER FUNCTION PUBLIC.FN_TEST (A_VC_DEFAULT_CSET_IMPLICIT_COLL VARCHAR(10),
        A_VC_DEFAULT_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        A_VC_NONDEF_CSET_IMPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251,
        A_VC_NONDEF_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        A_NC_FIXED_CHAR_IMPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1,
        A_NC_FIXED_CHAR_EXPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR,
        A_BT_DEFAULT_CSET_IMPLICIT_COLL BLOB,
        A_BT_DEFAULT_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        A_BT_NONDEF_CSET_IMPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251,
        A_BT_NONDEF_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        A_BLOB_BINARY BLOB)
        RETURNS PUBLIC.DM_VC_DEFAULT_CSET_EXPLICIT_COLL COLLATE SYSTEM.UNICODE_CI_AI
        ALTER PROCEDURE PUBLIC.SP_TEST (A_VC_DEFAULT_CSET_IMPLICIT_COLL VARCHAR(10),
        A_VC_DEFAULT_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        A_VC_NONDEF_CSET_IMPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251,
        A_VC_NONDEF_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        A_NC_FIXED_CHAR_IMPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1,
        A_NC_FIXED_CHAR_EXPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR,
        A_BT_DEFAULT_CSET_IMPLICIT_COLL BLOB,
        A_BT_DEFAULT_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        A_BT_NONDEF_CSET_IMPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251,
        A_BT_NONDEF_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        A_BLOB_BINARY BLOB)
        RETURNS (O_VC_DEFAULT_CSET_IMPLICIT_COLL VARCHAR(10),
        O_VC_DEFAULT_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        O_VC_NONDEF_CSET_IMPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251,
        O_VC_NONDEF_CSET_EXPLICIT_COLL VARCHAR(10) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        O_NC_FIXED_CHAR_IMPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1,
        O_NC_FIXED_CHAR_EXPLICIT_COLL CHAR(10) CHARACTER SET SYSTEM.ISO8859_1 COLLATE SYSTEM.FR_FR,
        O_BT_DEFAULT_CSET_IMPLICIT_COLL BLOB,
        O_BT_DEFAULT_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.UTF8 COLLATE SYSTEM.UTF8,
        O_BT_NONDEF_CSET_IMPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251,
        O_BT_NONDEF_CSET_EXPLICIT_COLL BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.PXW_CYRL,
        O_BLOB_BINARY BLOB)
    """
    act.expected_stdout = isql_meta_expected_stdout_6x
    act.extract_meta()
    assert act.clean_stdout == act.clean_expected_stdout
