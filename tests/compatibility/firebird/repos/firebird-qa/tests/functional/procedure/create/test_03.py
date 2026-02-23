#coding:utf-8

"""
ID:          procedure.create-03
TITLE:       CREATE PROCEDURE - Output paramaters
DESCRIPTION:
FBTEST:      functional.procedure.create.03
NOTES:
    [30.09.2023] pzotov
    Added all supported data types that did appear since FB 4.x.
    Expected output differ in FB 3.x  vs 4.x/5.x vs 6.x. Added splitting.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'win1250')
act = python_act('db', substitutions = [ ('=', ''), ('[ \t]+', ' ') ])

@pytest.mark.version('>=3')
def test_1(act: Action):
    expected_stdout = ""
    if act.is_version('<4'):
        test_sql = """
            set term ^;
            create procedure test returns(
                  p01 smallint
                , p02 integer
                , p03 float
                , p04 double precision
                , p05 decimal(9,3)
                , p06 numeric(10,4)
                , p07 date
                , p08 time
                , p09 timestamp
                , p10 char(40)
                , p11 varchar(60)
                , p12 nchar(70)
            )
            as
            begin
              suspend;
            end ^
            set term ;^
            commit;
            show procedure test;
        """
    else:
        test_sql = """
            set term ^;
            create procedure test returns(
                  p_smallint smallint
                , p_int int
                , p_bigint int
                , p_int128 int
                , p_float float
                , p_double double precision
                , p_dec decimal(9,3)
                , p_num numeric(10,4)
                , p_decfloat_16 decfloat(16)
                , p_decfloat_34 decfloat(34)
                , p_bool boolean
                , p_date date
                , p_just_time time
                , p_just_timestamp timestamp 
                , p_time_wo_tz time without time zone
                , p_time_wi_tz time with time zone
                , p_timestamp_wo_tz timestamp without time zone
                , p_timestamp_wi_tz timestamp with time zone
                , p_char char(1)
                , p_vchr_wo_cset_and_coll varchar(1)
                , p_vchr_wi_cset_and_coll varchar(1) character set win1251 collate win1251_ua
                , p_vchr_wi_cset_only varchar(1) character set win1251
                , p_vchr_wi_coll_only varchar(1) collate pxw_hundc
                , p_nchr nchar(1)
                , p_binary binary(16)
                , p_varbin varbinary(16)
                , p_blob0 blob sub_type 0
                , p_blob1 blob sub_type 1
                , p_blob2 blob sub_type 1 character set win1251
                , p_blob3 blob sub_type 1 character set win1251 collate win1251_ua
            )
            as
            begin
              suspend;
            end ^
            set term ;^
            commit;
            show procedure test;
        """

    if act.is_version('<4'):
        expected_stdout = """
            Procedure text:
            =============================================================================
            begin
                suspend;
            end
            =============================================================================
            Parameters:
            P01                               OUTPUT SMALLINT
            P02                               OUTPUT INTEGER
            P03                               OUTPUT FLOAT
            P04                               OUTPUT DOUBLE PRECISION
            P05                               OUTPUT DECIMAL(9, 3)
            P06                               OUTPUT NUMERIC(10, 4)
            P07                               OUTPUT DATE
            P08                               OUTPUT TIME
            P09                               OUTPUT TIMESTAMP
            P10                               OUTPUT CHAR(40)
            P11                               OUTPUT VARCHAR(60)
            P12                               OUTPUT CHAR(70) CHARACTER SET ISO8859_1
        """
    elif act.is_version('<6'):
        expected_stdout = """
            Procedure text:
            =============================================================================
            begin
                suspend;
            end
            =============================================================================
            Parameters:
            P_SMALLINT                        OUTPUT SMALLINT
            P_INT                             OUTPUT INTEGER
            P_BIGINT                          OUTPUT INTEGER
            P_INT128                          OUTPUT INTEGER
            P_FLOAT                           OUTPUT FLOAT
            P_DOUBLE                          OUTPUT DOUBLE PRECISION
            P_DEC                             OUTPUT DECIMAL(9, 3)
            P_NUM                             OUTPUT NUMERIC(10, 4)
            P_DECFLOAT_16                     OUTPUT DECFLOAT(16)
            P_DECFLOAT_34                     OUTPUT DECFLOAT(34)
            P_BOOL                            OUTPUT BOOLEAN
            P_DATE                            OUTPUT DATE
            P_JUST_TIME                       OUTPUT TIME
            P_JUST_TIMESTAMP                  OUTPUT TIMESTAMP
            P_TIME_WO_TZ                      OUTPUT TIME
            P_TIME_WI_TZ                      OUTPUT TIME WITH TIME ZONE
            P_TIMESTAMP_WO_TZ                 OUTPUT TIMESTAMP
            P_TIMESTAMP_WI_TZ                 OUTPUT TIMESTAMP WITH TIME ZONE
            P_CHAR                            OUTPUT CHAR(1)
            P_VCHR_WO_CSET_AND_COLL           OUTPUT VARCHAR(1)
            P_VCHR_WI_CSET_AND_COLL           OUTPUT VARCHAR(1) CHARACTER SET WIN1251 COLLATE WIN1251_UA
            P_VCHR_WI_CSET_ONLY               OUTPUT VARCHAR(1) CHARACTER SET WIN1251
            P_VCHR_WI_COLL_ONLY               OUTPUT VARCHAR(1) CHARACTER SET WIN1250 COLLATE PXW_HUNDC
            P_NCHR                            OUTPUT CHAR(1) CHARACTER SET ISO8859_1
            P_BINARY                          OUTPUT BINARY(16)
            P_VARBIN                          OUTPUT VARBINARY(16)
            P_BLOB0                           OUTPUT BLOB CHARACTER SET NONE
            P_BLOB1                           OUTPUT BLOB
            P_BLOB2                           OUTPUT BLOB CHARACTER SET WIN1251
            P_BLOB3                           OUTPUT BLOB CHARACTER SET WIN1251 COLLATE WIN1251_UA
        """
    else:
        expected_stdout = """
            Procedure: PUBLIC.TEST
            Procedure text:
            begin
            suspend;
            end
            Parameters:
            P_SMALLINT OUTPUT SMALLINT
            P_INT OUTPUT INTEGER
            P_BIGINT OUTPUT INTEGER
            P_INT128 OUTPUT INTEGER
            P_FLOAT OUTPUT FLOAT
            P_DOUBLE OUTPUT DOUBLE PRECISION
            P_DEC OUTPUT DECIMAL(9, 3)
            P_NUM OUTPUT NUMERIC(10, 4)
            P_DECFLOAT_16 OUTPUT DECFLOAT(16)
            P_DECFLOAT_34 OUTPUT DECFLOAT(34)
            P_BOOL OUTPUT BOOLEAN
            P_DATE OUTPUT DATE
            P_JUST_TIME OUTPUT TIME
            P_JUST_TIMESTAMP OUTPUT TIMESTAMP
            P_TIME_WO_TZ OUTPUT TIME
            P_TIME_WI_TZ OUTPUT TIME WITH TIME ZONE
            P_TIMESTAMP_WO_TZ OUTPUT TIMESTAMP
            P_TIMESTAMP_WI_TZ OUTPUT TIMESTAMP WITH TIME ZONE
            P_CHAR OUTPUT CHAR(1) CHARACTER SET SYSTEM.WIN1250
            P_VCHR_WO_CSET_AND_COLL OUTPUT VARCHAR(1) CHARACTER SET SYSTEM.WIN1250
            P_VCHR_WI_CSET_AND_COLL OUTPUT VARCHAR(1) CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.WIN1251_UA
            P_VCHR_WI_CSET_ONLY OUTPUT VARCHAR(1) CHARACTER SET SYSTEM.WIN1251
            P_VCHR_WI_COLL_ONLY OUTPUT VARCHAR(1) CHARACTER SET SYSTEM.WIN1250 COLLATE SYSTEM.PXW_HUNDC
            P_NCHR OUTPUT CHAR(1) CHARACTER SET SYSTEM.ISO8859_1
            P_BINARY OUTPUT BINARY(16)
            P_VARBIN OUTPUT VARBINARY(16)
            P_BLOB0 OUTPUT BLOB CHARACTER SET SYSTEM.NONE
            P_BLOB1 OUTPUT BLOB CHARACTER SET SYSTEM.WIN1250
            P_BLOB2 OUTPUT BLOB CHARACTER SET SYSTEM.WIN1251
            P_BLOB3 OUTPUT BLOB CHARACTER SET SYSTEM.WIN1251 COLLATE SYSTEM.WIN1251_UA
        """

    act.expected_stdout = expected_stdout
    act.isql(input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
