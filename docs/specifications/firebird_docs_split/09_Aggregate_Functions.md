# 09 Aggregate Functions



<!-- Original PDF Page: 492 -->

List currently active roles
select rdb$role_name
from rdb$roles
where rdb$role_in_use(rdb$role_name);
See also
CURRENT_ROLE
8.12.5. RDB$SYSTEM_PRIVILEGE()
Checks if the authorization of the current connection has a system privilege
Result type
BOOLEAN
Syntax
RDB$SYSTEM_PRIVILEGE (<sys_privilege>)
<sys_privilege> ::=
  !! See CREATE ROLE !!
Table 213. RDB$SYSTEM_PRIVILEGE Function Parameters
Parameter Description
sys_privilege System privilege
RDB$SYSTEM_PRIVILEGE accepts a system privilege name and returns TRUE if the current connection
has the given system privilege, and FALSE otherwise.
The authorization of the current connection is determined by privileges of the current user, the
user PUBLIC, and the currently active roles (explicitly set or activated by default).
RDB$SYSTEM_PRIVILEGE Examples
select rdb$system_privilege(user_management) from rdb$database;
See also
Fine-grained System Privileges
Chapter 8. Built-in Scalar Functions
491

<!-- Original PDF Page: 493 -->

Chapter 9. Aggregate Functions
Aggregate functions operate on groups of records, rather than on individual records or variables.
They are often used in combination with a GROUP BY clause.
Syntax
<aggregate_function> ::=
    aggragate_function ([<expr> [, <expr> ...]])
      [FILTER (WHERE <condition>)]
The aggregate functions can also be used as window functions with the OVER () clause. See Window
(Analytical) Functions for more information.
Aggregate functions are available in DSQL and PSQL. Availability in ESQL is not tracked by this
Language Reference.
9.1. FILTER Clause for Aggregate Functions
The FILTER clause extends aggregate functions ( SUM, AVG, COUNT, etc.) with an additional WHERE clause.
This limits the rows processed by the aggregate functions to the rows that satisfy the conditions of
both the main WHERE clause and those inside the FILTER clause.
It can be thought of as a more explicit form of using an aggregate function with a condition ( DECODE,
CASE, IIF, NULLIF) to ignore some values that would otherwise be considered by the aggregation.
The FILTER clause can be used with any aggregate functions in aggregate or windowed ( OVER)
statements, but not with window-only functions like DENSE_RANK.
Example of FILTER
Suppose you need a query to count the rows with status = 'A' and the row with status = 'E' as
different columns. The old way to do it would be:
select count(decode(status, 'A', 1)) status_a,
       count(decode(status, 'E', 1)) status_e
from data;
The FILTER clause lets you express those conditions more explicitly:
select count(*) filter (where status = 'A') status_a,
       count(*) filter (where status = 'E') status_e
from data;
 You can use more than one FILTER modifier in an aggregate query. You could, for
example, use 12 filters on totals aggregating sales for a year to produce monthly
Chapter 9. Aggregate Functions
492

<!-- Original PDF Page: 494 -->

figures for a pivot set.
9.2. General-purpose Aggregate Functions
9.2.1. AVG()
Average
Result type
Depends on the input type
Syntax
AVG ([ALL | DISTINCT] <expr>)
Table 214. AVG Function Parameters
Parameter Description
expr Expression. It may contain a table column, a constant, a variable, an
expression, a non-aggregate function or a UDF that returns a numeric
data type. Aggregate functions are not allowed as expressions
AVG returns the average argument value in the group. NULL is ignored.
• Parameter ALL (the default) applies the aggregate function to all values.
• Parameter DISTINCT directs the AVG function to consider only one instance of each unique value,
no matter how many times this value occurs.
• If the set of retrieved records is empty or contains only NULL, the result will be NULL.
The result type of AVG depends on the input type:
FLOAT, DOUBLE PRECISION DOUBLE PRECISION
SMALLINT, INTEGER, BIGINT BIGINT
INT128 INT128
DECIMAL/NUMERIC(p, n) with p < 19 DECIMAL/NUMERIC(18, n)
DECIMAL/NUMERIC(p, n) with p >= 19 DECIMAL/NUMERIC(38, n)
DECFLOAT(16) DECFLOAT(16)
DECFLOAT(34) DECFLOAT(34)
AVG Examples
SELECT
  dept_no,
  AVG(salary)
