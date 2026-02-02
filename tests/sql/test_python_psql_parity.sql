-- ScratchBird
-- Copyright (c) 2025-2026 Dalton Calford
--
-- Licensed under the Initial Developer's Public License Version 1.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at:
-- https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/

-- Python parity operators/functions smoke coverage (parser/executor)
SELECT 9 DIV 2;
SELECT 'abc' STARTING WITH 'a';
SELECT 'abc' CONTAINING 'b';
SELECT REPLACE('abc', 'b', 'x');
SELECT ENDS_WITH('abc', 'bc');
SELECT ARRAY[1,2,3][2:3];
SELECT ARRAY_POSITION(ARRAY[1,2,3], 2);
SELECT JSON_EXISTS('{"a":1}', '$.a');
SELECT JSON_HAS_KEY('{"a":1}', 'a');
SELECT TO_CHAR('2024-01-01', 'YYYY-MM-DD');
SELECT TO_DATE('2024-01-01', 'YYYY-MM-DD');
SELECT TO_TIMESTAMP('2024-01-01 01:02:03', 'YYYY-MM-DD HH24:MI:SS');
SELECT LEAST(1, 2, 3);
SELECT GREATEST(1, 2, 3);
