# App J Document History



<!-- Original PDF Page: 790 -->

Appendix I: License notice
The contents of this Documentation are subject to the Public Documentation License Version 1.0
(the “License”); you may only use this Documentation if you comply with the terms of this License.
Copies of the License are available at https://www.firebirdsql.org/pdfmanual/pdl.pdf (PDF) and
https://www.firebirdsql.org/manual/pdl.html (HTML).
The Original Documentation is titled Firebird 5.0 Language Reference . This Documentation was
derived from Firebird 4.0 Language Reference.
The Initial Writers of the Original Documentation are: Paul Vinkenoog, Dmitry Yemanov, Thomas
Woinke and Mark Rotteveel. Writers of text originally in Russian are Denis Simonov, Dmitry
Filippov, Alexander Karpeykin, Alexey Kovyazin and Dmitry Kuzmenko.
Copyright © 2008-2023. All Rights Reserved. Initial Writers contact: paul at vinkenoog dot nl.
Writers and Editors of included PDL-licensed material are: J. Beesley, Helen Borrie, Arno Brinkman,
Frank Ingermann, Vlad Khorsun, Alex Peshkov, Nickolay Samofatov, Adriano dos Santos Fernandes,
Dmitry Yemanov.
Included portions are Copyright © 2001-2023 by their respective authors. All Rights Reserved.
Contributor(s): Mark Rotteveel.
Portions created by Mark Rotteveel are Copyright © 2018-2023. All Rights Reserved. (Contributor
contact(s): mrotteveel at users dot sourceforge dot net).
Appendix I: License notice
789

<!-- Original PDF Page: 791 -->

Appendix J: Document History
The exact file history is recorded in our git repository; see https://github.com/FirebirdSQL/firebird-
documentation
Revision History
0.5 29 Sep
2023
M
R
• Removed unnecessary references to older Firebird versions
• Misc. copy-editing
• CHAR_LENGTH, OCTET_LENGTH and BIT_LENGTH use BIGINT for BLOB
• Fixed incorrect "equivalent" for REGR_COUNT
• Fixed incorrect references to idle timeout in SET STATEMENT TIMEOUT
• Documented that OVERRIDING USER VALUE  also works for GENERATED ALWAYS
identity columns
• Document QUARTER for EXTRACT, FIRST_DAY, and LAST_DAY
• Document DECFLOAT_ROUND and DECFLOAT_TRAPS for RDB$GET_CONTEXT
• Document LEVEL in PLG$PROF_RECORD_SOURCES and
PLG$PROF_RECORD_SOURCE_STATS_VIEW, order of columns for profiler tables
• Document new limit for IN-list
• Document OPTIMIZE FOR {FIRST | ALL} ROWS on SELECT and SET OPTIMIZE
• Added negative subtype to RDB$FIELDS.RDB$FIELD_SUB_TYPE, and fixed
formatting
0.4 20 Jun
2023
M
R
• Computed columns can be indexed with expression indexes
• Fixed wrong section levels for subsections of SET DECFLOAT
• Replaced firebird-docs references with firebird-devel
• Updated SQLCODE and GDSCODE Error Codes and Message Texts  with
error information from 5.0.0.1068
• Add caution about relying on ordered derived tables for LIST()
Appendix J: Document History
790

<!-- Original PDF Page: 792 -->

Revision History
0.3 26 May
2023
M
R
• Added missing context variable names for RDB$GET_CONTEXT()
• Documented hex-literal support for INT128
• CURRENT_CONNECTION returns BIGINT
• PLG$PROF_RECORD_SOURCES.ACCESS_PATH changed to VARCHAR(255)
• Example for RDB$ROLE_IN_USE() should use RDB$ROLES (#184)
• Changed explanation of maximum blob size (#160)
• Notes on RETURNING and updatable views (#95)
• Replaced occurrence of “collation sequence” with “collation”
• Removed section Joins with stored procedures as it no longer applies
• Replaced mention that implicit join is deprecated and might get removed;
its use is merely discouraged.
• Removed “Available in” sections if they listed both DSQL and PSQL
• Replaced “Used for” paragraphs with a plain paragraph (so, without
explicit “Used for” title)
• Rewrote function descriptions to include a short description at the top of
each function section
• Added note in Encryption Algorithm Requirements about AES variants
• Replaced incorrect ROLE keyword with DEFAULT in example in Granting the
RDB$ADMIN Role in a Regular Database
• Miscellaneous copy-editing
Appendix J: Document History
791

<!-- Original PDF Page: 793 -->

Revision History
0.2 10 May
2023
M
R
• Documented “standard” plugin tables in new appendix Plugin tables
• Removed Upgraders: PLEASE READ! sidebar from Built-in Scalar Functions,
the Possible name conflict  sections from function descriptions and the
Name Clash note on LOWER()
• Integrated (most) changes from the Firebird 5.0 beta 1 release notes
• Added new chapter System Packages , and moved RDB$TIME_ZONE_UTIL
documentation to it, and added RDB$BLOB_UTIL and RDB$PROFILER
documentation
• Documented that subroutines can access variables of their parent
• Simplified CONTINUE and LEAVE examples, by removing unnecessary ELSE
• Documented PLAN, ORDER BY and ROWS for UPDATE OR INSERT  and PLAN and
ORDER BY for MERGE
• Added Full SELECT Syntax  as a first step to address current
incomplete/simplified syntax used in SELECT
• Removed incorrect <common-table-expression> production in SELECT
syntax
• Revised syntax used in SELECT and Window (Analytical) Functions  for
completeness and correctness
• Document <parenthesized-joined-table> in SELECT
0.1 05 May
2023
M
R
Copied the Firebird 4.0 Language Reference as a starting point:
• renamed files and reference using fblangref40 to fblangref50
• where applicable, replaced reference to Firebird 4.0 with Firebird 5.0, or
rephrased sentences referencing Firebird 4.0
Appendix J: Document History
792