FROM employee
Chapter 9. Aggregate Functions
493

<!-- Original PDF Page: 495 -->

GROUP BY dept_no
See also
SELECT
9.2.2. COUNT()
Counts non-NULL values
Result type
BIGINT
Syntax
COUNT ([ALL | DISTINCT] <expr> | *)
Table 215. COUNT Function Parameters
Parameter Description
expr Expression. It may contain a table column, a constant, a variable, an
expression, a non-aggregate function or a UDF that returns a numeric
data type. Aggregate functions are not allowed as expressions
COUNT returns the number of non-null values in a group.
• ALL is the default: it counts all values in the set that are not NULL.
• If DISTINCT is specified, duplicates are excluded from the counted set.
• If COUNT (*) is specified instead of the expression expr, all rows will be counted. COUNT (*) — 
◦ does not accept parameters
◦ cannot be used with the keyword DISTINCT
◦ does not take an expr argument, since its context is column-unspecific by definition
◦ counts each row separately and returns the number of rows in the specified table or group
without omitting duplicate rows
◦ counts rows containing NULL
• If the result set is empty or contains only NULL in the specified column(s), the returned count is
zero.
COUNT Examples
SELECT
  dept_no,
  COUNT(*) AS cnt,
  COUNT(DISTINCT name) AS cnt_name
FROM employee
Chapter 9. Aggregate Functions
494

<!-- Original PDF Page: 496 -->

GROUP BY dept_no
See also
SELECT.
9.2.3. LIST()
Concatenates values into a string list
Result type
BLOB
Syntax
LIST ([ALL | DISTINCT] <expr> [, separator ])
Table 216. LIST Function Parameters
Parameter Description
expr Expression. It may contain a table column, a constant, a variable, an
expression, a non-aggregate function or a UDF that returns the string
data type or a BLOB. Fields of numeric and date/time types are converted
to strings. Aggregate functions are not allowed as expressions.
separator Optional alternative separator, a string expression. Comma is the default
separator
LIST returns a string consisting of the non- NULL argument values in the group, separated either by a
comma or by a user-supplied separator. If there are no non- NULL values (this includes the case
where the group is empty), NULL is returned.
• ALL (the default) results in all non- NULL values being listed. With DISTINCT, duplicates are
removed, except if expr is a BLOB.
• The optional separator argument may be any string expression. This makes it possible to specify
e.g. ascii_char(13) as a separator.
• The expr and separator arguments support BLOBs of any size and character set.
• Datetime and numeric arguments are implicitly converted to strings before concatenation.
• The result is a text BLOB, except when expr is a BLOB of another subtype.
• The ordering of the list values is undefined — the order in which the strings are concatenated is
determined by read order from the source set which, in tables, is not generally defined. If
ordering is important, the source data can be pre-sorted using a derived table or similar.

This is a trick/workaround, and it depends on implementation details of the
optimizer/execution order. This trick doesn’t always work, and it is not
guaranteed to work across versions.
Some reports indicate this no longer works in Firebird 5.0, or only in more
Chapter 9. Aggregate Functions
495

<!-- Original PDF Page: 497 -->

limited circumstances than in previous versions.
LIST Examples
1. Retrieving the list, order undefined:
SELECT LIST (display_name, '; ') FROM GR_WORK;
2. Retrieving the list in alphabetical order, using a derived table:
SELECT LIST (display_name, '; ')
FROM (SELECT display_name
      FROM GR_WORK
      ORDER BY display_name);
