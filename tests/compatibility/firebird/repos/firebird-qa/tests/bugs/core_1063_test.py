#coding:utf-8

"""
ID:          issue-1484
ISSUE:       1484
TITLE:       Server hangs eating CPU and performs huge I/O copying different codepage fields
DESCRIPTION:
JIRA:        CORE-1063
FBTEST:      bugs.core_1063
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core1063.fbk')

test_script = """alter table cms_wiki
add u_name varchar(100) character set UTF8,
add u_title varchar(100) character set UTF8,
add u_description blob sub_type 1 character set UTF8,
add u_text blob sub_type 1 character set UTF8;

commit;

update cms_wiki set
u_name = name, u_title = title, u_text = text, u_description = description;

commit;

drop index uq_cms_wiki;

alter table cms_wiki drop name, drop description, drop text, drop title;

commit;

alter table cms_wiki
alter u_name to name,
alter u_title to title,
alter u_description to description,
alter u_text to text;

commit;

CREATE UNIQUE INDEX UQ_CMS_WIKI ON CMS_WIKI (NAME, SITE_ID);

commit;
set heading off;
select name from CMS_WIKI;
"""

act = isql_act('db', test_script)

expected_stdout = """
	.menu
	awards.htm
	affiliate_program.htm
	user_guide.htm
	forms/feedback.htm
	faq.htm
	products.htm
	zonetick_v2.7_plan.htm
	fxstreet.htm
	confirmed.htm
	index.htm
	mail/confirm
	launch.htm
	.menu
	forms/why_uninstall.htm
	modification_guidelines.htm
	forms/feature_request.htm
	addition_minneapolis.htm
	cities.htm
	index.htm

	index.meta
	index.htm
	database_format_discussion.htm
	.menu
	profiles.htm
	modification_guidelines.htm
	update.htm
	oem.htm
	content_providers.htm
	addition_hamburg.htm
	forms/confirm.htm
	packs.htm
	zonetick_v2.7_released.htm
	quotes.htm
	skins.htm
	feedback.htm
	addition_phoenix.htm
	zonetick_v3.0_plan.htm
	wrtimetracker_v0.4_released.htm
	zonetick_v2.7_beta.htm


	jobs.htm
	contact.htm
	wrtimetracker/faq.htm
	wrtimetracker/user_manual.htm
	addition_salt_lake_city.htm
	wrtimetracker/index.htm
	wrtimetracker/license.htm
	addition_singapore.htm
	addition_lipetsk.htm
	addition_miami.htm
	addition_miami.htm
	.menu_zonetick
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

