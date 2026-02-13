# 10 Window Functions



<!-- Original PDF Page: 513 -->

Parameter Description
y Dependent variable of the regression line. It may contain a table column,
a constant, a variable, an expression, a non-aggregate function or a UDF.
Aggregate functions are not allowed as expressions.
x Independent variable of the regression line. It may contain a table
column, a constant, a variable, an expression, a non-aggregate function
or a UDF. Aggregate functions are not allowed as expressions.
The function REGR_SYY calculates the sum of squares of the dependent variable (y).
The function REGR_SYY(<y>, <x>) is equivalent to
REGR_COUNT(<y>, <x>) * VAR_POP(<exprY>)
<exprY> :==
  CASE WHEN <x> IS NOT NULL AND <y> IS NOT NULL THEN <y> END
See also
REGR_COUNT(), VAR_POP()
Chapter 9. Aggregate Functions
512

<!-- Original PDF Page: 514 -->

Chapter 10. Window (Analytical) Functions
Window functions (also known as analytical functions) are a kind of aggregation, but one that does
not “reduce” a group into a single row. The columns of aggregated data are mixed with the query
result set.
The window functions are used with the OVER clause. They may appear only in the SELECT list, or the
ORDER BY clause of a query.
Firebird window functions may be partitioned and ordered.
Window functions are available in DSQL and PSQL. Availability in ESQL is not tracked by this
Language Reference.
Syntax
<window_function> ::=
    <aggregate-function> OVER <window-name-or-spec>
  | <window-function-name> ([<value-expression> [, <value-expression> ...]])
    OVER <window-name-or-spec>
<aggregate-function> ::=
  !! See Aggregate Functions !!
<window-name-or-spec> ::=
  (<window-specification-details>) | existing_window_name
<window-function-name> ::=
    <ranking-function>
  | <navigational-function>
<ranking-function> ::=
    RANK | DENSE_RANK | PERCENT_RANK | ROW_NUMBER
  | CUME_DIST | NTILE
<navigational-function>
  LEAD | LAG | FIRST_VALUE | LAST_VALUE | NTH_VALUE
<window-specification-details> ::=
  [existing-window-name]
    [<window-partition-clause>]
    [<order-by-clause>]
    [<window-frame-clause>]
<window-partition-clause> ::=
  PARTITION BY <value-expression> [, <value-expression> ...]
<order-by-clause> ::=
  ORDER BY <sort-specification [, <sort-specification> ...]
Chapter 10. Window (Analytical) Functions
513

<!-- Original PDF Page: 515 -->

<sort-specification> ::=
  <value-expression> [<ordering-specification>] [<null-ordering>]
<ordering-specification> ::=
    ASC  | ASCENDING
  | DESC | DESCENDING
<null-ordering> ::=
    NULLS FIRST
  | NULLS LAST
<window-frame-clause> ::= { RANGE | ROWS | GROUPS } <window-frame-extent> [<window-frame-exclusion>]
<window-frame-extent> ::=
    <window-frame-start>
  | <window-frame-between>
<window-frame-start> ::=
    UNBOUNDED PRECEDING
  | <value-expression> PRECEDING
  | CURRENT ROW
<window-frame-between> ::=
  BETWEEN { UNBOUNDED PRECEDING | <value-expression> PRECEDING
          | CURRENT ROW | <value-expression> FOLLOWING }
  AND { <value-expression> PRECEDING | CURRENT ROW
      | <value-expression> FOLLOWING | UNBOUNDED FOLLOWING }
Table 236. Window Function Arguments
Argument Description
value-expression Expression. May contain a table column, constant, variable, expression,
scalar or aggregate function. Window functions are not allowed as an
expression.
aggregate-function An aggregate function used as a window function
existing-window-name A named window defined using the WINDOW clause of the current query
specification.
window-frame-exclusion An optional clause that excludes specific rows from the window frame.
Four exclusion types are supported: EXCLUDE CURRENT ROW (exclude the
current row), EXCLUDE GROUP (exclude the current row and its peers),
EXCLUDE TIES (exclude peers of the current row but not the current row),
and EXCLUDE NO OTHERS (exclude nothing, the default).
10.1. Aggregate Functions as Window Functions
All aggregate functions  — including FILTER clause — can be used as window functions, by adding
the OVER clause.
Imagine a table EMPLOYEE with columns ID, NAME and SALARY, and the need to show each employee
with their respective salary and the percentage of their salary over the payroll.
A normal query could achieve this, as follows:
Chapter 10. Window (Analytical) Functions
514