See also
SELECT
9.2.4. MAX()
Maximum
Result type
Returns a result of the same data type the input expression.
Syntax
MAX ([ALL | DISTINCT] <expr>)
Table 217. MAX Function Parameters
Parameter Description
expr Expression. It may contain a table column, a constant, a variable, an
expression, a non-aggregate function or a UDF. Aggregate functions are
not allowed as expressions.
MAX returns the maximum non-NULL element in the result set.
• If the group is empty or contains only NULLs, the result is NULL.
• If the input argument is a string, the function will return the value that will be sorted last if
COLLATE is used.
• This function fully supports text BLOBs of any size and character set.
 The DISTINCT parameter makes no sense if used with MAX() as it doesn’t change the
result; it is implemented only for compliance with the standard.
Chapter 9. Aggregate Functions
496

<!-- Original PDF Page: 498 -->

MAX Examples
SELECT
  dept_no,
  MAX(salary)
FROM employee
GROUP BY dept_no
See also
MIN(), SELECT
9.2.5. MIN()
Minimum
Result type
Returns a result of the same data type the input expression.
Syntax
MIN ([ALL | DISTINCT] <expr>)
Table 218. MIN Function Parameters
Parameter Description
expr Expression. It may contain a table column, a constant, a variable, an
expression, a non-aggregate function or a UDF. Aggregate functions are
not allowed as expressions.
MIN returns the minimum non-NULL element in the result set.
• If the group is empty or contains only NULLs, the result is NULL.
• If the input argument is a string, the function will return the value that will be sorted first if
COLLATE is used.
• This function fully supports text BLOBs of any size and character set.
 The DISTINCT parameter makes no sense if used with MIN() as it doesn’t change the
result; it is implemented only for compliance with the standard.
MIN Examples
SELECT
  dept_no,
  MIN(salary)
FROM employee
GROUP BY dept_no
Chapter 9. Aggregate Functions
497

<!-- Original PDF Page: 499 -->

See also
MAX(), SELECT
9.2.6. SUM()
Sum
Result type
Depends on the input type
Syntax
SUM ([ALL | DISTINCT] <expr>)
Table 219. SUM Function Parameters
Parameter Description
expr Numeric expression. It may contain a table column, a constant, a
variable, an expression, a non-aggregate function or a UDF. Aggregate
functions are not allowed as expressions.
SUM calculates and returns the sum of non-NULL values in the group.
• If the group is empty or contains only NULLs, the result is NULL.
• ALL is the default option — all values in the set that are not NULL are processed. If DISTINCT is
specified, duplicates are removed from the set and the SUM evaluation is done afterwards.
The result type of SUM depends on the input type:
FLOAT, DOUBLE PRECISION DOUBLE PRECISION
SMALLINT, INTEGER BIGINT
BIGINT, INT128 INT128
DECIMAL/NUMERIC(p, n) with p < 10 DECIMAL/NUMERIC(18, n)
DECIMAL/NUMERIC(p, n) with p >= 10 DECIMAL/NUMERIC(38, n)
DECFLOAT(16), DECFLOAT(34) DECFLOAT(34)
SUM Examples
SELECT
  dept_no,
  SUM (salary),
FROM employee
GROUP BY dept_no
See also
SELECT
Chapter 9. Aggregate Functions
498

<!-- Original PDF Page: 500 -->

9.3. Statistical Aggregate Functions
9.3.1. CORR()
Correlation coefficient
Result type
DOUBLE PRECISION
Syntax
CORR ( <expr1>, <expr2> )
Table 220. CORR Function Parameters
Parameter Description
exprN Numeric expression. It may contain a table column, a constant, a
variable, an expression, a non-aggregate function or a UDF. Aggregate
functions are not allowed as expressions.
The CORR function return the correlation coefficient for a pair of numerical expressions.
The function CORR(<expr1>, <expr2>) is equivalent to
COVAR_POP(<expr1>, <expr2>) / (STDDEV_POP(<expr2>) * STDDEV_POP(<expr1>))
This is also known as the Pearson correlation coefficient.
In a statistical sense, correlation is the degree to which a pair of variables are linearly related. A
linear relation between variables means that the value of one variable can to a certain extent
predict the value of the other. The correlation coefficient represents the degree of correlation as a
number ranging from -1 (high inverse correlation) to 1 (high correlation). A value of 0 corresponds
to no correlation.
If the group or window is empty, or contains only NULL values, the result will be NULL.
CORR Examples
select
  corr(alength, aheight) AS c_corr
