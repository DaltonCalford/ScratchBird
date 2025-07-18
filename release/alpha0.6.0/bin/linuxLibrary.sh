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
#  The Original Code was created by Mark O'Donohue
#  for the Firebird Open Source RDBMS project.
#
#  Copyright (c) Mark O'Donohue <mark.odonohue@ludwig.edu.au>
#  and all contributors signed below.
#
#  All Rights Reserved.
#  Contributor(s): ______________________________________.
#		Alex Peshkoff
#

RunUser=firebird
export RunUser
RunGroup=firebird
export RunGroup
PidDir=/var/run/firebird
export PidDir

#------------------------------------------------------------------------
# Get correct options & misc.

psOptions=-efaww
export psOptions
mktOptions=-q
export mktOptions
tarOptions=z
export tarOptions
tarExt=tar.gz
export tarExt

#------------------------------------------------------------------------
#  Add new user and group

TryAddGroup() {

	AdditionalParameter=$1
	testStr=`grep $RunGroup /etc/group`

    if [ -z "$testStr" ]
      then
        groupadd $AdditionalParameter $RunGroup
    fi

}


TryAddUser() {

	AdditionalParameter=$1
	testStr=`grep $RunUser /etc/passwd`

    if [ -z "$testStr" ]
      then
        useradd $AdditionalParameter -d ${fb_install_prefix} -s /sbin/nologin \
            -c "Firebird Database Owner" -g $RunUser $RunGroup
    fi

}


addFirebirdUser() {

	TryAddGroup "-g 84 -r" >/dev/null 2>/dev/null
	TryAddGroup "-g 84" >/dev/null 2>/dev/null
	TryAddGroup "-r" >/dev/null 2>/dev/null
	TryAddGroup " "

	TryAddUser "-u 84 -r -M" >/dev/null 2>/dev/null
	TryAddUser "-u 84 -M" >/dev/null 2>/dev/null
	TryAddUser "-r -M" >/dev/null 2>/dev/null
	TryAddUser "-M" >/dev/null 2>/dev/null
	TryAddUser "-u 84 -r" >/dev/null 2>/dev/null
	TryAddUser "-u 84" >/dev/null 2>/dev/null
	TryAddUser "-r" >/dev/null 2>/dev/null
	TryAddUser " "

}


#------------------------------------------------------------------------
#  print location of init script

getInitScriptLocation() {
    if [ -f /etc/rc.d/init.d/${fb_startup_name} ]
	then
		printf %s /etc/rc.d/init.d/${fb_startup_name}
    elif [ -f /etc/rc.d/rc.${fb_startup_name} ]
	then
		printf %s /etc/rc.d/rc.${fb_startup_name}
    elif [ -f /etc/init.d/${fb_startup_name} ]
	then
		printf %s /etc/init.d/${fb_startup_name}
    fi
}


#------------------------------------------------------------------------
#  register/start/stop server using systemd

SYSTEMCTL=systemctl
CTRL=${fb_startup_name}.service
SYSTEMD_DIR=/usr/lib/systemd/system
[ -d $SYSTEMD_DIR ] || SYSTEMD_DIR=/lib/systemd/system
TMPFILE_CONF=/usr/lib/tmpfiles.d/firebird.conf

systemdPresent() {
	proc1=`ps -p 1 -o comm=`

	[ "${proc1}" = systemd ] && return 0
	return 1
}

systemdError() {
	echo "Fatal error running '${1}' - exiting"
	exit 1
}

installSystemdCtrlFiles() {
	if systemdPresent
	then
		if [ ! -d ${SYSTEMD_DIR} ]
		then
			echo Missing /usr/lib/systemd/system or /lib/systemd/system
			echo but systemd seems to be running.
			echo Misconfigured - can not proceed with FB install.
			exit 1
		fi

		editFile "${fb_install_prefix}/misc/firebird.service" ExecStart "ExecStart=${fb_install_prefix}/bin/fbguard -daemon -forever"
		cp ${fb_install_prefix}/misc/firebird.service "${SYSTEMD_DIR}/${fb_startup_name}.service"

		mkdir -p ${PidDir}
		chown $RunUser:$RunGroup ${PidDir}
		chmod 0775 ${PidDir}
		echo "d ${PidDir} 0775 $RunUser $RunGroup -" >${TMPFILE_CONF}
	fi
}

osRemoveStartupFiles() {
	rm -f ${SYSTEMD_DIR}/${fb_startup_name}.*
	rm -f ${TMPFILE_CONF}
}

systemdSrv() {
	op=${1}
	ctrl=${2}

	if systemdPresent
	then
		if [ "${op}" = "stop" -o "${op}" = "disable" ]
		then
			if [ ! -f ${SYSTEMD_DIR}/${ctrl} ]
			then
				return 0
			fi
		fi

		${SYSTEMCTL} --quiet ${op} ${ctrl} || systemdError "${SYSTEMCTL} --quiet ${op} ${ctrl}"
		return 0
	fi
	return 1
}