<!-- Original PDF Page: 516 -->

select
    id,
    department,
    salary,
    salary / (select sum(salary) from employee) portion
  from employee
  order by id;
Results
id  department  salary  portion
--  ----------  ------  ----------
1   R & D        10.00      0.2040
2   SALES        12.00      0.2448
3   SALES         8.00      0.1632
4   R & D         9.00      0.1836
5   R & D        10.00      0.2040
The query is repetitive and lengthy to run, especially if EMPLOYEE happens to be a complex view.
The same query could be specified in a much faster and more elegant way using a window
function:
select
    id,
    department,
    salary,
    salary / sum(salary) OVER () portion
  from employee
  order by id;
Here, sum(salary) over ()  is computed with the sum of all SALARY from the query (the EMPLOYEE
table).
10.2. Partitioning
Like aggregate functions, that may operate alone or in relation to a group, window functions may
also operate on a group, which is called a “partition”.
Syntax
<window function>(...) OVER (PARTITION BY <expr> [, <expr> ...])
Aggregation over a group could produce more than one row, so the result set generated by a
partition is joined with the main query using the same expression list as the partition.
Continuing the EMPLOYEE example, instead of getting the portion of each employee’s salary over the
Chapter 10. Window (Analytical) Functions
515

<!-- Original PDF Page: 517 -->

all-employees total, we would like to get the portion based on the employees in the same
department:
select
    id,
    department,
    salary,
    salary / sum(salary) OVER (PARTITION BY department) portion
  from employee
  order by id;
Results
id  department  salary  portion
--  ----------  ------  ----------
1   R & D        10.00      0.3448
2   SALES        12.00      0.6000
3   SALES         8.00      0.4000
4   R & D         9.00      0.3103
5   R & D        10.00      0.3448
10.3. Ordering
The ORDER BY sub-clause can be used with or without partitions. The ORDER BY clause within OVER
specifies the order in which the window function will process rows. This order does not have to be
the same as the order rows appear in the output.
There is an important concept associated with window functions: for each row there is a set of rows
in its partition called the window frame. By default, when specifying ORDER BY, the frame consists of
all rows from the beginning of the partition to the current row and rows equal to the current ORDER
BY expression. Without ORDER BY, the default frame consists of all rows in the partition.
As a result, for standard aggregate functions, the ORDER BY clause produces partial aggregation
results as rows are processed.
Example
select
    id,
    salary,
    sum(salary) over (order by salary) cumul_salary
  from employee
  order by salary;
Results
id  salary  cumul_salary
--  ------  ------------
Chapter 10. Window (Analytical) Functions
516

<!-- Original PDF Page: 518 -->

3     8.00          8.00
4     9.00         17.00
1    10.00         37.00
5    10.00         37.00
2    12.00         49.00
Then cumul_salary returns the partial/accumulated (or running) aggregation (of the SUM function). It
may appear strange that 37.00 is repeated for the ids 1 and 5, but that is how it should work. The
ORDER BY keys are grouped together, and the aggregation is computed once (but summing the two
10.00). To avoid this, you can add the ID field to the end of the ORDER BY clause.
It’s possible to use multiple windows with different orders, and ORDER BY parts like ASC/DESC and
NULLS FIRST/LAST.
With a partition, ORDER BY works the same way, but at each partition boundary the aggregation is
reset.
All aggregation functions can use ORDER BY, except for LIST().
10.4. Window Frames
A window frame specifies which rows to consider for the current row when evaluating the window
function.
The frame comprises three pieces: unit, start bound, and end bound. The unit can be RANGE or ROWS,
which defines how the bounds will work.
The bounds are:
UNBOUNDED PRECEDING
<expr> PRECEDING
CURRENT ROW
<expr> FOLLOWING
UNBOUNDED FOLLOWING
• With RANGE, the ORDER BY should specify exactly one expression, and that expression should be of
a numeric, date, time, or timestamp type. For <expr> PRECEDING, expr is subtracted from the ORDER
BY expression, and for <expr> FOLLOWING, expr is added. For CURRENT ROW, the expression is used
as-is.
All rows inside the current partition that are between the bounds are considered part of the
resulting window frame.
• With ROWS, ORDER BY  expressions are not limited by number or type. For this unit, <expr>
PRECEDING and <expr FOLLOWING relate to the row position within the current partition, and not
the values of the ordering keys.
Both UNBOUNDED PRECEDING and UNBOUNDED FOLLOWING work identical with RANGE and ROWS. UNBOUNDED
Chapter 10. Window (Analytical) Functions
517

