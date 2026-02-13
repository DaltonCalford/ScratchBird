# App H Charsets and Collations



<!-- Original PDF Page: 783 -->

PLG$SRP
User and authentication information of the Srp user manager, used for authentication by the Srp
family of authentication plugins.
Column Name Data Type Description
PLG$USER_NAME VARCHAR(63) Username
PLG$VERIFIER VARBINARY(128) SRP verifier[1]
PLG$SALT VARBINARY(32) User-specific salt
PLG$COMMENT BLOB TEXT Comment text
PLG$FIRST VARCHAR(32) Firstname
PLG$MIDDLE VARCHAR(32) Middle name
PLG$LAST VARCHAR(32) Lastname
PLG$ATTRIBUTES BLOB TEXT User attributes (a.k.a. tags)
PLG$ACTIVE BOOLEAN Active or inactive user
PLG$USERS
User and authentication information of the Legacy_UserManager user manager, used for
authentication by the Legacy_Auth authentication plugins.
Column Name Data Type Description
PLG$USER_NAME VARCHAR(63) Username
PLG$GROUP_NAME VARCHAR(63) Group name
PLG$UID INTEGER User id
PLG$GID INTEGER Group id
PLG$PASSWD VARBINARY(64) Password hash
PLG$COMMENT BLOB TEXT Comment text
PLG$FIRST_NAME VARCHAR(32) Firstname
PLG$MIDDLE_NAME VARCHAR(32) Middle name
PLG$LAST_NAME VARCHAR(32) Lastname
[1] See http://srp.stanford.edu/design.html for details
Appendix G: Plugin tables
782

<!-- Original PDF Page: 784 -->

Appendix H: Character Sets and Collations
Table 275. Character Sets and Collations
Character Set ID Bytes
per
Char
Collation Language
ASCII 2 1 ASCII English
BIG_5 56 2 BIG_5 Chinese, Vietnamese, Korean
CP943C 68 2 CP943C Japanese
  〃 〃 〃 CP943C_UNICODE Japanese
CYRL 50 1 CYRL Russian
  〃 〃 〃 DB_RUS Russian dBase
  〃 〃 〃 PDOX_CYRL Russian Paradox
DOS437 10 1 DOS437 U.S. English
  〃 〃 〃 DB_DEU437 German dBase
  〃 〃 〃 DB_ESP437 Spanish dBase
  〃 〃 〃 DB_FIN437 Finnish dBase
  〃 〃 〃 DB_FRA437 French dBase
  〃 〃 〃 DB_ITA437 Italian dBase
  〃 〃 〃 DB_NLD437 Dutch dBase
  〃 〃 〃 DB_SVE437 Swedish dBase
  〃 〃 〃 DB_UK437 English (Great Britain) dBase
  〃 〃 〃 DB_US437 U.S. English dBase
  〃 〃 〃 PDOX_ASCII Code page Paradox-ASCII
  〃 〃 〃 PDOX_INTL International English Paradox
  〃 〃 〃 PDOX_SWEDFIN Swedish / Finnish Paradox
DOS737 9 1 DOS737 Greek
DOS775 15 1 DOS775 Baltic
DOS850 11 1 DOS850 Latin I (no Euro symbol)
  〃 〃 〃 DB_DEU850 German
  〃 〃 〃 DB_ESP850 Spanish
  〃 〃 〃 DB_FRA850 French
  〃 〃 〃 DB_FRC850 French-Canada
  〃 〃 〃 DB_ITA850 Italian
  〃 〃 〃 DB_NLD850 Dutch
Appendix H: Character Sets and Collations
783

<!-- Original PDF Page: 785 -->

Character Set ID Bytes
per
Char
Collation Language
  〃 〃 〃 DB_PTB850 Portuguese-Brazil
  〃 〃 〃 DB_SVE850 Swedish
  〃 〃 〃 DB_UK850 English-Great Britain
  〃 〃 〃 DB_US850 U.S. English
