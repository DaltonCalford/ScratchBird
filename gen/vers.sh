#!/bin/sh
#
#  The contents of this file are subject to the Initial
#  Developer's Public License Version 1.0 (the "License");
#  you may not use this file except in compliance with the
#  License. You may obtain a copy of the License at
#  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
#
#  Software distributed under the License is distributed AS IS,
#  WITHOUT WARRANTY OF ANY KIND, either express or implied.
#  See the License for the specific language governing rights
#  and limitations under the License.
#
#  The Original Code was created by Alex Peshkov
#  for the Firebird Open Source RDBMS project.
#
#  Copyright (c) 2010 Alex Peshkov <peshkoff@mail.ru>
#  and all contributors signed below.
#
#  All Rights Reserved.
#  Contributor(s): ______________________________________.
#

# Different echo versions interpret backslash escapes differently.
# To avoid related problems lets have:
TAB="	"

platform_aix() {
	echo '#!'

	for i in `grep -v '#' ${1}`
	do
		echo "${TAB}$i"
	done
}

platform_darwin() {
for i in `grep -v '#' ${1}`
do
	echo "${TAB}_$i"
done
}

platform_hpux() {
	for i in `grep -v '#' ${1}`
	do
		echo "+e $i"
	done
}

platform_linux() {
	echo '{'

	if grep -qv '^#' "${1}"; then
		echo 'global:'
	fi

	for i in `grep -v '#' ${1}`
	do
		echo "${TAB}$i;"
	done

	echo 'local:'
	echo "${TAB}*;"
	echo '};'
}

# main
FILE=${1}
if test -z "$FILE"
then
	echo "Usage: vers.sh export-symbol-file-name"
	exit 1
fi

FROM="../builds/posix/$FILE"
if test ! -f "$FROM"
then
	echo "vers.sh: missing $FROM"
	exit 1
fi

TO="$FILE"

platform_linux $FROM >$TO