<!-- Original PDF Page: 519 -->

PRECEDING start at the first row of the current partition, and UNBOUNDED FOLLOWING ends at the last row
of the current partition.
The frame syntax with <window-frame-start> specifies the start-frame, with the end-frame being
CURRENT ROW.
Some window functions discard frames:
• ROW_NUMBER, LAG and LEAD always work as ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
• DENSE_RANK, RANK, PERCENT_RANK and CUME_DIST always work as RANGE BETWEEN UNBOUNDED PRECEDING
AND CURRENT ROW
• FIRST_VALUE, LAST_VALUE and NTH_VALUE respect frames, but the RANGE unit behaviour is identical to
ROWS.
Example Using Frame
When the ORDER BY clause is used, but a frame clause is omitted, the default considers the partition
up to the current row. When combined with SUM, this results in a running total:
select
  id,
  salary,
  sum(salary) over (order by salary) sum_salary
from employee
order by salary;
Result:
| id | salary | sum_salary |
|---:|-------:|-----------:|
|  3 |   8.00 |       8.00 |
|  4 |   9.00 |      17.00 |
|  1 |  10.00 |      37.00 |
|  5 |  10.00 |      37.00 |
|  2 |  12.00 |      49.00 |
On the other hand, if we apply a frame for the entire partition, we get the total for the entire
partition.
select
  id,
  salary,
  sum(salary) over (
    order by salary
    ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
  ) sum_salary
from employee
Chapter 10. Window (Analytical) Functions
518

<!-- Original PDF Page: 520 -->

order by salary;
Result:
| id | salary | sum_salary |
|---:|-------:|-----------:|
|  3 |   8.00 |      49.00 |
|  4 |   9.00 |      49.00 |
|  1 |  10.00 |      49.00 |
|  5 |  10.00 |      49.00 |
|  2 |  12.00 |      49.00 |
This example is to demonstrate how this works; the result of this example would be simpler to
produce with sum(salary) over().
We can use a range frame to compute the count of employees with salaries between (an employee’s
salary - 1) and (their salary + 1) with this query:
select
  id,
  salary,
  count(*) over (
    order by salary
    RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING
  ) range_count
from employee
order by salary;
Result:
| id | salary | range_count |
|---:|-------:|------------:|
|  3 |   8.00 |           2 |
|  4 |   9.00 |           4 |
|  1 |  10.00 |           3 |
|  5 |  10.00 |           3 |
|  2 |  12.00 |           1 |
10.4.1. GROUPS Frame Unit
The GROUPS frame unit is an extension to the ROWS and RANGE frame units, introduced in SQL:2016.
Where ROWS operates on physical row positions and RANGE operates on logical value ranges, GROUPS
operates on groups of peer rows — rows that have identical values in the ORDER BY expression(s).

The GROUPS frame unit is particularly useful when you want to include entire groups of peers in
calculations, even when specifying offsets.

Syntax:
GROUPS BETWEEN <frame-start> AND <frame-end>

Example with GROUPS Frame:
-- Compare behavior of ROWS, RANGE, and GROUPS with duplicate salary values
select
  id,
  salary,
  count(*) over (
    order by salary
    ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING
  ) as rows_count,
  count(*) over (
    order by salary
    RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING
  ) as range_count,
  count(*) over (
    order by salary
    GROUPS BETWEEN 1 PRECEDING AND 1 FOLLOWING
  ) as groups_count