superSrv() {
	op=${1}

	systemdSrv ${op} ${CTRL}
}

registerSuperServer() {
	installSystemdCtrlFiles

	if [ "${fb_install_prefix}" = "${default_prefix}" ]
	then
		superSrv enable && return 0
		return 1
	fi

	systemdPresent && return 0
	return 1
}

unregisterSuperServer() {
	superSrv disable && return 0
}

startSuperServer() {
	superSrv start && return 0
}

stopSuperServer() {
	superSrv stop && return 0

	init_d=`getInitScriptLocation`
	if [ -x "$init_d" ]
	then
		$init_d stop
		return 0
	fi

	return 1
}



#------------------------------------------------------------------------
#  stop super server if it is running

stopSuperServerIfRunning() {
	checkString=`grepProcess "fbserver|fbguard|fb_smp_server|firebird"`

    if [ ! -z "$checkString" ]
    then
		i=1
		while [ $i -le 20 ]
		do
   	    	stopSuperServer || return	# silently giveup
			sleep 1
			checkString=`grepProcess "fbserver|fbguard|fb_smp_server|firebird"`
			if [ -z "$checkString" ]
			then
				return
			fi
			i=$((i+1))
		done
    fi
}

#------------------------------------------------------------------------
#  Create new password string - this routine is used only in
#  silent mode of the install script.

createNewPassword() {
    # openssl generates random data.
	openssl </dev/null >/dev/null 2>/dev/null
    if [ $? -eq 0 ]
    then
        # We generate 40 random chars, strip any '/''s and get the first 20
        NewPasswd=`openssl rand -base64 40 | tr -d '/' | cut -c1-20`
    fi

	# If openssl is missing...
	if [ -z "$NewPasswd" ]
	then
		NewPasswd=`dd if=/dev/urandom bs=10 count=1 2>/dev/null | od -x | head -n 1 | tr -d ' ' | cut -c8-27`
	fi

	# On some systems even this routines may be missing. So if
	# the specific one isn't available then keep the original password.
    if [ -z "$NewPasswd" ]
    then
        NewPasswd="masterkey"
    fi

	echo "$NewPasswd"
}

#------------------------------------------------------------------------
# installInitdScript
# Everybody stores this one in a separate location, so there is a bit of
# running around to actually get it for each packager.
# Update rcX.d with Firebird initd entries
# initd script for SuSE >= 7.2 is a part of RPM package

installInitdScript() {
	# systemd case
	registerSuperServer && return 0		# systemd's service file takes care about PidDir

	srcScript=""
	initScript=

# SuSE specific

    if [ -r /etc/SuSE-release ]
    then
        srcScript=firebird.init.d.suse
        initScript=/etc/init.d/${fb_startup_name}
        rm -f /usr/sbin/rc${fb_startup_name}
        ln -s ../../etc/init.d/${fb_startup_name} /usr/sbin/rc${fb_startup_name}

# Debian specific

    elif [ -r /etc/debian_version ]
    then
        srcScript=firebird.init.d.debian
        initScript=/etc/init.d/${fb_startup_name}
        rm -f /usr/sbin/rc${fb_startup_name}
        ln -s ../../etc/init.d/${fb_startup_name} /usr/sbin/rc${fb_startup_name}

# Slackware specific

    elif [ -r /etc/slackware-version ]
    then
        srcScript=firebird.init.d.slackware
        initScript=/etc/rc.d/rc.${fb_startup_name}
		rclocal=/etc/rc.d/rc.local
		if ! grep -q "$initScript" $rclocal
		then
			cat >>$rclocal <<EOF
if [ -x $initScript ] ; then
	$initScript start
fi
EOF
		fi

# Gentoo specific

    elif [ -r /etc/gentoo-release ]
    then
        srcScript=firebird.init.d.gentoo
        initScript=/etc/init.d/${fb_startup_name}

# This is for RH and MDK specific

    elif [ -e /etc/rc.d/init.d/functions ]		# very generic check - may be should go later to avoid fault mandrake detection ???
    then
        srcScript=firebird.init.d.mandrake
        initScript=/etc/rc.d/init.d/${fb_startup_name}

# Generic...

    elif [ -d /etc/rc.d/init.d ]
    then
        srcScript=firebird.init.d.generic
        initScript=/etc/rc.d/init.d/${fb_startup_name}
    fi


	if [ "$initScript" ]
	then
	    # Install the firebird init.d script
    	cp ${fb_install_prefix}/misc/$srcScript $initScript
	    chown root:root $initScript
    	chmod u=rwx,g=rx,o=r $initScript

		if [ "${fb_install_prefix}" = "${default_prefix}" ]
		then
		    # RedHat and Mandrake specific
    		if [ -x /sbin/chkconfig ]
	    	then
    	    	/sbin/chkconfig --add ${fb_startup_name}

		    # Gentoo specific
    		elif [ -x /sbin/rc-update ]
	    	then
				/sbin/rc-update add ${fb_startup_name} default

		    # Suse specific
    		elif [ -x /sbin/insserv ]
	    	then
    	    	/sbin/insserv /etc/init.d/${fb_startup_name}

			# One more way to register service - used in Debian
    		elif [ -x /usr/sbin/update-rc.d ]
	    	then
		    	/usr/sbin/update-rc.d -f ${fb_startup_name} remove
			    /usr/sbin/update-rc.d -f ${fb_startup_name} defaults
		    fi

	    	# More SuSE - rc.config fillup
		    if [ -f /etc/rc.config ]
    		then
      		if [ -x /bin/fillup ]
	        	then
	    	      /bin/fillup -q -d = /etc/rc.config ${fb_install_prefix}/misc/rc.config.firebird
    	    	fi
	    	elif [ -d /etc/sysconfig ]
	    	then
    	    	cp ${fb_install_prefix}/misc/rc.config.firebird /etc/sysconfig/firebird
	    	fi
	    fi

	else
		echo "Couldn't autodetect linux type. You must select"
		echo "the most appropriate startup script in ${fb_install_prefix}/misc"
		echo "and manually register it in your OS."
	fi
}


