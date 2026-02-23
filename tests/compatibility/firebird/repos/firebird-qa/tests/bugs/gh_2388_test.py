#coding:utf-8

"""
ID:          issue-2388
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/2388
TITLE:       Implement SQL standard FORMAT clause for CAST between string types and datetime types [CORE6507]
DESCRIPTION:
NOTES:
    [05.04.2024] pzotov
    Test generates SQL expressions to be used as execute statement argument.
    Main idea: choose some random timestamp, then convert it to string using some FORMAT and then convert back to timestamp.
    Origin and resulting values must be equal.
    More detailed description will be made later.

    Checked on 6.0.0.305 #73551f3
    ::: NB ::: execution time about 5-6 minutes!
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    recreate global temporary table tmp(tm_tz_txt varchar(100)) on commit preserve rows;

    recreate table fmt_delimiter(
        d char(1) primary key
    );
    insert into fmt_delimiter(d)
    select '.' from rdb$database union all
    select '/' from rdb$database union all
    select ',' from rdb$database union all
    select ';' from rdb$database union all
    select ':' from rdb$database union all
    select '-' from rdb$database union all
    select ' ' from rdb$database
    ;
    commit;

    recreate table tz_list (tz_name varchar(50));
    insert into tz_list (tz_name) values ('Africa/Abidjan');
    insert into tz_list (tz_name) values ('Africa/Accra');
    insert into tz_list (tz_name) values ('Africa/Addis_Ababa');
    insert into tz_list (tz_name) values ('Africa/Algiers');
    insert into tz_list (tz_name) values ('Africa/Asmara');
    insert into tz_list (tz_name) values ('Africa/Asmera');
    insert into tz_list (tz_name) values ('Africa/Bamako');
    insert into tz_list (tz_name) values ('Africa/Bangui');
    insert into tz_list (tz_name) values ('Africa/Banjul');
    insert into tz_list (tz_name) values ('Africa/Bissau');
    insert into tz_list (tz_name) values ('Africa/Blantyre');
    insert into tz_list (tz_name) values ('Africa/Brazzaville');
    insert into tz_list (tz_name) values ('Africa/Bujumbura');
    insert into tz_list (tz_name) values ('Africa/Cairo');
    insert into tz_list (tz_name) values ('Africa/Casablanca');
    insert into tz_list (tz_name) values ('Africa/Ceuta');
    insert into tz_list (tz_name) values ('Africa/Conakry');
    insert into tz_list (tz_name) values ('Africa/Dakar');
    insert into tz_list (tz_name) values ('Africa/Dar_es_Salaam');
    insert into tz_list (tz_name) values ('Africa/Djibouti');
    insert into tz_list (tz_name) values ('Africa/Douala');
    insert into tz_list (tz_name) values ('Africa/El_Aaiun');
    insert into tz_list (tz_name) values ('Africa/Freetown');
    insert into tz_list (tz_name) values ('Africa/Gaborone');
    insert into tz_list (tz_name) values ('Africa/Harare');
    insert into tz_list (tz_name) values ('Africa/Johannesburg');
    insert into tz_list (tz_name) values ('Africa/Juba');
    insert into tz_list (tz_name) values ('Africa/Kampala');
    insert into tz_list (tz_name) values ('Africa/Khartoum');
    insert into tz_list (tz_name) values ('Africa/Kigali');
    insert into tz_list (tz_name) values ('Africa/Kinshasa');
    insert into tz_list (tz_name) values ('Africa/Lagos');
    insert into tz_list (tz_name) values ('Africa/Libreville');
    insert into tz_list (tz_name) values ('Africa/Lome');
    insert into tz_list (tz_name) values ('Africa/Luanda');
    insert into tz_list (tz_name) values ('Africa/Lubumbashi');
    insert into tz_list (tz_name) values ('Africa/Lusaka');
    insert into tz_list (tz_name) values ('Africa/Malabo');
    insert into tz_list (tz_name) values ('Africa/Maputo');
    insert into tz_list (tz_name) values ('Africa/Maseru');
    insert into tz_list (tz_name) values ('Africa/Mbabane');
    insert into tz_list (tz_name) values ('Africa/Mogadishu');
    insert into tz_list (tz_name) values ('Africa/Monrovia');
    insert into tz_list (tz_name) values ('Africa/Nairobi');
    insert into tz_list (tz_name) values ('Africa/Ndjamena');
    insert into tz_list (tz_name) values ('Africa/Niamey');
    insert into tz_list (tz_name) values ('Africa/Nouakchott');
    insert into tz_list (tz_name) values ('Africa/Ouagadougou');
    insert into tz_list (tz_name) values ('Africa/Porto-Novo');
    insert into tz_list (tz_name) values ('Africa/Sao_Tome');
    insert into tz_list (tz_name) values ('Africa/Timbuktu');
    insert into tz_list (tz_name) values ('Africa/Tripoli');
    insert into tz_list (tz_name) values ('Africa/Tunis');
    insert into tz_list (tz_name) values ('Africa/Windhoek');
    insert into tz_list (tz_name) values ('America/Adak');
    insert into tz_list (tz_name) values ('America/Anchorage');
    insert into tz_list (tz_name) values ('America/Anguilla');
    insert into tz_list (tz_name) values ('America/Antigua');
    insert into tz_list (tz_name) values ('America/Araguaina');
    insert into tz_list (tz_name) values ('America/Argentina/Buenos_Aires');
    insert into tz_list (tz_name) values ('America/Argentina/Catamarca');
    insert into tz_list (tz_name) values ('America/Argentina/ComodRivadavia');
    insert into tz_list (tz_name) values ('America/Argentina/Cordoba');
    insert into tz_list (tz_name) values ('America/Argentina/Jujuy');
    insert into tz_list (tz_name) values ('America/Argentina/La_Rioja');
    insert into tz_list (tz_name) values ('America/Argentina/Mendoza');
    insert into tz_list (tz_name) values ('America/Argentina/Rio_Gallegos');
    insert into tz_list (tz_name) values ('America/Argentina/Salta');
    insert into tz_list (tz_name) values ('America/Argentina/San_Juan');
    insert into tz_list (tz_name) values ('America/Argentina/San_Luis');
    insert into tz_list (tz_name) values ('America/Argentina/Tucuman');
    insert into tz_list (tz_name) values ('America/Argentina/Ushuaia');
    insert into tz_list (tz_name) values ('America/Aruba');
    insert into tz_list (tz_name) values ('America/Asuncion');
    insert into tz_list (tz_name) values ('America/Atikokan');
    insert into tz_list (tz_name) values ('America/Atka');
    insert into tz_list (tz_name) values ('America/Bahia');
    insert into tz_list (tz_name) values ('America/Bahia_Banderas');
    insert into tz_list (tz_name) values ('America/Barbados');
    insert into tz_list (tz_name) values ('America/Belem');
    insert into tz_list (tz_name) values ('America/Belize');
    insert into tz_list (tz_name) values ('America/Blanc-Sablon');
    insert into tz_list (tz_name) values ('America/Boa_Vista');
    insert into tz_list (tz_name) values ('America/Bogota');
    insert into tz_list (tz_name) values ('America/Boise');
    insert into tz_list (tz_name) values ('America/Buenos_Aires');
    insert into tz_list (tz_name) values ('America/Cambridge_Bay');
    insert into tz_list (tz_name) values ('America/Campo_Grande');
    insert into tz_list (tz_name) values ('America/Cancun');
    insert into tz_list (tz_name) values ('America/Caracas');
    insert into tz_list (tz_name) values ('America/Catamarca');
    insert into tz_list (tz_name) values ('America/Cayenne');
    insert into tz_list (tz_name) values ('America/Cayman');
    insert into tz_list (tz_name) values ('America/Chicago');
    insert into tz_list (tz_name) values ('America/Chihuahua');
    insert into tz_list (tz_name) values ('America/Ciudad_Juarez');
    insert into tz_list (tz_name) values ('America/Coral_Harbour');
    insert into tz_list (tz_name) values ('America/Cordoba');
    insert into tz_list (tz_name) values ('America/Costa_Rica');
    insert into tz_list (tz_name) values ('America/Creston');
    insert into tz_list (tz_name) values ('America/Cuiaba');
    insert into tz_list (tz_name) values ('America/Curacao');
    insert into tz_list (tz_name) values ('America/Danmarkshavn');
    insert into tz_list (tz_name) values ('America/Dawson');
    insert into tz_list (tz_name) values ('America/Dawson_Creek');
    insert into tz_list (tz_name) values ('America/Denver');
    insert into tz_list (tz_name) values ('America/Detroit');
    insert into tz_list (tz_name) values ('America/Dominica');
    insert into tz_list (tz_name) values ('America/Edmonton');
    insert into tz_list (tz_name) values ('America/Eirunepe');
    insert into tz_list (tz_name) values ('America/El_Salvador');
    insert into tz_list (tz_name) values ('America/Ensenada');
    insert into tz_list (tz_name) values ('America/Fort_Nelson');
    insert into tz_list (tz_name) values ('America/Fort_Wayne');
    insert into tz_list (tz_name) values ('America/Fortaleza');
    insert into tz_list (tz_name) values ('America/Glace_Bay');
    insert into tz_list (tz_name) values ('America/Godthab');
    insert into tz_list (tz_name) values ('America/Goose_Bay');
    insert into tz_list (tz_name) values ('America/Grand_Turk');
    insert into tz_list (tz_name) values ('America/Grenada');
    insert into tz_list (tz_name) values ('America/Guadeloupe');
    insert into tz_list (tz_name) values ('America/Guatemala');
    insert into tz_list (tz_name) values ('America/Guayaquil');
    insert into tz_list (tz_name) values ('America/Guyana');
    insert into tz_list (tz_name) values ('America/Halifax');
    insert into tz_list (tz_name) values ('America/Havana');
    insert into tz_list (tz_name) values ('America/Hermosillo');
    insert into tz_list (tz_name) values ('America/Indiana/Indianapolis');
    insert into tz_list (tz_name) values ('America/Indiana/Knox');
    insert into tz_list (tz_name) values ('America/Indiana/Marengo');
    insert into tz_list (tz_name) values ('America/Indiana/Petersburg');
    insert into tz_list (tz_name) values ('America/Indiana/Tell_City');
    insert into tz_list (tz_name) values ('America/Indiana/Vevay');
    insert into tz_list (tz_name) values ('America/Indiana/Vincennes');
    insert into tz_list (tz_name) values ('America/Indiana/Winamac');
    insert into tz_list (tz_name) values ('America/Indianapolis');
    insert into tz_list (tz_name) values ('America/Inuvik');
    insert into tz_list (tz_name) values ('America/Iqaluit');
    insert into tz_list (tz_name) values ('America/Jamaica');
    insert into tz_list (tz_name) values ('America/Jujuy');
    insert into tz_list (tz_name) values ('America/Juneau');
    insert into tz_list (tz_name) values ('America/Kentucky/Louisville');
    insert into tz_list (tz_name) values ('America/Kentucky/Monticello');
    insert into tz_list (tz_name) values ('America/Knox_IN');
    insert into tz_list (tz_name) values ('America/Kralendijk');
    insert into tz_list (tz_name) values ('America/La_Paz');
    insert into tz_list (tz_name) values ('America/Lima');
    insert into tz_list (tz_name) values ('America/Los_Angeles');
    insert into tz_list (tz_name) values ('America/Louisville');
    insert into tz_list (tz_name) values ('America/Lower_Princes');
    insert into tz_list (tz_name) values ('America/Maceio');
    insert into tz_list (tz_name) values ('America/Managua');
    insert into tz_list (tz_name) values ('America/Manaus');
    insert into tz_list (tz_name) values ('America/Marigot');
    insert into tz_list (tz_name) values ('America/Martinique');
    insert into tz_list (tz_name) values ('America/Matamoros');
    insert into tz_list (tz_name) values ('America/Mazatlan');
    insert into tz_list (tz_name) values ('America/Mendoza');
    insert into tz_list (tz_name) values ('America/Menominee');
    insert into tz_list (tz_name) values ('America/Merida');
    insert into tz_list (tz_name) values ('America/Metlakatla');
    insert into tz_list (tz_name) values ('America/Mexico_City');
    insert into tz_list (tz_name) values ('America/Miquelon');
    insert into tz_list (tz_name) values ('America/Moncton');
    insert into tz_list (tz_name) values ('America/Monterrey');
    insert into tz_list (tz_name) values ('America/Montevideo');
    insert into tz_list (tz_name) values ('America/Montreal');
    insert into tz_list (tz_name) values ('America/Montserrat');
    insert into tz_list (tz_name) values ('America/Nassau');
    insert into tz_list (tz_name) values ('America/New_York');
    insert into tz_list (tz_name) values ('America/Nipigon');
    insert into tz_list (tz_name) values ('America/Nome');
    insert into tz_list (tz_name) values ('America/Noronha');
    insert into tz_list (tz_name) values ('America/North_Dakota/Beulah');
    insert into tz_list (tz_name) values ('America/North_Dakota/Center');
    insert into tz_list (tz_name) values ('America/North_Dakota/New_Salem');
    insert into tz_list (tz_name) values ('America/Nuuk');
    insert into tz_list (tz_name) values ('America/Ojinaga');
    insert into tz_list (tz_name) values ('America/Panama');
    insert into tz_list (tz_name) values ('America/Pangnirtung');
    insert into tz_list (tz_name) values ('America/Paramaribo');
    insert into tz_list (tz_name) values ('America/Phoenix');
    insert into tz_list (tz_name) values ('America/Port-au-Prince');
    insert into tz_list (tz_name) values ('America/Port_of_Spain');
    insert into tz_list (tz_name) values ('America/Porto_Acre');
    insert into tz_list (tz_name) values ('America/Porto_Velho');
    insert into tz_list (tz_name) values ('America/Puerto_Rico');
    insert into tz_list (tz_name) values ('America/Punta_Arenas');
    insert into tz_list (tz_name) values ('America/Rainy_River');
    insert into tz_list (tz_name) values ('America/Rankin_Inlet');
    insert into tz_list (tz_name) values ('America/Recife');
    insert into tz_list (tz_name) values ('America/Regina');
    insert into tz_list (tz_name) values ('America/Resolute');
    insert into tz_list (tz_name) values ('America/Rio_Branco');
    insert into tz_list (tz_name) values ('America/Rosario');
    insert into tz_list (tz_name) values ('America/Santa_Isabel');
    insert into tz_list (tz_name) values ('America/Santarem');
    insert into tz_list (tz_name) values ('America/Santiago');
    insert into tz_list (tz_name) values ('America/Santo_Domingo');
    insert into tz_list (tz_name) values ('America/Sao_Paulo');
    insert into tz_list (tz_name) values ('America/Scoresbysund');
    insert into tz_list (tz_name) values ('America/Shiprock');
    insert into tz_list (tz_name) values ('America/Sitka');
    insert into tz_list (tz_name) values ('America/St_Barthelemy');
    insert into tz_list (tz_name) values ('America/St_Johns');
    insert into tz_list (tz_name) values ('America/St_Kitts');
    insert into tz_list (tz_name) values ('America/St_Lucia');
    insert into tz_list (tz_name) values ('America/St_Thomas');
    insert into tz_list (tz_name) values ('America/St_Vincent');
    insert into tz_list (tz_name) values ('America/Swift_Current');
    insert into tz_list (tz_name) values ('America/Tegucigalpa');
    insert into tz_list (tz_name) values ('America/Thule');
    insert into tz_list (tz_name) values ('America/Thunder_Bay');
    insert into tz_list (tz_name) values ('America/Tijuana');
    insert into tz_list (tz_name) values ('America/Toronto');
    insert into tz_list (tz_name) values ('America/Tortola');
    insert into tz_list (tz_name) values ('America/Vancouver');
    insert into tz_list (tz_name) values ('America/Virgin');
    insert into tz_list (tz_name) values ('America/Whitehorse');
    insert into tz_list (tz_name) values ('America/Winnipeg');
    insert into tz_list (tz_name) values ('America/Yakutat');
    insert into tz_list (tz_name) values ('America/Yellowknife');
    insert into tz_list (tz_name) values ('Antarctica/Casey');
    insert into tz_list (tz_name) values ('Antarctica/Davis');
    insert into tz_list (tz_name) values ('Antarctica/DumontDUrville');
    insert into tz_list (tz_name) values ('Antarctica/Macquarie');
    insert into tz_list (tz_name) values ('Antarctica/Mawson');
    insert into tz_list (tz_name) values ('Antarctica/McMurdo');
    insert into tz_list (tz_name) values ('Antarctica/Palmer');
    insert into tz_list (tz_name) values ('Antarctica/Rothera');
    insert into tz_list (tz_name) values ('Antarctica/South_Pole');
    insert into tz_list (tz_name) values ('Antarctica/Syowa');
    insert into tz_list (tz_name) values ('Antarctica/Troll');
    insert into tz_list (tz_name) values ('Antarctica/Vostok');
    insert into tz_list (tz_name) values ('Arctic/Longyearbyen');
    insert into tz_list (tz_name) values ('Asia/Aden');
    insert into tz_list (tz_name) values ('Asia/Almaty');
    insert into tz_list (tz_name) values ('Asia/Amman');
    insert into tz_list (tz_name) values ('Asia/Anadyr');
    insert into tz_list (tz_name) values ('Asia/Aqtau');
    insert into tz_list (tz_name) values ('Asia/Aqtobe');
    insert into tz_list (tz_name) values ('Asia/Ashgabat');
    insert into tz_list (tz_name) values ('Asia/Ashkhabad');
    insert into tz_list (tz_name) values ('Asia/Atyrau');
    insert into tz_list (tz_name) values ('Asia/Baghdad');
    insert into tz_list (tz_name) values ('Asia/Bahrain');
    insert into tz_list (tz_name) values ('Asia/Baku');
    insert into tz_list (tz_name) values ('Asia/Bangkok');
    insert into tz_list (tz_name) values ('Asia/Barnaul');
    insert into tz_list (tz_name) values ('Asia/Beirut');
    insert into tz_list (tz_name) values ('Asia/Bishkek');
    insert into tz_list (tz_name) values ('Asia/Brunei');
    insert into tz_list (tz_name) values ('Asia/Calcutta');
    insert into tz_list (tz_name) values ('Asia/Chita');
    insert into tz_list (tz_name) values ('Asia/Choibalsan');
    insert into tz_list (tz_name) values ('Asia/Chongqing');
    insert into tz_list (tz_name) values ('Asia/Chungking');
    insert into tz_list (tz_name) values ('Asia/Colombo');
    insert into tz_list (tz_name) values ('Asia/Dacca');
    insert into tz_list (tz_name) values ('Asia/Damascus');
    insert into tz_list (tz_name) values ('Asia/Dhaka');
    insert into tz_list (tz_name) values ('Asia/Dili');
    insert into tz_list (tz_name) values ('Asia/Dubai');
    insert into tz_list (tz_name) values ('Asia/Dushanbe');
    insert into tz_list (tz_name) values ('Asia/Famagusta');
    insert into tz_list (tz_name) values ('Asia/Gaza');
    insert into tz_list (tz_name) values ('Asia/Harbin');
    insert into tz_list (tz_name) values ('Asia/Hebron');
    insert into tz_list (tz_name) values ('Asia/Ho_Chi_Minh');
    insert into tz_list (tz_name) values ('Asia/Hong_Kong');
    insert into tz_list (tz_name) values ('Asia/Hovd');
    insert into tz_list (tz_name) values ('Asia/Irkutsk');
    insert into tz_list (tz_name) values ('Asia/Istanbul');
    insert into tz_list (tz_name) values ('Asia/Jakarta');
    insert into tz_list (tz_name) values ('Asia/Jayapura');
    insert into tz_list (tz_name) values ('Asia/Jerusalem');
    insert into tz_list (tz_name) values ('Asia/Kabul');
    insert into tz_list (tz_name) values ('Asia/Kamchatka');
    insert into tz_list (tz_name) values ('Asia/Karachi');
    insert into tz_list (tz_name) values ('Asia/Kashgar');
    insert into tz_list (tz_name) values ('Asia/Kathmandu');
    insert into tz_list (tz_name) values ('Asia/Katmandu');
    insert into tz_list (tz_name) values ('Asia/Khandyga');
    insert into tz_list (tz_name) values ('Asia/Kolkata');
    insert into tz_list (tz_name) values ('Asia/Krasnoyarsk');
    insert into tz_list (tz_name) values ('Asia/Kuala_Lumpur');
    insert into tz_list (tz_name) values ('Asia/Kuching');
    insert into tz_list (tz_name) values ('Asia/Kuwait');
    insert into tz_list (tz_name) values ('Asia/Macao');
    insert into tz_list (tz_name) values ('Asia/Macau');
    insert into tz_list (tz_name) values ('Asia/Magadan');
    insert into tz_list (tz_name) values ('Asia/Makassar');
    insert into tz_list (tz_name) values ('Asia/Manila');
    insert into tz_list (tz_name) values ('Asia/Muscat');
    insert into tz_list (tz_name) values ('Asia/Nicosia');
    insert into tz_list (tz_name) values ('Asia/Novokuznetsk');
    insert into tz_list (tz_name) values ('Asia/Novosibirsk');
    insert into tz_list (tz_name) values ('Asia/Omsk');
    insert into tz_list (tz_name) values ('Asia/Oral');
    insert into tz_list (tz_name) values ('Asia/Phnom_Penh');
    insert into tz_list (tz_name) values ('Asia/Pontianak');
    insert into tz_list (tz_name) values ('Asia/Pyongyang');
    insert into tz_list (tz_name) values ('Asia/Qatar');
    insert into tz_list (tz_name) values ('Asia/Qostanay');
    insert into tz_list (tz_name) values ('Asia/Qyzylorda');
    insert into tz_list (tz_name) values ('Asia/Rangoon');
    insert into tz_list (tz_name) values ('Asia/Riyadh');
    insert into tz_list (tz_name) values ('Asia/Saigon');
    insert into tz_list (tz_name) values ('Asia/Sakhalin');
    insert into tz_list (tz_name) values ('Asia/Samarkand');
    insert into tz_list (tz_name) values ('Asia/Seoul');
    insert into tz_list (tz_name) values ('Asia/Shanghai');
    insert into tz_list (tz_name) values ('Asia/Singapore');
    insert into tz_list (tz_name) values ('Asia/Srednekolymsk');
    insert into tz_list (tz_name) values ('Asia/Taipei');
    insert into tz_list (tz_name) values ('Asia/Tashkent');
    insert into tz_list (tz_name) values ('Asia/Tbilisi');
    insert into tz_list (tz_name) values ('Asia/Tehran');
    insert into tz_list (tz_name) values ('Asia/Tel_Aviv');
    insert into tz_list (tz_name) values ('Asia/Thimbu');
    insert into tz_list (tz_name) values ('Asia/Thimphu');
    insert into tz_list (tz_name) values ('Asia/Tokyo');
    insert into tz_list (tz_name) values ('Asia/Tomsk');
    insert into tz_list (tz_name) values ('Asia/Ujung_Pandang');
    insert into tz_list (tz_name) values ('Asia/Ulaanbaatar');
    insert into tz_list (tz_name) values ('Asia/Ulan_Bator');
    insert into tz_list (tz_name) values ('Asia/Urumqi');
    insert into tz_list (tz_name) values ('Asia/Ust-Nera');
    insert into tz_list (tz_name) values ('Asia/Vientiane');
    insert into tz_list (tz_name) values ('Asia/Vladivostok');
    insert into tz_list (tz_name) values ('Asia/Yakutsk');
    insert into tz_list (tz_name) values ('Asia/Yangon');
    insert into tz_list (tz_name) values ('Asia/Yekaterinburg');
    insert into tz_list (tz_name) values ('Asia/Yerevan');
    insert into tz_list (tz_name) values ('Atlantic/Azores');
    insert into tz_list (tz_name) values ('Atlantic/Bermuda');
    insert into tz_list (tz_name) values ('Atlantic/Canary');
    insert into tz_list (tz_name) values ('Atlantic/Cape_Verde');
    insert into tz_list (tz_name) values ('Atlantic/Faeroe');
    insert into tz_list (tz_name) values ('Atlantic/Faroe');
    insert into tz_list (tz_name) values ('Atlantic/Jan_Mayen');
    insert into tz_list (tz_name) values ('Atlantic/Madeira');
    insert into tz_list (tz_name) values ('Atlantic/Reykjavik');
    insert into tz_list (tz_name) values ('Atlantic/South_Georgia');
    insert into tz_list (tz_name) values ('Atlantic/St_Helena');
    insert into tz_list (tz_name) values ('Atlantic/Stanley');
    insert into tz_list (tz_name) values ('Australia/ACT');
    insert into tz_list (tz_name) values ('Australia/Adelaide');
    insert into tz_list (tz_name) values ('Australia/Brisbane');
    insert into tz_list (tz_name) values ('Australia/Broken_Hill');
    insert into tz_list (tz_name) values ('Australia/Canberra');
    insert into tz_list (tz_name) values ('Australia/Currie');
    insert into tz_list (tz_name) values ('Australia/Darwin');
    insert into tz_list (tz_name) values ('Australia/Eucla');
    insert into tz_list (tz_name) values ('Australia/Hobart');
    insert into tz_list (tz_name) values ('Australia/LHI');
    insert into tz_list (tz_name) values ('Australia/Lindeman');
    insert into tz_list (tz_name) values ('Australia/Lord_Howe');
    insert into tz_list (tz_name) values ('Australia/Melbourne');
    insert into tz_list (tz_name) values ('Australia/NSW');
    insert into tz_list (tz_name) values ('Australia/North');
    insert into tz_list (tz_name) values ('Australia/Perth');
    insert into tz_list (tz_name) values ('Australia/Queensland');
    insert into tz_list (tz_name) values ('Australia/South');
    insert into tz_list (tz_name) values ('Australia/Sydney');
    insert into tz_list (tz_name) values ('Australia/Tasmania');
    insert into tz_list (tz_name) values ('Australia/Victoria');
    insert into tz_list (tz_name) values ('Australia/West');
    insert into tz_list (tz_name) values ('Australia/Yancowinna');
    insert into tz_list (tz_name) values ('Brazil/Acre');
    insert into tz_list (tz_name) values ('Brazil/DeNoronha');
    insert into tz_list (tz_name) values ('Brazil/East');
    insert into tz_list (tz_name) values ('Brazil/West');
    insert into tz_list (tz_name) values ('CET');
    insert into tz_list (tz_name) values ('CST6CDT');
    insert into tz_list (tz_name) values ('Canada/Atlantic');
    insert into tz_list (tz_name) values ('Canada/Central');
    insert into tz_list (tz_name) values ('Canada/Eastern');
    insert into tz_list (tz_name) values ('Canada/Mountain');
    insert into tz_list (tz_name) values ('Canada/Newfoundland');
    insert into tz_list (tz_name) values ('Canada/Pacific');
    insert into tz_list (tz_name) values ('Canada/Saskatchewan');
    insert into tz_list (tz_name) values ('Canada/Yukon');
    insert into tz_list (tz_name) values ('Chile/Continental');
    insert into tz_list (tz_name) values ('Chile/EasterIsland');
    insert into tz_list (tz_name) values ('Cuba');
    insert into tz_list (tz_name) values ('EET');
    insert into tz_list (tz_name) values ('EST');
    insert into tz_list (tz_name) values ('EST5EDT');
    insert into tz_list (tz_name) values ('Egypt');
    insert into tz_list (tz_name) values ('Eire');
    insert into tz_list (tz_name) values ('Etc/GMT');
    insert into tz_list (tz_name) values ('Etc/GMT+0');
    insert into tz_list (tz_name) values ('Etc/GMT+1');
    insert into tz_list (tz_name) values ('Etc/GMT+10');
    insert into tz_list (tz_name) values ('Etc/GMT+11');
    insert into tz_list (tz_name) values ('Etc/GMT+12');
    insert into tz_list (tz_name) values ('Etc/GMT+2');
    insert into tz_list (tz_name) values ('Etc/GMT+3');
    insert into tz_list (tz_name) values ('Etc/GMT+4');
    insert into tz_list (tz_name) values ('Etc/GMT+5');
    insert into tz_list (tz_name) values ('Etc/GMT+6');
    insert into tz_list (tz_name) values ('Etc/GMT+7');
    insert into tz_list (tz_name) values ('Etc/GMT+8');
    insert into tz_list (tz_name) values ('Etc/GMT+9');
    insert into tz_list (tz_name) values ('Etc/GMT-0');
    insert into tz_list (tz_name) values ('Etc/GMT-1');
    insert into tz_list (tz_name) values ('Etc/GMT-10');
    insert into tz_list (tz_name) values ('Etc/GMT-11');
    insert into tz_list (tz_name) values ('Etc/GMT-12');
    insert into tz_list (tz_name) values ('Etc/GMT-13');
    insert into tz_list (tz_name) values ('Etc/GMT-14');
    insert into tz_list (tz_name) values ('Etc/GMT-2');
    insert into tz_list (tz_name) values ('Etc/GMT-3');
    insert into tz_list (tz_name) values ('Etc/GMT-4');
    insert into tz_list (tz_name) values ('Etc/GMT-5');
    insert into tz_list (tz_name) values ('Etc/GMT-6');
    insert into tz_list (tz_name) values ('Etc/GMT-7');
    insert into tz_list (tz_name) values ('Etc/GMT-8');
    insert into tz_list (tz_name) values ('Etc/GMT-9');
    insert into tz_list (tz_name) values ('Etc/GMT0');
    insert into tz_list (tz_name) values ('Etc/Greenwich');
    insert into tz_list (tz_name) values ('Etc/UCT');
    insert into tz_list (tz_name) values ('Etc/UTC');
    insert into tz_list (tz_name) values ('Etc/Universal');
    insert into tz_list (tz_name) values ('Etc/Zulu');
    insert into tz_list (tz_name) values ('Europe/Amsterdam');
    insert into tz_list (tz_name) values ('Europe/Andorra');
    insert into tz_list (tz_name) values ('Europe/Astrakhan');
    insert into tz_list (tz_name) values ('Europe/Athens');
    insert into tz_list (tz_name) values ('Europe/Belfast');
    insert into tz_list (tz_name) values ('Europe/Belgrade');
    insert into tz_list (tz_name) values ('Europe/Berlin');
    insert into tz_list (tz_name) values ('Europe/Bratislava');
    insert into tz_list (tz_name) values ('Europe/Brussels');
    insert into tz_list (tz_name) values ('Europe/Bucharest');
    insert into tz_list (tz_name) values ('Europe/Budapest');
    insert into tz_list (tz_name) values ('Europe/Busingen');
    insert into tz_list (tz_name) values ('Europe/Chisinau');
    insert into tz_list (tz_name) values ('Europe/Copenhagen');
    insert into tz_list (tz_name) values ('Europe/Dublin');
    insert into tz_list (tz_name) values ('Europe/Gibraltar');
    insert into tz_list (tz_name) values ('Europe/Guernsey');
    insert into tz_list (tz_name) values ('Europe/Helsinki');
    insert into tz_list (tz_name) values ('Europe/Isle_of_Man');
    insert into tz_list (tz_name) values ('Europe/Istanbul');
    insert into tz_list (tz_name) values ('Europe/Jersey');
    insert into tz_list (tz_name) values ('Europe/Kaliningrad');
    insert into tz_list (tz_name) values ('Europe/Kiev');
    insert into tz_list (tz_name) values ('Europe/Kirov');
    insert into tz_list (tz_name) values ('Europe/Kyiv');
    insert into tz_list (tz_name) values ('Europe/Lisbon');
    insert into tz_list (tz_name) values ('Europe/Ljubljana');
    insert into tz_list (tz_name) values ('Europe/London');
    insert into tz_list (tz_name) values ('Europe/Luxembourg');
    insert into tz_list (tz_name) values ('Europe/Madrid');
    insert into tz_list (tz_name) values ('Europe/Malta');
    insert into tz_list (tz_name) values ('Europe/Mariehamn');
    insert into tz_list (tz_name) values ('Europe/Minsk');
    insert into tz_list (tz_name) values ('Europe/Monaco');
    insert into tz_list (tz_name) values ('Europe/Moscow');
    insert into tz_list (tz_name) values ('Europe/Nicosia');
    insert into tz_list (tz_name) values ('Europe/Oslo');
    insert into tz_list (tz_name) values ('Europe/Paris');
    insert into tz_list (tz_name) values ('Europe/Podgorica');
    insert into tz_list (tz_name) values ('Europe/Prague');
    insert into tz_list (tz_name) values ('Europe/Riga');
    insert into tz_list (tz_name) values ('Europe/Rome');
    insert into tz_list (tz_name) values ('Europe/Samara');
    insert into tz_list (tz_name) values ('Europe/San_Marino');
    insert into tz_list (tz_name) values ('Europe/Sarajevo');
    insert into tz_list (tz_name) values ('Europe/Saratov');
    insert into tz_list (tz_name) values ('Europe/Simferopol');
    insert into tz_list (tz_name) values ('Europe/Skopje');
    insert into tz_list (tz_name) values ('Europe/Sofia');
    insert into tz_list (tz_name) values ('Europe/Stockholm');
    insert into tz_list (tz_name) values ('Europe/Tallinn');
    insert into tz_list (tz_name) values ('Europe/Tirane');
    insert into tz_list (tz_name) values ('Europe/Tiraspol');
    insert into tz_list (tz_name) values ('Europe/Ulyanovsk');
    insert into tz_list (tz_name) values ('Europe/Uzhgorod');
    insert into tz_list (tz_name) values ('Europe/Vaduz');
    insert into tz_list (tz_name) values ('Europe/Vatican');
    insert into tz_list (tz_name) values ('Europe/Vienna');
    insert into tz_list (tz_name) values ('Europe/Vilnius');
    insert into tz_list (tz_name) values ('Europe/Volgograd');
    insert into tz_list (tz_name) values ('Europe/Warsaw');
    insert into tz_list (tz_name) values ('Europe/Zagreb');
    insert into tz_list (tz_name) values ('Europe/Zaporozhye');
    insert into tz_list (tz_name) values ('Europe/Zurich');
    insert into tz_list (tz_name) values ('Factory');
    insert into tz_list (tz_name) values ('GB');
    insert into tz_list (tz_name) values ('GB-Eire');
    insert into tz_list (tz_name) values ('GMT+0');
    insert into tz_list (tz_name) values ('GMT-0');
    insert into tz_list (tz_name) values ('GMT0');
    insert into tz_list (tz_name) values ('Greenwich');
    insert into tz_list (tz_name) values ('HST');
    insert into tz_list (tz_name) values ('Hongkong');
    insert into tz_list (tz_name) values ('Iceland');
    insert into tz_list (tz_name) values ('Indian/Antananarivo');
    insert into tz_list (tz_name) values ('Indian/Chagos');
    insert into tz_list (tz_name) values ('Indian/Christmas');
    insert into tz_list (tz_name) values ('Indian/Cocos');
    insert into tz_list (tz_name) values ('Indian/Comoro');
    insert into tz_list (tz_name) values ('Indian/Kerguelen');
    insert into tz_list (tz_name) values ('Indian/Mahe');
    insert into tz_list (tz_name) values ('Indian/Maldives');
    insert into tz_list (tz_name) values ('Indian/Mauritius');
    insert into tz_list (tz_name) values ('Indian/Mayotte');
    insert into tz_list (tz_name) values ('Indian/Reunion');
    insert into tz_list (tz_name) values ('Iran');
    insert into tz_list (tz_name) values ('Israel');
    insert into tz_list (tz_name) values ('Jamaica');
    insert into tz_list (tz_name) values ('Japan');
    insert into tz_list (tz_name) values ('Kwajalein');
    insert into tz_list (tz_name) values ('Libya');
    insert into tz_list (tz_name) values ('MET');
    insert into tz_list (tz_name) values ('MST');
    insert into tz_list (tz_name) values ('MST7MDT');
    insert into tz_list (tz_name) values ('Mexico/BajaNorte');
    insert into tz_list (tz_name) values ('Mexico/BajaSur');
    insert into tz_list (tz_name) values ('Mexico/General');
    insert into tz_list (tz_name) values ('NZ');
    insert into tz_list (tz_name) values ('NZ-CHAT');
    insert into tz_list (tz_name) values ('Navajo');
    insert into tz_list (tz_name) values ('PRC');
    insert into tz_list (tz_name) values ('PST8PDT');
    insert into tz_list (tz_name) values ('Pacific/Apia');
    insert into tz_list (tz_name) values ('Pacific/Auckland');
    insert into tz_list (tz_name) values ('Pacific/Bougainville');
    insert into tz_list (tz_name) values ('Pacific/Chatham');
    insert into tz_list (tz_name) values ('Pacific/Chuuk');
    insert into tz_list (tz_name) values ('Pacific/Easter');
    insert into tz_list (tz_name) values ('Pacific/Efate');
    insert into tz_list (tz_name) values ('Pacific/Enderbury');
    insert into tz_list (tz_name) values ('Pacific/Fakaofo');
    insert into tz_list (tz_name) values ('Pacific/Fiji');
    insert into tz_list (tz_name) values ('Pacific/Funafuti');
    insert into tz_list (tz_name) values ('Pacific/Galapagos');
    insert into tz_list (tz_name) values ('Pacific/Gambier');
    insert into tz_list (tz_name) values ('Pacific/Guadalcanal');
    insert into tz_list (tz_name) values ('Pacific/Guam');
    insert into tz_list (tz_name) values ('Pacific/Honolulu');
    insert into tz_list (tz_name) values ('Pacific/Johnston');
    insert into tz_list (tz_name) values ('Pacific/Kanton');
    insert into tz_list (tz_name) values ('Pacific/Kiritimati');
    insert into tz_list (tz_name) values ('Pacific/Kosrae');
    insert into tz_list (tz_name) values ('Pacific/Kwajalein');
    insert into tz_list (tz_name) values ('Pacific/Majuro');
    insert into tz_list (tz_name) values ('Pacific/Marquesas');
    insert into tz_list (tz_name) values ('Pacific/Midway');
    insert into tz_list (tz_name) values ('Pacific/Nauru');
    insert into tz_list (tz_name) values ('Pacific/Niue');
    insert into tz_list (tz_name) values ('Pacific/Norfolk');
    insert into tz_list (tz_name) values ('Pacific/Noumea');
    insert into tz_list (tz_name) values ('Pacific/Pago_Pago');
    insert into tz_list (tz_name) values ('Pacific/Palau');
    insert into tz_list (tz_name) values ('Pacific/Pitcairn');
    insert into tz_list (tz_name) values ('Pacific/Pohnpei');
    insert into tz_list (tz_name) values ('Pacific/Ponape');
    insert into tz_list (tz_name) values ('Pacific/Port_Moresby');
    insert into tz_list (tz_name) values ('Pacific/Rarotonga');
    insert into tz_list (tz_name) values ('Pacific/Saipan');
    insert into tz_list (tz_name) values ('Pacific/Samoa');
    insert into tz_list (tz_name) values ('Pacific/Tahiti');
    insert into tz_list (tz_name) values ('Pacific/Tarawa');
    insert into tz_list (tz_name) values ('Pacific/Tongatapu');
    insert into tz_list (tz_name) values ('Pacific/Truk');
    insert into tz_list (tz_name) values ('Pacific/Wake');
    insert into tz_list (tz_name) values ('Pacific/Wallis');
    insert into tz_list (tz_name) values ('Pacific/Yap');
    insert into tz_list (tz_name) values ('Poland');
    insert into tz_list (tz_name) values ('Portugal');
    insert into tz_list (tz_name) values ('ROC');
    insert into tz_list (tz_name) values ('ROK');
    insert into tz_list (tz_name) values ('Singapore');
    insert into tz_list (tz_name) values ('Turkey');
    insert into tz_list (tz_name) values ('UCT');
    insert into tz_list (tz_name) values ('US/Alaska');
    insert into tz_list (tz_name) values ('US/Aleutian');
    insert into tz_list (tz_name) values ('US/Arizona');
    insert into tz_list (tz_name) values ('US/Central');
    insert into tz_list (tz_name) values ('US/East-Indiana');
    insert into tz_list (tz_name) values ('US/Eastern');
    insert into tz_list (tz_name) values ('US/Hawaii');
    insert into tz_list (tz_name) values ('US/Indiana-Starke');
    insert into tz_list (tz_name) values ('US/Michigan');
    insert into tz_list (tz_name) values ('US/Mountain');
    insert into tz_list (tz_name) values ('US/Pacific');
    insert into tz_list (tz_name) values ('US/Samoa');
    insert into tz_list (tz_name) values ('UTC');
    insert into tz_list (tz_name) values ('Universal');
    insert into tz_list (tz_name) values ('W-SU');
    insert into tz_list (tz_name) values ('WET');
    insert into tz_list (tz_name) values ('Zulu');
    commit;

    recreate table str2dts(
        dtp varchar(10), -- timing part: year/month/daynum/hour/minute/second/fract_seconds/timezone
        fmt varchar(10) unique using index str2dts_fmt_unq
        ,txt varchar(80) -- description
    );
    insert into str2dts(dtp, fmt, txt) values('yy', 'YEAR', 'Year');
    insert into str2dts(dtp, fmt, txt) values('yy', 'YYYY', 'Last 4 digits of Year');
    insert into str2dts(dtp, fmt, txt) values('yy', 'YYY', 'Last 3 digits of Year');
    insert into str2dts(dtp, fmt, txt) values('yy', 'YY', 'Last 2 digits of Year');
    insert into str2dts(dtp, fmt, txt) values('yy', 'Y', 'Last 1 digit of Year');
    insert into str2dts(dtp, fmt, txt) values('', 'Q', 'Quarter of the Year, 1 .. 4');
    insert into str2dts(dtp, fmt, txt) values('mm', 'MM', 'Month, 1 .. 12');
    insert into str2dts(dtp, fmt, txt) values('mm', 'MON', 'Short Month name');
    insert into str2dts(dtp, fmt, txt) values('mm', 'MONTH', 'Full Month name');

    insert into str2dts(dtp, fmt, txt) values('mm', 'RM', 'Roman representation of the Month, I .. XII');

    insert into str2dts(dtp, fmt, txt) values('', 'WW', 'Week of the Year, 01 .. 53');
    insert into str2dts(dtp, fmt, txt) values('', 'W', 'Week of the Month 1 .. 5');
    insert into str2dts(dtp, fmt, txt) values('dd', 'D', 'Day of the Week, 1 .. 7');
    insert into str2dts(dtp, fmt, txt) values('dd', 'DAY', 'Full name of the Day: MONDAY, TUESDAY, ...');
    insert into str2dts(dtp, fmt, txt) values('dd', 'DD', 'Day of the Month, 01 .. 31)');
    insert into str2dts(dtp, fmt, txt) values('dd', 'DDD', 'Day of the Year, 001 .. 366');
    insert into str2dts(dtp, fmt, txt) values('dd', 'DY', 'Short name of the Day: Mon, Tue, ...');
    insert into str2dts(dtp, fmt, txt) values('', 'J', 'Julian Day (number of days since January 1, 4712 BC)');
    insert into str2dts(dtp, fmt, txt) values('hh', 'HH', 'Hour of the Day (01 - 12) without Period');
    insert into str2dts(dtp, fmt, txt) values('hh', 'HH12', 'Hour of the Day (01 - 12) without Period');
    insert into str2dts(dtp, fmt, txt) values('hh', 'HH24', 'Hour of the Day (00 - 23)');
    insert into str2dts(dtp, fmt, txt) values('mi', 'MI', 'Minutes, 00 .. 59');
    insert into str2dts(dtp, fmt, txt) values('ss', 'SS', 'Seconds, 00 .. 59');
    insert into str2dts(dtp, fmt, txt) values('sm', 'SSSSS', 'Seconds after midnight, 0 .. 86399');
    insert into str2dts(dtp, fmt, txt) values('ff', 'FF1', 'Fractional seconds with the accuracy 1');
    insert into str2dts(dtp, fmt, txt) values('ff', 'FF2', 'Fractional seconds with the accuracy 2');
    insert into str2dts(dtp, fmt, txt) values('ff', 'FF3', 'Fractional seconds with the accuracy 3');
    insert into str2dts(dtp, fmt, txt) values('ff', 'FF4', 'Fractional seconds with the accuracy 4');
    insert into str2dts(dtp, fmt, txt) values('tz', 'TZH', 'Time zone in Hours, -14 .. 14');
    insert into str2dts(dtp, fmt, txt) values('tz', 'TZM', 'Time zone in Minutes, 0 .. 59');
    insert into str2dts(dtp, fmt, txt) values('tz', 'TZR', 'Time zone Name');
    commit;


    recreate table dts2str(
         dtp varchar(10)
        ,fmt varchar(10) unique using index dts2str_fmt_unq
        ,txt varchar(80) -- description
    );

    insert into dts2str(dtp, fmt, txt) values('yy', 'YEAR', 'Year');
    insert into dts2str(dtp, fmt, txt) values('yy', 'YYYY', 'Last 4 digits of Year');
    insert into dts2str(dtp, fmt, txt) values('yy', 'YYY', 'Last 3 digits of Year');
    insert into dts2str(dtp, fmt, txt) values('yy', 'YY', 'Last 2 digits of Year');
    insert into dts2str(dtp, fmt, txt) values('yy', 'Y', 'Last 1 digit of Year');
    -- insert into dts2str(dtp, fmt, txt) values('yy', 'RR', 'Round Year');
    -- insert into dts2str(dtp, fmt, txt) values('yy', 'RRRR', 'Round Year');
    insert into dts2str(dtp, fmt, txt) values('mm', 'MM', 'Month, 1 .. 12');
    insert into dts2str(dtp, fmt, txt) values('mm', 'MON', 'Short Month name');
    insert into dts2str(dtp, fmt, txt) values('mm', 'MONTH', 'Full Month name');

    insert into dts2str(dtp, fmt, txt) values('mm', 'RM', 'Roman representation of the Month, I .. XII');

    insert into dts2str(dtp, fmt, txt) values('dd', 'DD', 'Day of the Month, 1 .. 31');
    insert into dts2str(dtp, fmt, txt) values('', 'J', 'Julian Day (number of days since January 1, 4712 BC)');
    insert into dts2str(dtp, fmt, txt) values('hh', 'HH', 'Hour of the Day (1 - 12) without Period');
    insert into dts2str(dtp, fmt, txt) values('hh', 'HH12', 'Hour of the Day (1 - 12) without Period');
    insert into dts2str(dtp, fmt, txt) values('hh', 'HH24', 'Hour of the Day (0 - 23)');
    insert into dts2str(dtp, fmt, txt) values('', 'A.M.', 'Period for 12 hours time');
    insert into dts2str(dtp, fmt, txt) values('', 'P.M.', 'Period for 12 hours time');
    insert into dts2str(dtp, fmt, txt) values('mi', 'MI', 'Minutes, 0 .. 59');
    insert into dts2str(dtp, fmt, txt) values('ss', 'SS', 'Seconds, 0 .. 59');
    insert into dts2str(dtp, fmt, txt) values('sm', 'SSSSS', 'Seconds after midnight, 0 .. 86399');
    insert into dts2str(dtp, fmt, txt) values('ff', 'FF1', 'Fractional seconds with the accuracy 1');
    insert into dts2str(dtp, fmt, txt) values('ff', 'FF2', 'Fractional seconds with the accuracy 2');
    insert into dts2str(dtp, fmt, txt) values('ff', 'FF3', 'Fractional seconds with the accuracy 3');
    insert into dts2str(dtp, fmt, txt) values('ff', 'FF4', 'Fractional seconds with the accuracy 4');
    insert into dts2str(dtp, fmt, txt) values('tz', 'TZH', 'Time zone in Hours, -14 .. 14');
    insert into dts2str(dtp, fmt, txt) values('tz', 'TZM', 'Time zone in Minutes, 0 .. 59');
    insert into dts2str(dtp, fmt, txt) values('tz', 'TZR', 'Time zone Name');
    commit;

    set term ^;
    set bail off ^

    --/*********************
    -- CHECK-1.
    -- Generate stetements to convert from timestamp to varchar with time '00:00:00',
    -- then convert from this text to DATE and finally - from date back to varchar.
    -- Final text representation of date must be equal to starting one.
    -- exec time: ~140 seconds.
    execute block returns( 
         dtx timestamp with time zone
        ,fmt varchar(15)
        ,ts_as_text varchar(50)
        ,txt_as_dt date
        ,dt_as_text varchar(50)
        ,equals boolean
    ) as
        declare n_err int = 0;
    begin
        rdb$set_context('USER_SESSION','CHECK_1','FAILED');
        delete from tmp;
        insert into tmp(tm_tz_txt)
            with
            i as (
                select cast( dateadd( rand()*86399*1000*365 millisecond to timestamp '01.01.2023 00:00:00') as varchar(24) ) time_txt from rdb$database
            )
            ,z as (
                select i.time_txt || ' ' || z.tz_name as tm_tz_txt from i cross join tz_list z
            )
            select * from z
        ;
        -----------------------------
        for
            with
            i as (
                select t.tm_tz_txt from tmp t -- rows 1
            )
            ,s as (
                select a.dtp, a.fmt
                from dts2str a
                where a.dtp > ''
            )
            ,ymd as (
                select
                  a.fmt as fmt_a
                 ,b.fmt as fmt_b
                 ,c.fmt as fmt_c
                from s a
                join s b on a.dtp<> b.dtp
                join s c on c.dtp not in (a.dtp, b.dtp)
                where
                    a.dtp in ('dd','mm','yy')
                    and b.dtp in ('dd','mm','yy')
                    and c.dtp in ('dd','mm','yy')
            )
            select i.tm_tz_txt, y.*, d.d as token_delimiter
            from ymd y cross join i cross join (select d.d from fmt_delimiter d rows 5) d
        as cursor c
        do begin
            dtx = c.tm_tz_txt;
            fmt = trim(c.fmt_a) || c.token_delimiter || trim(c.fmt_b) || c.token_delimiter || trim(c.fmt_c);
            execute statement ( 'select cast(timestamp ''' || dtx || ''' as varchar(50) format '''|| fmt || ''') from rdb$database' )    -- timestamp ==> date_as_txt
            into ts_as_text;

            execute statement ( 'select cast(''' || ts_as_text || ''' as date format '''|| fmt || ''') from rdb$database' )              -- date_as_txt ==> date
            into txt_as_dt;

            execute statement ( 'select cast(date ''' || txt_as_dt || ''' as varchar(50) format '''|| fmt || ''') from rdb$database' )   -- date ==> date_as_txt2
            into dt_as_text;

            equals = ts_as_text is not distinct from dt_as_text;

            if (not equals) then
            begin
                n_err = n_err + 1;
                suspend;
            end
        end
        if (n_err = 0) then
            rdb$set_context('USER_SESSION','CHECK_1','PASSED');
        else
            rdb$set_context('USER_SESSION','CHECK_1','FAILED ' || n_err || 'statements');

    end
    ^
    select rdb$get_context('USER_SESSION','CHECK_1') as check_1 from rdb$database
    ^

    -- *******************/

    --/*********************
    -- CHECK-2.
    -- Generate stetements to convert from timestamp to varchar with date cut-off,
    -- then convert from this text to time and finally - from time back to varchar.
    -- Final text representation of date must be equal to starting one.
    -- NOTE: exec time is ~130 s.
    execute block returns(
         dtx time with time zone
        ,fmt varchar(50)
        ,tm_as_text varchar(50)
        ,txt_as_time time
        ,tm_back_to_text varchar(50) 
        ,equals boolean
    ) as
        declare fmt_wo_tz varchar(50);
        declare n_err int = 0;
    begin

        rdb$set_context('USER_SESSION','CHECK_2','FAILED');

        delete from tmp;
        insert into tmp(tm_tz_txt)
            with
            i as (
                select cast( dateadd( rand()*86399*1000 millisecond to time '00:00:00') as varchar(13) ) time_txt from rdb$database
            )
            ,z as (
                select i.time_txt || ' ' || z.tz_name as tm_tz_txt from i cross join tz_list z
            )
            select * from z
        ;
        ---------------------------
        for
            with
            i as (
                select t.tm_tz_txt from tmp t -- rows 10
            )
            ,s as (
                select a.dtp, trim(a.fmt || coalesce(t.mer, '')) as fmt
                from dts2str a
                -- HH/HH12 can't be used without A.M./P.M. and vice versa
                left join ( select ' A.M.' as mer from rdb$database union all select ' P.M.' from rdb$database ) t on a.fmt in ('HH', 'HH12')
                where a.dtp > ''
            )
            ,hms as (
                select
                  a.fmt as fmt_a
                 ,b.fmt as fmt_b
                 ,c.fmt as fmt_c
                 ,d.fmt as fmt_d
                from s a
                join s b on a.dtp<> b.dtp
                join s c on c.dtp not in (a.dtp, b.dtp)
                join s d on d.dtp not in (a.dtp, b.dtp, c.dtp)
                where
                    a.dtp in ('hh','mi','ss', 'ff')
                    and b.dtp in ('hh','mi','ss', 'ff')
                    and c.dtp in ('hh','mi','ss', 'ff')
                    and d.dtp in ('hh','mi','ss', 'ff')

            )
            select i.tm_tz_txt, h.* from hms h cross join i
            -- where hms.fmt_a = 'HH24' ROWS 1
        as cursor c
        do begin
            dtx = c.tm_tz_txt;
            fmt = trim(c.fmt_a) || ':' || trim(c.fmt_b) || ':' || trim(c.fmt_c) || ':' || trim(c.fmt_d);
            execute statement ( 'select cast(time ''' || dtx || ''' as varchar(50) format '''|| fmt || ''') from rdb$database -- 1' )
            into tm_as_text;

            execute statement ( 'select cast(''' || tm_as_text || ''' as time format '''|| fmt || ''') from rdb$database -- 2' )
            --execute statement ( 'select cast(time ''' || tm_as_text || ''' as varchar(50) format '''|| fmt || ''') from rdb$database' )
            --execute statement ( 'select cast(''' || tm_as_text || ''' as time format '''|| fmt_wo_tz || ''') from rdb$database' )
            into txt_as_time;
            
            --execute statement ( 'select cast( cast(''' || txt_as_time || ''' as time with time zone) as varchar(50) format '''|| fmt || ''') from rdb$database' )
            --execute statement ( 'select cast(''' || txt_as_time || ''' as time) from rdb$database' )
            execute statement ( 'select cast(time ''' || txt_as_time || ''' as varchar(50) format ''' || fmt || ''') from rdb$database -- 3' )
            into tm_back_to_text;

            equals = tm_as_text is not distinct from tm_back_to_text;

            if (not equals) then
            begin
                n_err = n_err + 1;
                suspend;
            end
        end

        if (n_err = 0) then
            rdb$set_context('USER_SESSION','CHECK_2','PASSED');
        else
            rdb$set_context('USER_SESSION','CHECK_2','FAILED ' || n_err || 'statements');
    end
    ^
    select rdb$get_context('USER_SESSION','CHECK_2') as check_2 from rdb$database
    ^
    --***************/

    -- CHECK-3.
    execute block returns( fmt varchar(512), tmtz_as_txt varchar(512), txt_as_tmtz time with time zone, tmtz_back_to_txt varchar(512), equals boolean ) as
        declare fmt_wo_tz varchar(100);
        declare tmtz_random_as_txt varchar(100);
        declare v_cast_to_tmtz_as_txt varchar(512);
        declare v_cast_to_txt_as_tmtz varchar(512);
        declare v_tmtz_back_to_txt varchar(512);
        declare n_err int = 0;
    begin
        rdb$set_context('USER_SESSION','CHECK_3','FAILED');

        for
            with
            s as (
                select a.dtp, a.fmt
                from dts2str a
                where a.dtp > ''
            )
            ,hms as (
                select
                     a.fmt as fmt_a --  trim(a.fmt || iif(a.fmt in ('HH', 'HH12'), m.mer, '')) as fmt_a
                    ,b.fmt as fmt_b
                    ,trim(c.fmt || '.' || f.fmt || iif(a.fmt in ('HH', 'HH12'), m.mer, '')) as fmt_c
                    ,d.fmt as fmt_d -- TZH/TZR
                    ,e.fmt as fmt_e -- TZM/TZR
                from s a
                cross join (select ' A.M.' as mer from rdb$database union all select ' P.M.' from rdb$database) m
                cross join s b
                cross join s c
                cross join s d
                cross join s e
                cross join s f
                where
                    a.dtp = 'hh'
                    and b.dtp = 'mi'
                    and c.dtp = 'ss'
                    and (
                            d.fmt = 'TZH' and e.fmt = 'TZM'    -- Time zone in Hours, -14 .. 14; Time zone in Minutes, 0 .. 59
                            or d.fmt = 'TZR' and e.fmt = 'TZR' -- Time zone Name
                        )
                    and f.dtp = 'ff'
            )
            select h.fmt_a, h.fmt_b, h.fmt_c, h.fmt_d, h.fmt_e, z.tz_name, d.d as token_delimiter
            from hms h
            cross join tz_list z
            cross join (select d.d from fmt_delimiter d rows 5) d
            --where h.fmt_d = 'TZR'
            -- where h.fmt_d = 'TZH' and h.fmt_e = 'TZM' rows 1
            -- h.fmt_a = 'HH24' 
            -- ROWS 1
        as cursor c
        do begin
            
            tmtz_random_as_txt = cast( dateadd(rand()*86399*1000 millisecond to time '00:00:00') as varchar(13)) || ' ' || c.tz_name;

            fmt = trim(c.fmt_a) || c.token_delimiter || trim(c.fmt_b) || c.token_delimiter || trim(c.fmt_c) || ' ';
            if (c.fmt_d = 'TZH') then
                fmt = fmt || c.fmt_d || c.token_delimiter || c.fmt_e;
            else
                fmt = fmt || c.fmt_d;

            v_cast_to_tmtz_as_txt = 'select cast( cast(''' || tmtz_random_as_txt || ''' as time with time zone) as varchar(100) format '''|| fmt || ''') from rdb$database';
            execute statement ( v_cast_to_tmtz_as_txt )
            into tmtz_as_txt;

            v_cast_to_txt_as_tmtz = 'select cast(''' || tmtz_as_txt || ''' as time with time zone format '''|| fmt || ''') from rdb$database';
            execute statement ( v_cast_to_txt_as_tmtz )
            into txt_as_tmtz;

            v_tmtz_back_to_txt = 'select cast( cast(''' || txt_as_tmtz || ''' as time with time zone) as varchar(100) format ''' || fmt || ''') from rdb$database';
            execute statement ( v_tmtz_back_to_txt )
            into tmtz_back_to_txt;

            
            equals = tmtz_as_txt is not distinct from tmtz_back_to_txt;

            if (not equals) then
            begin
                fmt = 'WRONG: ' || fmt;
                tmtz_as_txt = 'WRONG: ' || tmtz_as_txt || ' ' || v_cast_to_tmtz_as_txt || ';' || ' ' || v_cast_to_txt_as_tmtz || ';' ;
                tmtz_back_to_txt = 'WRONG: ' || tmtz_back_to_txt || ' ' || v_tmtz_back_to_txt || ';' ;
            end

            if (not equals) then
            begin
                n_err = n_err + 1;
                suspend;
            end
        end

        if (n_err = 0) then
            rdb$set_context('USER_SESSION','CHECK_3','PASSED');
        else
            rdb$set_context('USER_SESSION','CHECK_3','FAILED ' || n_err || 'statements');
    end
    ^
    select rdb$get_context('USER_SESSION','CHECK_3') as check_3 from rdb$database
    ^
    set term ;^
"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

@pytest.mark.version('>=6.0.0')
def test_1(act: Action):

    expected_stdout = f"""
        CHECK_1                         PASSED
        CHECK_2                         PASSED
        CHECK_3                         PASSED
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