from employee
order by salary;

Result with GROUPS:
| id | salary | rows_count | range_count | groups_count |
|---:|-------:|-----------:|------------:|-------------:|
|  3 |   8.00 |          2 |           2 |            4 |
|  4 |   9.00 |          3 |           4 |            5 |
|  1 |  10.00 |          4 |           3 |            5 |
|  5 |  10.00 |          4 |           3 |            5 |
|  2 |  12.00 |          2 |           1 |            3 |

For salary 8.00 with GROUPS BETWEEN 1 PRECEDING AND 1 FOLLOWING:
- Includes current group (salary 8.00: 1 row)
- Includes next group (salary 9.00: 1 row)
- Includes group after that (salary 10.00: 2 rows)
- Total: 4 rows

Note: Unlike RANGE which requires a single ORDER BY expression of numeric/date/time type, GROUPS
works with any number and type of ORDER BY expressions.

10.4.2. Frame Exclusion Clauses
Frame exclusion allows fine-grained control over which rows within a frame are included in the
window function calculation. This feature is part of SQL:2016 and provides four exclusion modes:

EXCLUDE NO OTHERS (default)
  Include all rows in the frame. This is the default behavior when no exclusion clause is specified.

EXCLUDE CURRENT ROW
  Exclude only the current row from the frame, but include its peers (rows with identical ORDER BY values).

EXCLUDE GROUP
  Exclude the current row and all its peers (the entire peer group).

EXCLUDE TIES
  Exclude peers of the current row, but keep the current row itself.

Syntax:
{ ROWS | RANGE | GROUPS } <frame-extent>
  [ EXCLUDE CURRENT ROW | EXCLUDE GROUP | EXCLUDE TIES | EXCLUDE NO OTHERS ]

Example with Frame Exclusions:
-- Calculate cumulative salary excluding different row sets
select
  id,
  salary,
  sum(salary) over (
    order by salary
    ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
  ) as total_all,
  sum(salary) over (
    order by salary
    ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    EXCLUDE CURRENT ROW
  ) as total_excl_current,
  sum(salary) over (
    order by salary
    RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    EXCLUDE GROUP
  ) as total_excl_group,
  sum(salary) over (
    order by salary
    RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    EXCLUDE TIES
  ) as total_excl_ties
from employee
order by id;

Result:
| id | salary | total_all | total_excl_current | total_excl_group | total_excl_ties |
|---:|-------:|----------:|-------------------:|-----------------:|----------------:|
|  1 |  10.00 |     49.00 |              39.00 |            29.00 |           39.00 |
|  2 |  12.00 |     49.00 |              37.00 |            37.00 |           49.00 |
|  3 |   8.00 |     49.00 |              41.00 |            41.00 |           49.00 |
|  4 |   9.00 |     49.00 |              40.00 |            40.00 |           49.00 |
|  5 |  10.00 |     49.00 |              39.00 |            29.00 |           39.00 |

For employee id=1 with salary 10.00:
- total_all: All 5 employees = 49.00
- total_excl_current: Excludes id=1 only = 39.00
- total_excl_group: Excludes both employees with salary 10.00 (ids 1 and 5) = 29.00
- total_excl_ties: Excludes peers (id=5) but keeps current row (id=1) = 39.00

Use Cases for Frame Exclusions:
- EXCLUDE CURRENT ROW: Comparing a value against all others (e.g., "this salary vs. average of all other salaries")
- EXCLUDE GROUP: Calculating aggregates that exclude the current peer group entirely
- EXCLUDE TIES: Including the current row but excluding its duplicates when computing ranks or percentiles

10.5. Named Windows
The WINDOW clause  can be used to explicitly name a window, for example to avoid repetitive or
confusing expressions.
A named window can be used
a. in the OVER clause to reference a window definition, e.g. OVER window_name
Chapter 10. Window (Analytical) Functions
519

<!-- Original PDF Page: 521 -->

b. as a base window of another named or inline ( OVER) window, if it is not a window with a frame
(ROWS or RANGE clauses)
 A window with a base windows cannot have PARTITION BY, nor override the