#------------------------------------------------------------------------
#  start init.d service

startService() {
	# systemd case
	startSuperServer && return 0

    InitFile=`getInitScriptLocation`
    if [ -f "$InitFile" ]; then
		"$InitFile" start

		checkString=`grepProcess "firebird"`
		if [ -z "$checkString" ]
		then
			# server didn't start - wait a bit and recheck
			sleep 2
			"$InitFile" start

			checkString=`grepProcess "firebird"`
			if [ -z "$checkString" ]
			then
				echo "Looks like standalone server failed to start"
				echo "Trying to continue anyway..."
			fi
		fi
	fi
}


#------------------------------------------------------------------------
# If we have right systems remove the service autostart stuff.

removeServiceAutostart() {
	if standaloneServerInstalled
	then
		# Must try to stop server cause it can be started manually when -path is used

		# systemd case
		unregisterSuperServer && return 0

		# Unregister using OS command
		if [ -x /sbin/insserv ]; then
			/sbin/insserv /etc/init.d/
		fi

		if [ -x /sbin/chkconfig ]; then
			/sbin/chkconfig --del ${fb_startup_name}
		fi

		if [ -x /sbin/rc-update ]; then
			/sbin/rc-update del ${fb_startup_name}
		fi

		# Remove /usr/sbin/rcfirebird symlink
		if [ -e /usr/sbin/rc${fb_startup_name} ]
		then
			rm -f /usr/sbin/rc${fb_startup_name}
		fi

		# Remove initd script
		rm -f $InitFile
	fi
}


#------------------------------------------------------------------------
# Returns TRUE if SA server is installed

standaloneServerInstalled() {
	if systemdPresent; then
		${SYSTEMCTL} --quiet is-enabled ${CTRL} && return 0
		return 1
	fi

	InitFile=`getInitScriptLocation`
	if [ -f "$InitFile" ]; then
		return 0
	fi
	return 1
}


#------------------------------------------------------------------------
# Corrects build-time "libdir" value

CorrectLibDir() {
	ld=${1}

	case $(uname -m) in
		i686)
			LDL=ld-linux.so
			;;
		x86_64)
			LDL=ld-linux-x86-64.so
			;;
		aarch64)
			LDL=ld-linux-aarch64.so
			;;
		*)
			echo $ld
			return
			;;
	esac

	dirname $(ldconfig -p | grep $LDL | awk -F '>' '{print $2;}') | head -n 1
}


#------------------------------------------------------------------------
# Validate library name

checkLibName() {
	grepFlag=-v
	[ "x64" = "x64" ] && grepFlag=
	ldconfig -p | grep -w "${1}" | grep $grepFlag 'x86-64'
}


#------------------------------------------------------------------------
# Checks for presence of libName in OS

haveLibrary() {
	libName=${1}
	[ -z "$libName" ] && return 1

	fixTommath=fixTomMath

	checkLibName "$libName" >/dev/null 2>/dev/null && return 0

	checkLibName "lib$libName" >/dev/null 2>/dev/null

	return $?
}


#------------------------------------------------------------------------
# Fix .so version of libtommath

fixTomMath() {
	[ -z "$LIBTOMMATH" ] && return
	if [ "$LIBTOMMATH" = "libtommath.so.1" ]
	then
		checklib=libtommath.so.0
	else
		checklib=libtommath.so.1
	fi

	tm1=`checkLibName $checklib | awk '{print $4}'`
	[ -z "$tm1" ] && return
	tm0=`dirname $tm1`/$LIBTOMMATH
	[ -e "$tm0" ] && return
	ln -s $tm1 $tm0
}


#------------------------------------------------------------------------
# refresh cache of dynamic loader after add/delete files in system libdir

reconfigDynamicLoader() {
	ldconfig
}
