#! /bin/sh
#
#  This library is part of the Firebird project
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#  You may obtain a copy of the Licence at
#  http://www.gnu.org/licences/lgpl.html
#  
#  As a special exception this file can also be included in modules
#  with other source code as long as that source code has been 
#  released under an Open Source Initiative certified licence.
#  More information about OSI certification can be found at: 
#  http://www.opensource.org 
#  
#  This module is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public Licence for more details.
#  
#  This module was created by members of the firebird development 
#  team.  All individual contributions remain the Copyright (C) of 
#  those individuals and all rights are reserved.  Contributors to 
#  this file are either listed below or can be obtained from a CVS 
#  history command.
# 
#   Created by:  Mark O'Donohue <mark.odonohue@firebirdsql.org>
# 
#   Contributor(s):
#   Alex Peshkoff
# 

fb_install_prefix=/usr/local/firebird

#------------------------------------------------------------------------------
# appendDatabaseEntry
# check to see if an entry already exists in the databases.conf file
# if it doesn't append it to the end of the file

appendDatabaseEntry() {
    aliasName=$1
    newDB=$2

    # check if aliasName already exists
    oldLine=`grep "^$aliasName" $DatabaseFile`
    if [ -z "$oldLine" ]
      then
        # Create the alias file entry
        echo "$aliasName = $newDB" >> $DatabaseFile
      else
        cat <<EOF
The alias name $aliasName already exists in $DatabaseFile with value:
$oldLine

A new entry will not be created and the existing one will be used.
EOF

        exit
    fi

}


#------------------------------------------------------------------------------
# checkNameStartsWithSlash
# 

checkNameStartsWithSlash() {

    name=$1

    letter=`echo $name | cut -c1`

    if [ $letter != "/" ]
      then
        cat <<EOF
The file name "$name" needs to be an absolute path to the file not a 
relative path.  

Possibly you want: `pwd`/$name
for the path name.
EOF
        exit
    fi
}


#------------------------------------------------------------------------------
# checkAccessToFile
# This routine should check that the firebird user has 
# access to the file if it exists or the directory if it 
# does not.

checkAccessToFile() {
    fileName=$1

    dirName=`dirname $fileName`

    echo $fileName 
    echo $dirName

    userName=`id -un`
    if [ $userName = "root" ]
      then
        result=`su firebird -c " if [ -w $dirName ]; then echo 'Ok'; fi"`
    fi


}

#= Main Post ===============================================================

    if [ $# -ne 2 ]
      then
        echo "Usage is registerDatabase.sh <alias> <database>"
        exit
    fi

    aliasName=$1
    newDB=$2

    DatabaseFile=${fb_install_prefix}/databases.conf

    checkNameStartsWithSlash $newDB
    appendDatabaseEntry $aliasName $newDB

#    checkAccessToFile $newDB

    if [ ! -f $newDB ]
      then
        ${fb_install_prefix}/bin/isql <<EOF
create database '$aliasName';
quit;
EOF
    fi

	chown firebird:firebird $newDB
