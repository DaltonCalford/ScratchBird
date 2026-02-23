#!/bin/sh
set -e

# Syntax:
# Run this script passing environment variables:
# FBQA_OUT=/path/to/out FBQA_INSTALLER=/path/to/Firebird.tar.gz ./run-docker.sh

THIS_DIR=`readlink -f $0`
THIS_DIR=`dirname $THIS_DIR`

if [ -z "$FBQA_OUT" ]; then
	echo \$FBQA_OUT is not defined.
fi

if [ -z "$FBQA_INSTALLER" ]; then
	echo \$FBQA_INSTALLER is not defined.
fi

if [ -z "$FBQA_OUT" ] || [ -z "$FBQA_INSTALLER" ]; then
	exit 1
fi

docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) --progress=plain -t firebird-qa $THIS_DIR
mkdir -p $FBQA_OUT/tests
docker run --user $(id -u) --rm -v `realpath $FBQA_OUT`:/qa-out -v `realpath $THIS_DIR`:/qa -v `realpath $FBQA_INSTALLER`:/firebird-installer.tar.gz firebird-qa "$@"
