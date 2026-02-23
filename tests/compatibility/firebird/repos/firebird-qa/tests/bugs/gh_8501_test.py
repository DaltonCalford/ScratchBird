#coding:utf-8

"""
ID:          issue-8501
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8501
TITLE:       fix(cast-format): Throw exception when value cannot be found for specific pattern in string to datetime conversion
DESCRIPTION:
    Commit:
        https://github.com/FirebirdSQL/firebird/commit/90e0f49acac2f317757b9ebecf8935692a2b46df
    Examples taken from src/common/tests/CvtTest.cpp:
        https://github.com/FirebirdSQL/firebird/pull/8501/files#diff-4c41a1e71a7a12168ddce05a6e570ee08d64e36c981fca8e7274ebac4c41edb1
    Also (for "Handle integer overflow when converting string to int") see:
        https://github.com/FirebirdSQL/firebird/issues/8475#issuecomment-2772324636
NOTES:
    [11.04.2025] pzotov

    Following queries display THE SAME error messages before and after fix:
        * select cast('00:60' as time with time zone format 'TZR') from rdb$database
        * select cast('15:00' as time format 'TZH:TZM') from rdb$database
        * select cast('15:00' as time format 'TZR') from rdb$database
        * select cast('-15:00' as time format 'TZH:TZM') from rdb$database
        * select cast('-15:00' as time format 'TZR') from rdb$database
    It is unclear for what reason they were added into src/common/tests/CvtTest.cpp
    
    Confirmed wrong output (w/o errors or with different messages) on 6.0.0.722.
    Checked on 6.0.0.725
"""
from firebird.driver import DatabaseError
import pytest
from firebird.qa import *

db = db_factory()

# NB: all following expression must issue errors:
# #################
query_map = {

     # These must issue:
     # Cannot find value in input string for "..." pattern
     1000 : ( "select cast('Apr' as date format 'Y     MON') from rdb$database" )
    ,1010 : ( "select cast('Apr' as date format 'YY    MON') from rdb$database" )
    ,1020 : ( "select cast('Apr' as date format 'YYY   MON') from rdb$database" )
    ,1030 : ( "select cast('Apr' as date format 'YYYY  MON') from rdb$database" )
    ,1040 : ( "select cast('Apr' as date format 'YEAR  MON') from rdb$database" )
    ,1050 : ( "select cast('Apr' as date format 'RR    MON') from rdb$database" )
    ,1060 : ( "select cast('Apr' as date format 'RRRR  MON') from rdb$database" )
    ,1070 : ( "select cast('Apr' as date format 'MM    MON') from rdb$database" )
    ,1080 : ( "select cast('Apr' as date format 'DD    MON') from rdb$database" )
    ,1090 : ( "select cast('Apr' as date format 'J     MON') from rdb$database" )

     # These must issue:
     # *  Invalid time zone offset: 2147483647 - must use format +/-hours:minutes and be between -14:00 and +14:00
     # OR
     # *  Value for TZR pattern is out of range [0, 59]
     # See also: https://github.com/FirebirdSQL/firebird/issues/8475#issuecomment-2772324636:
    ,2000 : ( "select cast('9999999999999999999999999999999999999:00' as time with time zone format 'TZH:TZM') from rdb$database" )
    ,2010 : ( "select cast('9999999999999999999999999999999999999:00' as time with time zone format 'TZR') from rdb$database" )
    ,2020 : ( "select cast('-9999999999999999999999999999999999999:00' as time with time zone format 'TZH:TZM') from rdb$database" )
    ,2030 : ( "select cast('-9999999999999999999999999999999999999:00' as time with time zone format 'TZR') from rdb$database" )
    ,2040 : ( "select cast('00:9999999999999999999999999999999999999' as time with time zone format 'TZH:TZM') from rdb$database" )
    ,2050 : ( "select cast('00:9999999999999999999999999999999999999' as time with time zone format 'TZR') from rdb$database" )
    ,2060 : ( "select cast('00:-9999999999999999999999999999999999999' as time with time zone format 'TZH:TZM') from rdb$database;" )
    ,2070 : ( "select cast('00:-9999999999999999999999999999999999999' as time with time zone format 'TZR') from rdb$database" )

     # These issue the same as in previous snapshot:
     # *  Value for TZR pattern is out of range [0, 59]
     # OR
     # *  Cannot use <TZR|TZH> format with current date type
    ,3000 : ( "select cast('00:60' as time with time zone format 'TZR') from rdb$database" )
    ,3010 : ( "select cast('15:00' as time format 'TZH:TZM') from rdb$database" )
    ,3020 : ( "select cast('15:00' as time format 'TZR') from rdb$database" )
    ,3030 : ( "select cast('-15:00' as time format 'TZH:TZM') from rdb$database" )
    ,3040 : ( "select cast('-15:00' as time format 'TZR') from rdb$database" )
}