from measure
See also
COVAR_POP(), STDDEV_POP()
Chapter 9. Aggregate Functions
499

<!-- Original PDF Page: 501 -->

9.3.2. COVAR_POP()
Population covariance
Result type
DOUBLE PRECISION
Syntax
COVAR_POP ( <expr1>, <expr2> )
Table 221. COVAR_POP Function Parameters
Parameter Description
exprN Numeric expression. It may contain a table column, a constant, a
variable, an expression, a non-aggregate function or a UDF. Aggregate
functions are not allowed as expressions.
The function COVAR_POP returns the population covariance for a pair of numerical expressions.
The function COVAR_POP(<expr1>, <expr2>) is equivalent to
(SUM(<expr1> * <expr2>) - SUM(<expr1>) * SUM(<expr2>) / COUNT(*)) / COUNT(*)
If the group or window is empty, or contains only NULL values, the result will be NULL.
COVAR_POP Examples
select
  covar_pop(alength, aheight) AS c_covar_pop
from measure
See also
COVAR_SAMP(), SUM(), COUNT()
9.3.3. COVAR_SAMP()
Sample covariance
Result type
DOUBLE PRECISION
Syntax
COVAR_SAMP ( <expr1>, <expr2> )
Table 222. COVAR_SAMP Function Parameters
Chapter 9. Aggregate Functions
500

<!-- Original PDF Page: 502 -->

Parameter Description
exprN Numeric expression. It may contain a table column, a constant, a
variable, an expression, a non-aggregate function or a UDF. Aggregate
functions are not allowed as expressions.
The function COVAR_SAMP returns the sample covariance for a pair of numerical expressions.
The function COVAR_SAMP(<expr1>, <expr2>) is equivalent to
(SUM(<expr1> * <expr2>) - SUM(<expr1>) * SUM(<expr2>) / COUNT(*)) / (COUNT(*) - 1)
If the group or window is empty, contains only 1 row, or contains only NULL values, the result will be
NULL.
COVAR_SAMP Examples
select
  covar_samp(alength, aheight) AS c_covar_samp
from measure
See also
COVAR_POP(), SUM(), COUNT()
9.3.4. STDDEV_POP()
Population standard deviation
Result type
DOUBLE PRECISION or NUMERIC depending on the type of expr
Syntax
STDDEV_POP ( <expr> )
Table 223. STDDEV_POP Function Parameters
Parameter Description
expr Numeric expression. It may contain a table column, a constant, a
variable, an expression, a non-aggregate function or a UDF. Aggregate
functions are not allowed as expressions.
The function STDDEV_POP returns the population standard deviation for a group or window. NULL
values are skipped.
The function STDDEV_POP(<expr>) is equivalent to
Chapter 9. Aggregate Functions
501

<!-- Original PDF Page: 503 -->

SQRT(VAR_POP(<expr>))
If the group or window is empty, or contains only NULL values, the result will be NULL.
STDDEV_POP Examples
select
  dept_no
  stddev_pop(salary)
from employee
group by dept_no
See also
STDDEV_SAMP(), VAR_POP(), SQRT
9.3.5. STDDEV_SAMP()
Sample standard deviation
Result type
DOUBLE PRECISION or NUMERIC depending on the type of expr
Syntax
STDDEV_POP ( <expr> )
Table 224. STDDEV_SAMP Function Parameters
Parameter Description
expr Numeric expression. It may contain a table column, a constant, a
variable, an expression, a non-aggregate function or a UDF. Aggregate
functions are not allowed as expressions.
The function STDDEV_SAMP returns the sample standard deviation for a group or window. NULL values
are skipped.
The function STDDEV_SAMP(<expr>) is equivalent to
SQRT(VAR_SAMP(<expr>))
If the group or window is empty, contains only 1 row, or contains only NULL values, the result will be
NULL.
STDDEV_SAMP Examples
Chapter 9. Aggregate Functions
502

