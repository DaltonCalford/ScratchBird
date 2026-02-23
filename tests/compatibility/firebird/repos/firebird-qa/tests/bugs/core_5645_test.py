#coding:utf-8

"""
ID:          issue-5911
ISSUE:       5911
TITLE:       Wrong transaction can be passed to external engine
DESCRIPTION:
  Implemented according to notes given by Adriano in the ticket 27-oct-2017 02:41.
JIRA:        CORE-5645
FBTEST:      bugs.core_5645
"""

import pytest
from firebird.qa import *

init_script = """
    create table persons (
        id integer not null,
        name varchar(60) not null,
        address varchar(60),
        info blob sub_type text
    );
"""

db = db_factory(sql_dialect=3, init=init_script)
db_repl = db_factory(sql_dialect=3, init=init_script, filename='tmp_5645_repl.fd')

act = python_act('db', substitutions=[('INFO_BLOB_ID.*', '')])

expected_stdout = """
    ID                              1
    NAME                            One
    ADDRESS                         some_address
    INFO_BLOB_ID                    80:0
    some_blob_info

    ID                              1
    NAME                            One
    ADDRESS                         <null>
    INFO_BLOB_ID                    80:1
    some_blob_info
    Records affected: 2
"""

@pytest.mark.version('>=3.0.3')
def test_1(act: Action, db_repl: Database):
    ddl_for_replication = f"""
        create table replicate_config (
            name varchar(31) not null,
            data_source varchar(255) not null
        );

        insert into replicate_config (name, data_source)
           values ('ds1', '{db_repl.db_path}');

        create trigger persons_replicate
            after insert on persons
            external name 'udrcpp_example!replicate!ds1'
            engine udr;

        create trigger persons_replicate2
            after insert on persons
            external name 'udrcpp_example!replicate_persons!ds1'
            engine udr;
        commit;
        """
    act.isql(switches=['-q'], input=ddl_for_replication)
    #
    with act.db.connect() as con:
        c = con.cursor()
        c.execute("insert into persons values (1, 'One', 'some_address', 'some_blob_info')")
        con.commit()
    #
    if act.is_version('>4.0'):
        act.reset()
        act.isql(switches=['-q'], input='ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL;')
    # Check
    act.reset()
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q', db_repl.dsn],
               input='set list on; set count on; select id,name,address,info as info_blob_id from persons; rollback;',
               connect_db=False)
    assert act.clean_stdout == act.clean_expected_stdout
