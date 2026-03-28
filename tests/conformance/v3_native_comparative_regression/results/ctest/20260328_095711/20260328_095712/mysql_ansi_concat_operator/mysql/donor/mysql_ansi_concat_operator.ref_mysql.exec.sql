SET @@sql_mode = 'ANSI';
SELECT 'ASSERT|mysql_ansi_concat_operator|value|' || 'AB';
SET @@sql_mode = '';
