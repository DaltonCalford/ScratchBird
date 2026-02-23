#coding:utf-8

"""
ID:          issue-6224
ISSUE:       6224
TITLE:       External engine trigger crashing server if table have computed field
DESCRIPTION:
  We use supplied UDR example which operates with TRIGGER for replication purpuses.
  Two databases are used here: one is 'main' (which is created by fbtest) and second
  is auxiliary and serves as slave (replica).

  We create table PERSONS in both databases, its DDL is taken from examples code.
  This table will be normally replicated until we add COMPUTED BY field to it.

  When such field is added and we issue INSERT command, standard exception must raise:
    Statement failed, SQLSTATE = 42000
    Execute statement error at isc_dsql_prepare :
    335544569 : Dynamic SQL Error
    335544436 : SQL error code = -206
    335544578 : Column unknown
    335544382 : COMP
    336397208 : At line 1, column 57
    Statement : insert into "PERSONS" ("ID", "NAME", "ADDRESS", "INFO", "COMP") values (?, ?, ?, ?, ?)
    Data source : Firebird::<path-to-db>
    -At block line: ...
    -At trigger 'PERSONS_REPLICATE'

  We expect appearing of this exception (see try/except block): check its class and content of message.
JIRA:        CORE-5972
FBTEST:      bugs.core_5972
NOTES:
    [02.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
import platform
from firebird.qa import *
from firebird.driver import DatabaseError

substitutions = [ ('[ \t]+', ' '),
                  ('(-)?At block line.*', 'At block'),
                  ('read-only column.*', 'read-only column'),
                  ('Statement.*', 'Statement'), ('Data source.*', 'Data source'),
                ]

init_script = """
    create table persons (
        id integer not null,
        name varchar(60) not null,
        address varchar(60),
        info blob sub_type text,
        comp int computed by (1) -- COMPUTED_BY FIELD AS IT IS DESCRIBED IN THE TICKET
    );
"""

db = db_factory(init=init_script)
db_repl = db_factory(init=init_script, filename='core_5972_repl.fdb')

act = python_act('db', substitutions=substitutions)

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, db_repl: Database, capsys):
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
    # Test
    with act.db.connect() as con:
        c = con.cursor()
        try:
            c.execute("insert into persons values (1, 'One', 'some_address', 'some_blob_info')")
            con.commit()
        except DatabaseError as e:
            print(f'Got exception: {e.__class__}')
            print(e)
    #
    if act.is_version('>=4'):
        act.reset()
        act.isql(switches=['-q'], input='ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL;')
    #
    act.reset()

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TRIGGER_NAME = "'PERSONS_REPLICATE'" if act.is_version('<6') else '"PERSONS_REPLICATE"'

    expected_stdout = f"""
        Got exception: <class 'firebird.driver.types.DatabaseError'>
        Execute statement error at isc_dsql_prepare :
        335544359 : attempted update of read-only column PERSONS.COMP
        Statement : insert into "PERSONS" ("ID", "NAME", "ADDRESS", "INFO", "COMP") values (?, ?, ?, ?, ?)
        Data source : Firebird::
        -At block line: 9, col: 5
        -At trigger {SQL_SCHEMA_PREFIX}{TRIGGER_NAME}
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