<!-- Original PDF Page: 504 -->

select
  dept_no
  stddev_samp(salary)
from employee
group by dept_no
See also
STDDEV_POP(), VAR_SAMP(), SQRT
9.3.6. VAR_POP()
Population variance
Result type
DOUBLE PRECISION or NUMERIC depending on the type of expr
Syntax
VAR_POP ( <expr> )
Table 225. VAR_POP Function Parameters
Parameter Description
expr Numeric expression. It may contain a table column, a constant, a
variable, an expression, a non-aggregate function or a UDF. Aggregate
functions are not allowed as expressions.
The function VAR_POP returns the population variance for a group or window. NULL values are
skipped.
The function VAR_POP(<expr>) is equivalent to
(SUM(<expr> * <expr>) - SUM (<expr>) * SUM (<expr>) / COUNT(<expr>))
  / COUNT (<expr>)
If the group or window is empty, or contains only NULL values, the result will be NULL.
VAR_POP Examples
select
  dept_no
  var_pop(salary)
from employee
group by dept_no
See also
Chapter 9. Aggregate Functions
503

<!-- Original PDF Page: 505 -->

VAR_SAMP(), SUM(), COUNT()
9.3.7. VAR_SAMP()
Sample variance
Result type
DOUBLE PRECISION or NUMERIC depending on the type of expr
Syntax
VAR_SAMP ( <expr> )
Table 226. VAR_SAMP Function Parameters
Parameter Description
expr Numeric expression. It may contain a table column, a constant, a
variable, an expression, a non-aggregate function or a UDF. Aggregate
functions are not allowed as expressions.
The function VAR_POP returns the sample variance for a group or window. NULL values are skipped.
The function VAR_SAMP(<expr>) is equivalent to
(SUM(<expr> * <expr>) - SUM(<expr>) * SUM (<expr>) / COUNT (<expr>))
  / (COUNT(<expr>) - 1)
If the group or window is empty, contains only 1 row, or contains only NULL values, the result will be
NULL.
VAR_SAMP Examples
select
  dept_no
  var_samp(salary)
from employee
group by dept_no
See also
VAR_POP(), SUM(), COUNT()
9.4. Linear Regression Aggregate Functions
Linear regression functions are useful for trend line continuation. The trend or regression line is
usually a pattern followed by a set of values. Linear regression is useful to predict future values. To
continue the regression line, you need to know the slope and the point of intersection with the y-
Chapter 9. Aggregate Functions
504

<!-- Original PDF Page: 506 -->

axis. As set of linear functions can be used for calculating these values.
In the function syntax, y is interpreted as an x-dependent variable.
The linear regression aggregate functions take a pair of arguments, the dependent variable
expression ( y) and the independent variable expression ( x), which are both numeric value
expressions. Any row in which either argument evaluates to NULL is removed from the rows that
qualify. If there are no rows that qualify, then the result of REGR_COUNT is 0 (zero), and the other
linear regression aggregate functions result in NULL.
9.4.1. REGR_AVGX()
Average of the independent variable of the regression line
Result type
DOUBLE PRECISION
Syntax
REGR_AVGX ( <y>, <x> )
Table 227. REGR_AVGX Function Parameters
Parameter Description
y Dependent variable of the regression line. It may contain a table column,
a constant, a variable, an expression, a non-aggregate function or a UDF.
Aggregate functions are not allowed as expressions.
x Independent variable of the regression line. It may contain a table
column, a constant, a variable, an expression, a non-aggregate function
or a UDF. Aggregate functions are not allowed as expressions.
The function REGR_AVGX calculates the average of the independent variable (x) of the regression line.
The function REGR_AVGX(<y>, <x>) is equivalent to
SUM(<exprX>) / REGR_COUNT(<y>, <x>)
<exprX> :==
  CASE WHEN <x> IS NOT NULL AND <y> IS NOT NULL THEN <x> END
See also
REGR_AVGY(), REGR_COUNT(), SUM()
9.4.2. REGR_AVGY()
Average of the dependent variable of the regression line
Result type
Chapter 9. Aggregate Functions
505