ordering (ORDER BY) of a base window.
10.6. Ranking Functions
The ranking functions compute the ordinal rank of a row within the window partition.
These functions can be used with or without partitioning and ordering. However, using them
without ordering almost never makes sense.
The ranking functions can be used to create different type of counters. Consider SUM(1) OVER (ORDER
BY SALARY) as an example of what they can do, each of them differently. Following is an example
query, also comparing with the SUM behavior.
select
    id,
    salary,
    dense_rank() over (order by salary),
    rank() over (order by salary),
    row_number() over (order by salary),
    sum(1) over (order by salary)
  from employee
  order by salary;
Results
id  salary  dense_rank  rank  row_number  sum
--  ------  ----------  ----  ----------  ---
 3    8.00           1     1           1    1
 4    9.00           2     2           2    2
 1   10.00           3     3           3    4
 5   10.00           3     3           4    4
 2   12.00           4     5           5    5
The difference between DENSE_RANK and RANK is that there is a gap related to duplicate rows (relative
to the window ordering) only in RANK. DENSE_RANK continues assigning sequential numbers after the
duplicate salary. On the other hand, ROW_NUMBER always assigns sequential numbers, even when
there are duplicate values.
10.6.1. CUME_DIST()
Relative rank (or, cumulative distribution) of a row within a window partition
Result type
DOUBLE PRECISION
Chapter 10. Window (Analytical) Functions
520

<!-- Original PDF Page: 522 -->

Syntax
CUME_DIST () OVER <window_name_or_spec>
CUME_DIST is calculated as the number of rows preceding or peer of the current row divided by the
number of rows in the partition.
In other words, CUME_DIST() OVER <window_name_or_spec>  is equivalent to COUNT(*) OVER
<window_name_or_spec> / COUNT(*) OVER()
CUME_DIST Examples
select
  id,
  salary,
  cume_dist() over (order by salary)
from employee
order by salary;
Result
id salary cume_dist
-- ------ ---------
 3   8.00       0.2
 4   9.00       0.4
 1  10.00       0.8
 5  10.00       0.8
 2  12.00         1
10.6.2. DENSE_RANK()
See also RANK(), PERCENT_RANK()
Rank of rows in a partition without gaps
Result type
BIGINT
Syntax
DENSE_RANK () OVER <window_name_or_spec>
Rows with the same window_order values get the same rank within the partition window_partition,
if specified. The dense rank of a row is equal to the number of different rank values in the partition
preceding the current row, plus one.
DENSE_RANK Examples
Chapter 10. Window (Analytical) Functions
521

<!-- Original PDF Page: 523 -->

select
  id,
  salary,
  dense_rank() over (order by salary)
from employee
order by salary;
Result
id salary dense_rank
-- ------ ----------
 3  8.00           1
 4  9.00           2
 1 10.00           3
 5 10.00           3
 2 12.00           4
10.6.3. NTILE()
See also RANK(), ROW_NUMBER()
Distributes the rows of the current window partition into the specified number of tiles (groups)
Result type
BIGINT
Syntax
NTILE ( number_of_tiles ) OVER <window_name_or_spec>
Table 237. Arguments of NTILE
Argument Description
number_of_tiles Number of tiles (groups). Restricted to a positive integer literal, a named
parameter (PSQL), or a positional parameter (DSQL).
NTILE Examples
select
  id,
  salary,
  rank() over (order by salary),
  ntile(3) over (order by salary)
from employee
order by salary;
Chapter 10. Window (Analytical) Functions
522

<!-- Original PDF Page: 524 -->

Result
ID SALARY RANK NTILE
== ====== ==== =====
 3   8.00    1     1
 4   9.00    2     1
 1  10.00    3     2
 5  10.00    3     2
 2  12.00    5     3
10.6.4. PERCENT_RANK()
Relative rank of a row within a window partition.
Result type
DOUBLE PRECISION
Syntax
PERCENT_RANK () OVER <window_name_or_spec>
PERCENT_RANK is calculated as the RANK() minus 1 of the current row divided by the number of rows
in the partition minus 1.
In other words, PERCENT_RANK() OVER <window_name_or_spec>  is equivalent to (RANK() OVER
<window_name_or_spec> - 1) / CAST(COUNT(*) OVER() - 1 AS DOUBLE PRECISION)
PERCENT_RANK Examples
select
  id,
  salary,
  rank() over (order by salary),
  percent_rank() over (order by salary)
