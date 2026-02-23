#!/bin/sh
set -e

sudo /qa/docker/setup.sh

cd /qa-run

pytest \
	--server local \
	--save-output \
	--extend-xml \
	--junitxml /qa-out/junit.xml \
	--disable-warnings \
	-vv \
	--tb=long \
	--basetemp=/tmp/pytest-tmp \
	--md-report \
	--md-report-flavor gfm \
	--md-report-verbose 1 \
	--md-report-color never \
	--md-report-output /qa-out/md_report.md \
	--ignore=tests/functional/replication \
	--ignore=tests/functional/basic/isql/test_08.py \
	-m "not replication and not encryption" \
	"$@"