DOS852 45 1 DOS852 Latin II
  〃 〃 〃 DB_CSY Czech dBase
  〃 〃 〃 DB_PLK Polish dBase
  〃 〃 〃 DB_SLO Slovene dBase
  〃 〃 〃 PDOX_CSY Czech Paradox
  〃 〃 〃 PDOX_HUN Hungarian Paradox
  〃 〃 〃 PDOX_PLK Polish Paradox
  〃 〃 〃 PDOX_SLO Slovene Paradox
DOS857 46 1 DOS857 Turkish
  〃 〃 〃 DB_TRK Turkish dBase
DOS858 16 1 DOS858 Latin I (with Euro symbol)
DOS860 13 1 DOS860 Portuguese
  〃 〃 〃 DB_PTG860 Portuguese dBase
DOS861 47 1 DOS861 Icelandic
  〃 〃 〃 PDOX_ISL Icelandic Paradox
DOS862 17 1 DOS862 Hebrew
DOS863 14 1 DOS863 French-Canada
  〃 〃 〃 DB_FRC863 French dBase-Canada
DOS864 18 1 DOS864 Arabic
DOS865 12 1 DOS865 Scandinavian
  〃 〃 〃 DB_DAN865 Danish dBase
  〃 〃 〃 DB_NOR865 Norwegian dBase
  〃 〃 〃 PDOX_NORDAN4 Paradox Norway and Denmark
DOS866 48 1 DOS866 Russian
DOS869 49 1 DOS869 Modern Greek
EUCJ_0208 6 2 EUCJ_0208 Japanese EUC
GB18030 69 4 GB18030 Chinese
  〃 〃 〃 GB18030_UNICODE Chinese
Appendix H: Character Sets and Collations
784

<!-- Original PDF Page: 786 -->

Character Set ID Bytes
per
Char
Collation Language
GBK 67 2 GBK Chinese
  〃 〃 〃 GBK_UNICODE Chinese
GB_2312 57 2 GB_2312 Simplified Chinese (Hong Kong,
Korea)
ISO8859_1 21 1 ISO8859_1 Latin I
  〃 〃 〃 DA_DA Danish
  〃 〃 〃 DE_DE German
  〃 〃 〃 DU_NL Dutch
  〃 〃 〃 EN_UK English-Great Britain
  〃 〃 〃 EN_US U.S. English
  〃 〃 〃 ES_ES Spanish
  〃 〃 〃 ES_ES_CI_AI Spanish — case insensitive and +
accent-insensitive
  〃 〃 〃 FI_FI Finnish
  〃 〃 〃 FR_CA French-Canada
  〃 〃 〃 FR_CA_CI_AI French-Canada — case insensitive
+ accent insensitive
  〃 〃 〃 FR_FR French
  〃 〃 〃 FR_FR_CI_AI French — case insensitive + accent
insensitive
  〃 〃 〃 IS_IS Icelandic
  〃 〃 〃 IT_IT Italian
  〃 〃 〃 NO_NO Norwegian
  〃 〃 〃 PT_BR Portuguese-Brazil
  〃 〃 〃 PT_PT Portuguese
  〃 〃 〃 SV_SV Swedish
ISO8859_2 22 1 ISO8859_2 Latin 2 — Central Europe
(Croatian, Czech, Hungarian,
Polish, Romanian, Serbian, Slovak,
Slovenian)
  〃 〃 〃 CS_CZ Czech
  〃 〃 〃 ISO_HUN Hungarian — case insensitive,
accent sensitive
  〃 〃 〃 ISO_PLK Polish
Appendix H: Character Sets and Collations
785

<!-- Original PDF Page: 787 -->

