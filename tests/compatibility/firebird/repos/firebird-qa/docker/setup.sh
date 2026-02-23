#!/bin/bash
set -e
shopt -s extglob

mkdir /tmp/firebird-installer
cd /tmp/firebird-installer
tar xzvf /firebird-installer.tar.gz --strip=1
./install.sh -silent
echo

/etc/init.d/firebird stop
echo

echo Changing SYSDBA password
echo "alter user sysdba password 'masterkey';" | /opt/firebird/bin/isql -q employee

echo Configuring

cat <<EOF > /opt/firebird/firebird.conf
BugcheckAbort = 1
ExternalFileAccess = Full
AuthServer = Srp, Win_Sspi, Legacy_Auth
UserManager = Srp, Legacy_UserManager
ReadConsistency = 0
WireCrypt = Enabled
ExtConnPoolSize = 10
ExtConnPoolLifeTime = 10
UseFileSystemCache = true
IpcName = xnet_fb4x_qa
DefaultDbCachePages = 2048
MaxUnflushedWrites = -1
MaxUnflushedWriteTime = -1
StatementTimeout = 7200
KeyHolderPlugin = fbSampleKeyHolder
RemoteServicePort = 3050
ParallelWorkers = 1
MaxParallelWorkers = 8
EOF

if test -f /opt/firebird/examples/prebuilt/plugins/fbSampleDbCrypt.conf; then
	cp /opt/firebird/examples/prebuilt/plugins/fbSampleDbCrypt.conf /opt/firebird/plugins/
	cp /opt/firebird/examples/prebuilt/plugins/libfbSample*.so /opt/firebird/plugins/

	cat <<EOF > /opt/firebird/plugins/fbSampleKeyHolder.conf
Auto = true
KeyRed=111
KeyGreen = 119
EOF
fi

cat <<EOF > /opt/firebird/databases.conf
employee.fdb = \$(dir_sampleDb)/employee.fdb
employee = \$(dir_sampleDb)/employee.fdb

security.db = \$(dir_secDb)/$(basename $(ls /opt/firebird/security*.fdb))
{
	RemoteAccess = true
	DefaultDbCachePages = 256
}
EOF

cat /qa/files/qa-databases.conf >> /opt/firebird/databases.conf

/etc/init.d/firebird start
echo

rm -r /tmp/firebird-installer

mkdir /opt/firebird/examples/empbuild/qa
chmod -R 777 /opt/firebird/examples/empbuild

cp -rn /qa/!(out) /qa-run/
