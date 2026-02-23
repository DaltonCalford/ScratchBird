#coding:utf-8

"""
ID:          trigger.database.transaction-commit
TITLE:       Trigger on commit transaction. See also CORE-645
DESCRIPTION:
  Test trigger on commit transaction
FBTEST:      functional.trigger.database.transactioncommit_01
"""

import pytest
from firebird.qa import *

init_script = """
	SET AUTODDL OFF;
	CREATE TABLE T1
	(
		T1_ID BIGINT NOT NULL,
		T1_VAL CHAR(10) ,
		CONSTRAINT PK_T1 PRIMARY KEY (T1_ID)
	);

	CREATE SEQUENCE S_TRANSACTION;

	SET TERM ^ ;
	RECREATE TRIGGER TRIG_TRANSAC ACTIVE
	ON TRANSACTION COMMIT
	POSITION 0
	AS
	BEGIN
		RDB$SET_CONTEXT('USER_SESSION', 'Trn_ID', (NEXT VALUE FOR S_TRANSACTION));
	 END^

	SET TERM ; ^

	COMMIT;

"""

db = db_factory(init=init_script)

test_script = """SET AUTODDL OFF;

	SET LIST ON;

	INSERT INTO T1 VALUES (1,'val1');
	SELECT RDB$GET_CONTEXT('USER_SESSION', 'Trn_ID') AS CTX_VAR FROM RDB$DATABASE;
	COMMIT;
	UPDATE T1 SET T1_VAL='val1mod' WHERE T1_ID=1;
	SELECT RDB$GET_CONTEXT('USER_SESSION', 'Trn_ID') AS CTX_VAR  FROM RDB$DATABASE;
	ROLLBACK;
	DELETE FROM T1 WHERE T1_ID=1;
	SELECT RDB$GET_CONTEXT('USER_SESSION', 'Trn_ID') AS CTX_VAR  FROM RDB$DATABASE;
	COMMIT;
	SELECT RDB$GET_CONTEXT('USER_SESSION', 'Trn_ID') AS CTX_VAR  FROM RDB$DATABASE;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
	CTX_VAR                         <null>
	CTX_VAR                         3
	CTX_VAR                         3
	CTX_VAR                         4
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
