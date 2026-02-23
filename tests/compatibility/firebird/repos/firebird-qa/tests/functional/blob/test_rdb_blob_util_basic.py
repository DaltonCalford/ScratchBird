#coding:utf-8

"""
ID:          test_rdb_blob_util_basic
TITLE:       Basic test of RDB$BLOB_UTIL package functionality
DESCRIPTION: Test uses only examples from doc/sql.extensions/README.blob_util.md; advanced checks will be stored in another tests.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set blob all;
    set term ^;
    -- Example 1: Create a BLOB in temporary space and return it from EXECUTE BLOCK:
    execute block returns (example_1_blob_id blob)
    as
    begin
        -- Create a BLOB handle in the temporary space.
        example_1_blob_id = rdb$blob_util.new_blob(false, true);

        -- Add chunks of data.
        example_1_blob_id = blob_append(example_1_blob_id, '12345');
        example_1_blob_id = blob_append(example_1_blob_id, '67');

        suspend;
    end
    ^
    -- ##############################################################################
    -- Example 2: Open a BLOB and return chunks of it from EXECUTE BLOCK:
    -- NB: rdb$blob_util.read_data() returns type: `VARBINARY(32767)`.
    execute block returns (example_2_blob_read_data varchar(10))
    as
        declare b blob = '1234567';
        declare bhandle integer;
    begin
        -- Open the BLOB and get a BLOB handle.
        bhandle = rdb$blob_util.open_blob(b);

        -- Get chunks of data as string and return.

        example_2_blob_read_data = rdb$blob_util.read_data(bhandle, 3);
        suspend;

        example_2_blob_read_data = rdb$blob_util.read_data(bhandle, 3);
        suspend;

        example_2_blob_read_data = rdb$blob_util.read_data(bhandle, 3);
        suspend;

        -- Here EOF is found, so it returns NULL.
        example_2_blob_read_data = rdb$blob_util.read_data(bhandle, 3);
        suspend;

        -- Close the BLOB handle.
        execute procedure rdb$blob_util.close_handle(bhandle);
    end
    ^
    -- ##############################################################################
    -- Example 3: Seek in a blob.
    execute block returns (example_3_blob_read_data varchar(10))
    as
        declare b blob;
        declare bhandle integer;
    begin
        -- Create a stream BLOB handle.
        b = rdb$blob_util.new_blob(false, true);

        -- Add data.
        b = blob_append(b, '0123456789');

        -- Open the BLOB.
        bhandle = rdb$blob_util.open_blob(b);

        -- Seek to 5 since the start.
        rdb$blob_util.seek(bhandle, 0, 5);
        example_3_blob_read_data = rdb$blob_util.read_data(bhandle, 3);
        suspend;

        -- Seek to 2 since the start.
        rdb$blob_util.seek(bhandle, 0, 2);
        example_3_blob_read_data = rdb$blob_util.read_data(bhandle, 3);
        suspend;

        -- Advance 2.
        rdb$blob_util.seek(bhandle, 1, 2);
        example_3_blob_read_data = rdb$blob_util.read_data(bhandle, 3);
        suspend;

        -- Seek to -1 since the end.
        rdb$blob_util.seek(bhandle, 2, -1);
        example_3_blob_read_data = rdb$blob_util.read_data(bhandle, 3);
        suspend;
    end
    ^
    -- ##############################################################################
    -- Example 4: Check if blobs are writable:
    create table t(b blob)
    ^
    execute block returns (example_4_blob_is_writable boolean)
    as
        declare b blob;
    begin
        b = blob_append(null, 'writable');
        example_4_blob_is_writable = rdb$blob_util.is_writable(b);
        suspend;

        insert into t (b) values ('not writable') returning b into b;
        example_4_blob_is_writable = rdb$blob_util.is_writable(b);
        suspend;
    end
    ^
"""

act = isql_act('db', test_script, substitutions=[('EXAMPLE_1_BLOB_ID .*', '')])

expected_stdout = """
    EXAMPLE_1_BLOB_ID               0:1
    1234567
    EXAMPLE_2_BLOB_READ_DATA        123
    EXAMPLE_2_BLOB_READ_DATA        456
    EXAMPLE_2_BLOB_READ_DATA        7
    EXAMPLE_2_BLOB_READ_DATA        <null>
    EXAMPLE_3_BLOB_READ_DATA        567
    EXAMPLE_3_BLOB_READ_DATA        234
    EXAMPLE_3_BLOB_READ_DATA        789
    EXAMPLE_3_BLOB_READ_DATA        9
    EXAMPLE_4_BLOB_IS_WRITABLE      <true>
    EXAMPLE_4_BLOB_IS_WRITABLE      <false>
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