###############################################################################

act = python_act('db')

#-----------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:

        cur = con.cursor()
        for idx, test_sql in query_map.items():
            print(idx)
            print(test_sql)
            try:
                cur.execute(test_sql)
                for r in cur:
                    print(r[0])
            except DatabaseError as e:
                print(e.__str__())
                for x in e.gds_codes:
                    print(x)

    expected_out = f"""
        1000
        {query_map[1000]}
        Cannot find value in input string for "Y" pattern
        335545315

        1010
        {query_map[1010]}
        Cannot find value in input string for "YY" pattern
        335545315

        1020
        {query_map[1020]}
        Cannot find value in input string for "YYY" pattern
        335545315
        
        1030
        {query_map[1030]}
        Cannot find value in input string for "YYYY" pattern
        335545315
        
        1040
        {query_map[1040]}
        Cannot find value in input string for "YEAR" pattern
        335545315
        
        1050
        {query_map[1050]}
        Cannot find value in input string for "RR" pattern
        335545315
        
        1060
        {query_map[1060]}
        Cannot find value in input string for "RRRR" pattern
        335545315
        
        1070
        {query_map[1070]}
        Cannot find value in input string for "MM" pattern
        335545315
        
        1080
        {query_map[1080]}
        Cannot find value in input string for "DD" pattern
        335545315
        
        1090
        {query_map[1090]}
        Cannot find value in input string for "J" pattern
        335545315
        
        2000
        {query_map[2000]}
        Invalid time zone offset: 2147483647 - must use format +/-hours:minutes and be between -14:00 and +14:00
        335545213
        
        2010
        {query_map[2010]}
        Value for TZR pattern is out of range [0, 59]
        335545297
        
        2020
        {query_map[2020]}
        Invalid time zone offset: 2147483647 - must use format +/-hours:minutes and be between -14:00 and +14:00
        335545213
        
        2030
        {query_map[2030]}
        Value for TZR pattern is out of range [0, 59]
        335545297
        
        2040
        {query_map[2040]}
        Value for TZM pattern is out of range [0, 59]
        335545297
        
        2050
        {query_map[2050]}
        Value for TZR pattern is out of range [0, 59]
        335545297
        
        2060
        {query_map[2060]}
        Value for TZM pattern is out of range [0, 59]
        335545297
        
        2070
        {query_map[2070]}
        Value for TZR pattern is out of range [0, 59]
        335545297
        
        3000
        {query_map[3000]}
        Value for TZR pattern is out of range [0, 59]
        335545297
        
        3010
        {query_map[3010]}
        Cannot use "TZH" format with current date type
        335545296
        
        3020
        {query_map[3020]}
        Cannot use "TZR" format with current date type
        335545296
        
        3030
        {query_map[3030]}
        Cannot use "TZH" format with current date type
        335545296
        
        3040
        {query_map[3040]}
        Cannot use "TZR" format with current date type
        335545296
    """
    act.expected_stdout = expected_out
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
