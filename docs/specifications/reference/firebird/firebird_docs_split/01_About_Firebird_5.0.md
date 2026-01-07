# 01 About Firebird 5.0



<!-- Original PDF Page: 18 -->

SEC$DB_CREATORS . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  772
SEC$GLOBAL_AUTH_MAPPING. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  772
SEC$USERS . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  773
SEC$USER_ATTRIBUTES . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  774
Appendix G: Plugin tables . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  775
PLG$PROF_CURSORS . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  776
PLG$PROF_PSQL_STATS . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  776
PLG$PROF_PSQL_STATS_VIEW. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  777
PLG$PROF_RECORD_SOURCES. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  777
PLG$PROF_RECORD_SOURCE_STATS. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  778
PLG$PROF_RECORD_SOURCE_STATS_VIEW. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  778
PLG$PROF_REQUESTS . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  780
PLG$PROF_SESSIONS . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  780
PLG$PROF_STATEMENTS . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  780
PLG$PROF_STATEMENT_STATS_VIEW. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  781
PLG$SRP . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  782
PLG$USERS . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  782
Appendix H: Character Sets and Collations . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  783
Appendix I: License notice. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  789
Appendix J: Document History . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .  790
Table of Contents
17

<!-- Original PDF Page: 19 -->

Chapter 1. About the Firebird 5.0 Language
Reference
This Language Reference describes the SQL language supported by Firebird 5.0.
This Firebird 5.0 Language Reference is the fourth comprehensive manual to cover all aspects of
the query language used by developers to communicate, through their applications, with the
Firebird relational database management system.
1.1. Subject
The subject of this volume is Firebird’s implementation of the SQL (“Structured Query Language”)
relational database language. Firebird conforms closely with international standards for SQL, from
data type support, data storage structures, referential integrity mechanisms, to data manipulation
capabilities and access privileges. Firebird also implements a robust procedural
language — procedural SQL (PSQL) — for stored procedures, stored functions, triggers, and
dynamically-executable code blocks. These areas are addressed in this volume.
This document does not cover configuration of Firebird, Firebird command-line tools, nor its
programming APIs. See Firebird RDBMS , and specifically Reference Manuals  for more Firebird
documentation.
1.2. Authorship
For the Firebird 5.0 version, the Firebird 4.0 Language Reference was taken as the base, and Firebird
5.0 information was added based on the Firebird 5.0 Release Notes and feature documentation.
1.2.1. Contributors
Direct Content
• Dmitry Filippov (writer)
• Alexander Karpeykin (writer)
• Alexey Kovyazin (writer, editor)
• Dmitry Kuzmenko (writer, editor)
• Denis Simonov (writer, editor)
• Paul Vinkenoog (writer, designer)
• Dmitry Yemanov (writer)
• Mark Rotteveel (writer, editor)
Resource Content
• Adriano dos Santos Fernandes
Chapter 1. About the Firebird 5.0 Language Reference
18