from employee
order by salary;
Result
id salary rank percent_rank
-- ------ ---- ------------
 3   8.00    1            0
 4   9.00    2         0.25
 1  10.00    3          0.5
 5  10.00    3          0.5
 2  12.00    5            1
Chapter 10. Window (Analytical) Functions
523

<!-- Original PDF Page: 525 -->

10.6.5. RANK()
See also RANK(), CUME_DIST()
Rank of each row in a partition
Result type
BIGINT
Syntax
RANK () OVER <window_name_or_spec>
Rows with the same values of window-order get the same rank with in the partition window-
partition, if specified. The rank of a row is equal to the number of rank values in the partition
preceding the current row, plus one.
RANK Examples
select
  id,
  salary,
  rank() over (order by salary)
from employee
order by salary;
Result
id salary rank
-- ------ ----
 3  8.00     1
 4  9.00     2
 1 10.00     3
 5 10.00     3
 2 12.00     5
See also
DENSE_RANK(), ROW_NUMBER()
10.6.6. ROW_NUMBER()
Sequential row number in the partition
Result type
BIGINT
Chapter 10. Window (Analytical) Functions
524

<!-- Original PDF Page: 526 -->

Syntax
ROW_NUMBER () OVER <window_name_or_spec>
Returns the sequential row number in the partition, where 1 is the first row in each of the
partitions.
ROW_NUMBER Examples
select
  id,
  salary,
  row_number() over (order by salary)
from employee
order by salary;
Result
id salary rank
-- ------ ----
 3  8.00     1
 4  9.00     2
 1 10.00     3
 5 10.00     4
 2 12.00     5
See also
DENSE_RANK(), RANK()
10.7. Navigational Functions
The navigational functions get the simple (non-aggregated) value of an expression from another
row of the query, within the same partition.

FIRST_VALUE, LAST_VALUE and NTH_VALUE also operate on a window frame. For
navigational functions, Firebird applies a default frame from the first to the
current row of the partition, not to the last. In other words, it behaves as if the
following frame is specified:
RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
This is likely to produce strange or unexpected results for NTH_VALUE and especially
LAST_VALUE, so make sure to specify an explicit frame with these functions.
Chapter 10. Window (Analytical) Functions
525

<!-- Original PDF Page: 527 -->

Example of Navigational Functions
select
    id,
    salary,
    first_value(salary) over (order by salary),
    last_value(salary) over (order by salary),
    nth_value(salary, 2) over (order by salary),
    lag(salary) over (order by salary),
    lead(salary) over (order by salary)
  from employee
  order by salary;
Results
id  salary  first_value  last_value  nth_value     lag    lead
--  ------  -----------  ----------  ---------  ------  ------
3     8.00         8.00        8.00     <null>  <null>    9.00
4     9.00         8.00        9.00       9.00    8.00   10.00
1    10.00         8.00       10.00       9.00    9.00   10.00
5    10.00         8.00       10.00       9.00   10.00   12.00
2    12.00         8.00       12.00       9.00   10.00  <null>
10.7.1. FIRST_VALUE()
First value of the current partition
Result type
The same as type as expr
Syntax
FIRST_VALUE ( <expr> )
  [ IGNORE NULLS | RESPECT NULLS ]
  OVER <window_name_or_spec>
Table 238. Arguments of FIRST_VALUE
Argument Description
expr Expression. May contain a table column, constant, variable, expression,
scalar function. Aggregate functions are not allowed as an expression.

NULL Handling:
By default, FIRST_VALUE uses RESPECT NULLS behavior, which returns the first value in the frame
even if it is NULL. When IGNORE NULLS is specified, FIRST_VALUE returns the first non-NULL value
in the frame. If all values in the frame are NULL, FIRST_VALUE returns NULL regardless of which
option is specified.

Example with IGNORE NULLS:
select
  id,
  department,
  bonus,
  first_value(bonus) RESPECT NULLS over (
    partition by department
    order by id
  ) as first_bonus_default,
  first_value(bonus) IGNORE NULLS over (
    partition by department
    order by id
  ) as first_non_null_bonus