<!-- Original PDF Page: 507 -->

DOUBLE PRECISION
Syntax
REGR_AVGY ( <y>, <x> )
Table 228. REGR_AVGY Function Parameters
Parameter Description
y Dependent variable of the regression line. It may contain a table column,
a constant, a variable, an expression, a non-aggregate function or a UDF.
Aggregate functions are not allowed as expressions.
x Independent variable of the regression line. It may contain a table
column, a constant, a variable, an expression, a non-aggregate function
or a UDF. Aggregate functions are not allowed as expressions.
The function REGR_AVGY calculates the average of the dependent variable (y) of the regression line.
The function REGR_AVGY(<y>, <x>) is equivalent to
SUM(<exprY>) / REGR_COUNT(<y>, <x>)
<exprY> :==
  CASE WHEN <x> IS NOT NULL AND <y> IS NOT NULL THEN <y> END
See also
REGR_AVGX(), REGR_COUNT(), SUM()
9.4.3. REGR_COUNT()
Number of non-empty pairs of the regression line
Result type
DOUBLE PRECISION
Syntax
REGR_COUNT ( <y>, <x> )
Table 229. REGR_COUNT Function Parameters
Parameter Description
y Dependent variable of the regression line. It may contain a table column,
a constant, a variable, an expression, a non-aggregate function or a UDF.
Aggregate functions are not allowed as expressions.
Chapter 9. Aggregate Functions
506

<!-- Original PDF Page: 508 -->

Parameter Description
x Independent variable of the regression line. It may contain a table
column, a constant, a variable, an expression, a non-aggregate function
or a UDF. Aggregate functions are not allowed as expressions.
The function REGR_COUNT counts the number of non-empty pairs of the regression line.
The function REGR_COUNT(<y>, <x>) is equivalent to
COUNT(*) FILTER (WHERE <x> IS NOT NULL AND <y> IS NOT NULL)
See also
COUNT()
9.4.4. REGR_INTERCEPT()
Point of intersection of the regression line with the y-axis
Result type
DOUBLE PRECISION
Syntax
REGR_INTERCEPT ( <y>, <x> )
Table 230. REGR_INTERCEPT Function Parameters
Parameter Description
y Dependent variable of the regression line. It may contain a table column,
a constant, a variable, an expression, a non-aggregate function or a UDF.
Aggregate functions are not allowed as expressions.
x Independent variable of the regression line. It may contain a table
column, a constant, a variable, an expression, a non-aggregate function
or a UDF. Aggregate functions are not allowed as expressions.
The function REGR_INTERCEPT calculates the point of intersection of the regression line with the y-
axis.
The function REGR_INTERCEPT(<y>, <x>) is equivalent to
REGR_AVGY(<y>, <x>) - REGR_SLOPE(<y>, <x>) * REGR_AVGX(<y>, <x>)
REGR_INTERCEPT Examples
Forecasting sales volume
Chapter 9. Aggregate Functions
507

<!-- Original PDF Page: 509 -->

with recursive years (byyear) as (
  select 1991
  from rdb$database
  union all
  select byyear + 1
  from years
  where byyear < 2020
),
s as (
  select
    extract(year from order_date) as byyear,
    sum(total_value) as total_value
  from sales
  group by 1
),
regr as (
  select
    regr_intercept(total_value, byyear) as intercept,
    regr_slope(total_value, byyear) as slope
  from s
)
select
  years.byyear as byyear,
  intercept + (slope * years.byyear) as total_value
from years
cross join regr
BYYEAR TOTAL_VALUE
------ ------------
  1991    118377.35
  1992    414557.62
  1993    710737.89
  1994   1006918.16
  1995   1303098.43
  1996   1599278.69
  1997   1895458.96
  1998   2191639.23
  1999   2487819.50
  2000   2783999.77
...
See also
REGR_AVGX(), REGR_AVGY(), REGR_SLOPE()
9.4.5. REGR_R2()
Coefficient of determination of the regression line
Chapter 9. Aggregate Functions
508