Character Set ID Bytes
per
Char
Collation Language
ISO8859_3 23 1 ISO8859_3 Latin 3 — Southern Europe (Malta,
Esperanto)
ISO8859_4 34 1 ISO8859_4 Latin 4 — Northern Europe
(Estonian, Latvian, Lithuanian,
Greenlandic, Lappish)
ISO8859_5 35 1 ISO8859_5 Cyrillic (Russian)
ISO8859_6 36 1 ISO8859_6 Arabic
ISO8859_7 37 1 ISO8859_7 Greek
ISO8859_8 38 1 ISO8859_8 Hebrew
ISO8859_9 39 1 ISO8859_9 Latin 5
ISO8859_13 40 1 ISO8859_13 Latin 7 — Baltic
  〃 〃 〃 LT_LT Lithuanian
KOI8R 63 1 KOI8R Russian — dictionary ordering
  〃 〃 〃 KOI8R_RU Russian
KOI8U 64 1 KOI8U Ukrainian — dictionary ordering
  〃 〃 〃 KOI8U_UA Ukrainian
KSC_5601 44 2 KSC_5601 Korean
  〃 〃 〃 KSC_DICTIONARY Korean — dictionary sort order
NEXT 19 1 NEXT Coding NeXTSTEP
  〃 〃 〃 NXT_DEU German
  〃 〃 〃 NXT_ESP Spanish
  〃 〃 〃 NXT_FRA French
  〃 〃 〃 NXT_ITA Italian
  〃 19 1 NXT_US U.S. English
NONE 0 1 NONE Neutral code page. Translation to
upper case is performed only for
code ASCII 97-122.
Recommendation: avoid this
character set
OCTETS 1 1 OCTETS Binary character encoding
SJIS_0208 5 2 SJIS_0208 Japanese
TIS620 66 1 TIS620 Thai
  〃 〃 〃 TIS620_UNICODE Thai
UNICODE_FSS 3 3 UNICODE_FSS All English
Appendix H: Character Sets and Collations
786

<!-- Original PDF Page: 788 -->

Character Set ID Bytes
per
Char
Collation Language
UTF8 4 4 UTF8 Any language that is supported in
Unicode 4.0
  〃 〃 〃 UCS_BASIC Any language that is supported in
Unicode 4.0
  〃 〃 〃 UNICODE Any language that is supported in
Unicode 4.0
  〃 〃 〃 UNICODE_CI Any language that is supported in
Unicode 4.0 — Case insensitive
  〃 〃 〃 UNICODE_CI_AI Any language that is supported in
Unicode 4.0 — Case insensitive and
accent insensitive
WIN1250 51 1 WIN1250 ANSI — Central Europe
  〃 〃 〃 BS_BA Bosnian
  〃 〃 〃 PXW_CSY Czech
  〃 〃 〃 PXW_HUN Hungarian — case insensitive,
accent sensitive
  〃 〃 〃 PXW_HUNDC Hungarian — dictionary ordering
  〃 〃 〃 PXW_PLK Polish
  〃 〃 〃 PXW_SLOV Slovenian
  〃 〃 〃 WIN_CZ Czech
  〃 〃 〃 WIN_CZ_CI_AI Czech — Case insensitive and
accent insensitive
WIN1251 52 1 WIN1251 ANSI Cyrillic
  〃 〃 〃 PXW_CYRL Paradox Cyrillic (Russian)
  〃 〃 〃 WIN1251_UA Ukrainian
WIN1252 53 1 WIN1252 ANSI — Latin I
  〃 〃 〃 PXW_INTL English International
  〃 〃 〃 PXW_INTL850 Paradox multilingual Latin I
  〃 〃 〃 PXW_NORDAN4 Norwegian and Danish
  〃 〃 〃 PXW_SPAN Paradox Spanish
  〃 〃 〃 PXW_SWEDFIN Swedish and Finnish
  〃 〃 〃 WIN_PTBR Portuguese — Brazil
WIN1253 54 1 WIN1253 ANSI Greek
  〃 〃 〃 PXW_GREEK Paradox Greek
Appendix H: Character Sets and Collations
787