from employee
order by department, id;

See also
LAST_VALUE(), NTH_VALUE()
10.7.2. LAG()
Value from row in the current partition with a given offset before the current row
Result type
Chapter 10. Window (Analytical) Functions
526

<!-- Original PDF Page: 528 -->

The same as type as expr
Syntax
LAG ( <expr> [, <offset [, <default>]])
  OVER <window_name_or_spec>
  [ IGNORE NULLS | RESPECT NULLS ]
Table 239. Arguments of LAG
Argument Description
expr Expression. May contain a table column, constant, variable, expression,
scalar function. Aggregate functions are not allowed as an expression.
offset The offset in rows before the current row to get the value identified by
expr. If offset is not specified, the default is 1. offset can be a column,
subquery or other expression that results in a positive integer value, or
another type that can be implicitly converted to BIGINT. offset cannot be
negative (use LEAD instead).
default The default value to return if offset points outside the partition. Default is
NULL.
The LAG function provides access to the row in the current partition with a given offset before the
current row.
If offset points outside the current partition, default will be returned, or NULL if no default was
specified.

NULL Handling:
By default, LAG uses RESPECT NULLS behavior, which returns NULL values when encountered. When
IGNORE NULLS is specified, LAG skips over NULL values and returns the first non-NULL value found
at the specified offset. This is particularly useful for carrying forward the last known non-NULL
value in time series data.

Example with IGNORE NULLS:
select
  trade_date,
  closing_price,
  lag(closing_price, 1) RESPECT NULLS over (order by trade_date) as prev_close_default,
  lag(closing_price, 1) IGNORE NULLS over (order by trade_date) as prev_close_ignore_null
from stock_prices
order by trade_date;

-- If some days have NULL closing_price (market closed), IGNORE NULLS will return
-- the most recent non-NULL closing price instead of NULL.

LAG Examples
Suppose you have RATE table that stores the exchange rate for each day. To trace the change of the
exchange rate over the past five days you can use the following query.
select
  bydate,
  cost,
  cost - lag(cost) over (order by bydate) as change,
  100 * (cost - lag(cost) over (order by bydate)) /
    lag(cost) over (order by bydate) as percent_change
from rate
where bydate between dateadd(-4 day to current_date)
and current_date
order by bydate
Result
bydate     cost   change percent_change
---------- ------ ------ --------------
27.10.2014  31.00 <null>         <null>
28.10.2014  31.53   0.53         1.7096
Chapter 10. Window (Analytical) Functions
527

<!-- Original PDF Page: 529 -->

29.10.2014  31.40  -0.13        -0.4123
30.10.2014  31.67   0.27         0.8598
31.10.2014  32.00   0.33         1.0419
See also
LEAD()
10.7.3. LAST_VALUE()
Last value from the current partition
Result type
The same as type as expr
Syntax
LAST_VALUE ( <expr> )
  [ IGNORE NULLS | RESPECT NULLS ]
  OVER <window_name_or_spec>
Table 240. Arguments of LAST_VALUE
Argument Description
expr Expression. May contain a table column, constant, variable, expression,
scalar function. Aggregate functions are not allowed as an expression.

NULL Handling:
By default, LAST_VALUE uses RESPECT NULLS behavior, which returns the last value in the frame
even if it is NULL. When IGNORE NULLS is specified, LAST_VALUE returns the last non-NULL value
in the frame. If all values in the frame are NULL, LAST_VALUE returns NULL regardless of which
option is specified.

Example with IGNORE NULLS:
select
  measurement_time,
  sensor_reading,
  last_value(sensor_reading) RESPECT NULLS over (
    order by measurement_time
    rows between unbounded preceding and unbounded following
  ) as last_reading_default,
  last_value(sensor_reading) IGNORE NULLS over (
    order by measurement_time
    rows between unbounded preceding and unbounded following
  ) as last_valid_reading
from sensor_data
order by measurement_time;

-- When the most recent sensor reading is NULL (sensor offline), IGNORE NULLS will return
-- the last valid reading before the sensor went offline.