<!-- Original PDF Page: 510 -->

Result type
DOUBLE PRECISION
Syntax
REGR_R2 ( <y>, <x> )
Table 231. REGR_R2 Function Parameters
Parameter Description
y Dependent variable of the regression line. It may contain a table column,
a constant, a variable, an expression, a non-aggregate function or a UDF.
Aggregate functions are not allowed as expressions.
x Independent variable of the regression line. It may contain a table
column, a constant, a variable, an expression, a non-aggregate function
or a UDF. Aggregate functions are not allowed as expressions.
The REGR_R2 function calculates the coefficient of determination, or R-squared, of the regression
line.
The function REGR_R2(<y>, <x>) is equivalent to
POWER(CORR(<y>, <x>), 2)
See also
CORR(), POWER
9.4.6. REGR_SLOPE()
Slope of the regression line
Result type
DOUBLE PRECISION
Syntax
REGR_SLOPE ( <y>, <x> )
Table 232. REGR_SLOPE Function Parameters
Parameter Description
y Dependent variable of the regression line. It may contain a table column,
a constant, a variable, an expression, a non-aggregate function or a UDF.
Aggregate functions are not allowed as expressions.
x Independent variable of the regression line. It may contain a table
column, a constant, a variable, an expression, a non-aggregate function
or a UDF. Aggregate functions are not allowed as expressions.
Chapter 9. Aggregate Functions
509

<!-- Original PDF Page: 511 -->

The function REGR_SLOPE calculates the slope of the regression line.
The function REGR_SLOPE(<y>, <x>) is equivalent to
COVAR_POP(<y>, <x>) / VAR_POP(<exprX>)
<exprX> :==
  CASE WHEN <x> IS NOT NULL AND <y> IS NOT NULL THEN <x> END
See also
COVAR_POP(), VAR_POP()
9.4.7. REGR_SXX()
Sum of squares of the independent variable
Result type
DOUBLE PRECISION
Syntax
REGR_SXX ( <y>, <x> )
Table 233. REGR_SXX Function Parameters
Parameter Description
y Dependent variable of the regression line. It may contain a table column,
a constant, a variable, an expression, a non-aggregate function or a UDF.
Aggregate functions are not allowed as expressions.
x Independent variable of the regression line. It may contain a table
column, a constant, a variable, an expression, a non-aggregate function
or a UDF. Aggregate functions are not allowed as expressions.
The function REGR_SXX calculates the sum of squares of the independent expression variable (x).
The function REGR_SXX(<y>, <x>) is equivalent to
REGR_COUNT(<y>, <x>) * VAR_POP(<exprX>)
<exprX> :==
  CASE WHEN <x> IS NOT NULL AND <y> IS NOT NULL THEN <x> END
See also
REGR_COUNT(), VAR_POP()
Chapter 9. Aggregate Functions
510

<!-- Original PDF Page: 512 -->

9.4.8. REGR_SXY()
Sum of products of the independent variable and the dependent variable
Result type
DOUBLE PRECISION
Syntax
REGR_SXY ( <y>, <x> )
Table 234. REGR_SXY Function Parameters
Parameter Description
y Dependent variable of the regression line. It may contain a table column,
a constant, a variable, an expression, a non-aggregate function or a UDF.
Aggregate functions are not allowed as expressions.
x Independent variable of the regression line. It may contain a table
column, a constant, a variable, an expression, a non-aggregate function
or a UDF. Aggregate functions are not allowed as expressions.
The function REGR_SXY calculates the sum of products of independent variable expression ( x) times
dependent variable expression (y).
The function REGR_SXY(<y>, <x>) is equivalent to
REGR_COUNT(<y>, <x>) * COVAR_POP(<y>, <x>)
See also
COVAR_POP(), REGR_COUNT()
9.4.9. REGR_SYY()
Sum of squares of the dependent variable
Result type
DOUBLE PRECISION
Syntax
REGR_SYY ( <y>, <x> )
Table 235. REGR_SYY Function Parameters
Chapter 9. Aggregate Functions
511