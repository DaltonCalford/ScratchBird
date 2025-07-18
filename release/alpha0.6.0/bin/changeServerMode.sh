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
#  The Original Code was created by Alex Peshkoff
#  for the Firebird Open Source RDBMS project.
#
#  Copyright (c) 2008 Alex Peshkoff <peshkoff@mail.ru>
#  and all contributors signed below.
#
#  All Rights Reserved.
#  Contributor(s): ______________________________________.
#

#= Main ====================================================================

cat <<EOF
Firebird server may run in 2 different modes - super and classic.
Super server provides better performance, classic - better availability.

*******************************************************
* This script is deprecated and will be removed soon: *
*         edit firebird.conf directly instead.        *
*******************************************************

EOF

AskQuestion "Which option would you like to choose: (super|classic) [super] " "super"
multiAnswer=$Answer

case "$multiAnswer" in
s)
	multiAnswer=super
	;;
c)
	multiAnswer=classic
	;;
super)
	;;
classic)
	;;
*)
	echo "Unknown option $multiAnswer chosen"
	exit 1
	;;
esac

echo "Stopping currently running engine..."
checkIfServerRunning

sc=Starting
[ ${fb_install_prefix} = ${default_prefix} ] || sc=Configure
echo "$sc firebird in $multiAnswer server mode..."
fbconf="${fb_install_prefix}/firebird.conf"
if [ $multiAnswer = classic ]; then
	replaceLineInFile $fbconf "ServerMode = Classic" "^ServerMode"
else
	replaceLineInFile $fbconf "ServerMode = Super" "^ServerMode"
fi

startFirebird

echo "Done."