See also note on frame for navigational functions.
See also
FIRST_VALUE(), NTH_VALUE()
10.7.4. LEAD()
Value from a row in the current partition with a given offset after the current row
Result type
The same as type as expr
Syntax
LEAD ( <expr> [, <offset [, <default>]])
  [ IGNORE NULLS | RESPECT NULLS ]
  OVER <window_name_or_spec>
Table 241. Arguments of LEAD
Argument Description
expr Expression. May contain a table column, constant, variable, expression,
scalar function. Aggregate functions are not allowed as an expression.
Chapter 10. Window (Analytical) Functions
528

<!-- Original PDF Page: 530 -->

Argument Description
offset The offset in rows after the current row to get the value identified by
expr. If offset is not specified, the default is 1. offset can be a column,
subquery or other expression that results in a positive integer value, or
another type that can be implicitly converted to BIGINT. offset cannot be
negative (use LAG instead).
default The default value to return if offset points outside the partition. Default is
NULL.
The LEAD function provides access to the row in the current partition with a given offset after the
current row.
If offset points outside the current partition, default will be returned, or NULL if no default was
specified.

NULL Handling:
By default, LEAD uses RESPECT NULLS behavior, which returns NULL values when encountered. When
IGNORE NULLS is specified, LEAD skips over NULL values and returns the first non-NULL value found
at the specified offset after the current row. This is useful for forward-filling or forecasting
with the next known non-NULL value.

Example with IGNORE NULLS:
select
  order_date,
  next_delivery_date,
  lead(next_delivery_date, 1) RESPECT NULLS over (order by order_date) as next_delivery_default,
  lead(next_delivery_date, 1) IGNORE NULLS over (order by order_date) as next_delivery_ignore_null
from orders
order by order_date;

-- When next_delivery_date is NULL (e.g., pending orders), IGNORE NULLS will look ahead
-- to find the next confirmed delivery date instead of returning NULL.

See also
LAG()
10.7.5. NTH_VALUE()
The Nth value starting from the first or the last row of the current frame
Result type
The same as type as expr
Syntax
NTH_VALUE ( <expr>, <offset> )
  [FROM {FIRST | LAST}]
  [ IGNORE NULLS | RESPECT NULLS ]
  OVER <window_name_or_spec>
Table 242. Arguments of NTH_VALUE
Argument Description
expr Expression. May contain a table column, constant, variable, expression,
scalar function. Aggregate functions are not allowed as an expression.
offset The offset in rows from the start (FROM FIRST), or the last (FROM LAST) to get
the value identified by expr. offset can be a column, subquery or other
expression that results in a positive integer value, or another type that
can be implicitly converted to BIGINT. offset cannot be zero or negative.
The NTH_VALUE function returns the Nth value starting from the first ( FROM FIRST) or the last ( FROM
LAST) row of the current frame, see also note on frame for navigational functions. Offset 1 with FROM
FIRST is equivalent to FIRST_VALUE, and offset 1 with FROM LAST is equivalent to LAST_VALUE.

NULL Handling:
By default, NTH_VALUE uses RESPECT NULLS behavior, which returns the Nth value in the frame even
if it is NULL. When IGNORE NULLS is specified, NTH_VALUE skips over NULL values when counting to
the Nth position, effectively returning the Nth non-NULL value in the frame. If there are fewer
than N non-NULL values in the frame, NTH_VALUE returns NULL.

Example with IGNORE NULLS:
select
  product_id,
  review_date,
  rating,
  nth_value(rating, 3) FROM FIRST RESPECT NULLS over (
    partition by product_id
    order by review_date
    rows between unbounded preceding and unbounded following
  ) as third_rating,
  nth_value(rating, 3) FROM FIRST IGNORE NULLS over (
    partition by product_id
    order by review_date
    rows between unbounded preceding and unbounded following
  ) as third_valid_rating
from product_reviews
order by product_id, review_date;

-- With IGNORE NULLS, if the first two reviews have NULL ratings, the function will
-- skip them and return the 3rd non-NULL rating instead of the literal 3rd rating.

See also
FIRST_VALUE(), LAST_VALUE()
Chapter 10. Window (Analytical) Functions
529