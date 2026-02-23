#coding:utf-8

"""
ID:          issue-8056
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8056#issuecomment-2032627160
TITLE:       "Too many temporary blobs" - additional test for issuecomment-2032627160
DESCRIPTION:
NOTES:
    Confirmed bug on 5.0.1.1373 #48915d1 (commit timestamp: 02-apr-2024 14:14 UTC).
    Checked on 5.0.1.1377 #3b5ab26 (commit timestamp: 03-apr-2024 20:59 UTC) - all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    set count on;
    set list on;
    set bail on;
    set term ^;
    execute block returns (vb varchar(20))
    as
        declare b blob;
        declare bhandle integer;
        declare read_data varbinary(20);
    begin
        -- Create a BLOB handle in the temporary space.
        b = rdb$blob_util.new_blob(true, true);

        -- Add chunks of data.
        b = blob_append(b, '1');
        b = blob_append(b, '2345');
        b = blob_append(b, '67');
        b = blob_append(b, '8');

        if (rdb$blob_util.is_writable(b)) then
        begin
            vb = '';
    	    bhandle = rdb$blob_util.open_blob(b);

            while (true)
            do
            begin
                read_data = rdb$blob_util.read_data(bhandle, null);
                if (read_data is null) then
                    break;

                vb = vb || read_data || '-';
            end

            execute procedure rdb$blob_util.close_handle(bhandle);

            suspend;
        end
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    VB 1-2345-67-8-
    Records affected: 1
"""

@pytest.mark.version('>=5.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
