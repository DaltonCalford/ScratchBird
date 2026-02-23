# SPDX-FileCopyrightText: 2021-present The Firebird Projects <www.firebirdsql.org>
#
# SPDX-License-Identifier: MIT
#
# PROGRAM/MODULE: firebird-qa
# FILE:           firebird/qa/plugin.py
# DESCRIPTION:    pytest plugin for Firebird QA
# CREATED:        9.4.2021
#
# The contents of this file are subject to the MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# Copyright (c) 2020 Firebird Project (www.firebirdsql.org)
# All Rights Reserved.
#
# Contributor(s): Pavel Císař (original code)
#                 ______________________________________

"""firebird-qa - pytest plugin for Firebird QA


"""

from __future__ import annotations
from typing import List, Dict, Union, Optional, Tuple, Sequence, Set
import sys
import locale
import os
import re
import shutil
import platform
import weakref
import pytest
from configparser import ConfigParser, ExtendedInterpolation
from difflib import ndiff
from collections import deque
from _pytest.config import Config
from _pytest.terminal import TerminalReporter, _get_raw_skip_reason, _format_trimmed
from _pytest.pathlib import bestrelpath
from subprocess import run, CompletedProcess, PIPE, STDOUT
from pathlib import Path
from configparser import ConfigParser, ExtendedInterpolation
from packaging.specifiers import SpecifierSet
from packaging.version import parse
import time
from datetime import datetime
from dataclasses import dataclass
from threading import Thread, Barrier, Event
from firebird.driver import connect, connect_server, create_database, driver_config, \
     NetProtocol, Server, CHARSET_MAP, Connection, Cursor, \
     DESCRIPTION_NAME, DESCRIPTION_DISPLAY_SIZE, DatabaseConfig, DBKeyScope, DbInfoCode, \
     DbWriteMode, get_api, Error, TIMEOUT
from firebird.driver.core import _connect_helper

Substitutions = List[Tuple[str, str]]

_vars_ = {'server': None,
          'bin-dir': None,
          'firebird-config': None,
          'runslow': False,
          }

_platform = platform.system()
_nodemap = {}

#: Configuration for tests
QA_GLOBALS: ConfigParser = ConfigParser(interpolation=ExtendedInterpolation())

FIELD_ID = 'ID:'
FIELD_ISSUE = 'ISSUE:'
FIELD_TITLE = 'TITLE:'
FIELD_DECRIPTION = 'DESCRIPTION:'
FIELD_NOTES = 'NOTES:'
FIELD_JIRA = 'JIRA:'
FIELD_FBTEST = 'FBTEST:'

SKIP_VERSION = 'version'
SKIP_PLATFORM = 'platform'
SKIP_ANY = 'any'

@pytest.fixture(scope='session', autouse=True)
def log_session_context(record_testsuite_property):
    """Autoused session fixture that records `version`,`architecture` and `mode`
    XML propeties.
    """
    if _vars_['extend-xml']:
        record_testsuite_property('version', _vars_['version'])
        record_testsuite_property('architecture', _vars_['arch'])
        record_testsuite_property('mode', _vars_['server-arch'])

class DbCache:
    """Cache for empty databases.
    """
    def __init__(self):
        self.cache: Path = _vars_['dbcache']
        self.databases: Dict[str, Path] = {}
        for db in self.cache.glob('**/*.fdb'):
            self.databases[db.stem] = db
    def get_db(self, page_size: Optional[int]=None, sql_dialect: Optional[int]=None,
               charset: Optional[str] = None) -> Optional[Path]:
        """Returns path to cached database or None if database is not in cache.

        Arguments:
          page_size: Database page size.
          sql_dialect: Database SQL dialect.
          charset: Database character set.
        """
        return self.databases.get(f"db-{_vars_['ods'][0]}.{_vars_['ods'][1]}-{page_size}-{sql_dialect}-{charset}")
    def store_db(self, src_path: Path, page_size: Optional[int]=None,
                 sql_dialect: Optional[int]=None, charset: Optional[str] = None) -> None:
        """Copy database to cache.

        Arguments:
          page_size: Database page size.
          sql_dialect: Database SQL dialect.
          charset: Database character set.
        """
        db_name = f"db-{_vars_['ods'][0]}.{_vars_['ods'][1]}-{page_size}-{sql_dialect}-{charset}.fdb"
        db_path = self.cache / db_name
        shutil.copyfile(src_path, db_path)
        # Fix permissions
        if platform.system != 'Windows':
            os.chmod(db_path, 33206)
        self.databases[db_name] = db_path

@pytest.fixture(scope='session', autouse=True)
def db_cache() -> DbCache:
    """Database cache for empty databases.
    """
    return DbCache()

class ExecutionError(Exception):
    """Exception used to indicate errors when external QA tools (like isql, gstat etc.) are executed.
    """
    pass

def pytest_addoption(parser, pluginmanager):
    """Adds plugin-specific pytest command-line options.

    .. seealso:: `pytest documentation <_pytest.hookspec.pytest_addoption>` for details.
    """
    grp = parser.getgroup('firebird', "Firebird QA", 'general')
    grp.addoption('--server', help="Server configuration name", default='', required=True)
    grp.addoption('--driver-config', help="Firebird driver configuration filename", default='firebird-driver.conf')
    grp.addoption('--bin-dir', metavar='PATH', help="Path to directory with Firebird utilities")
    grp.addoption('--protocol',
                  choices=[i.name.lower() for i in NetProtocol],
                  help="Network protocol used for database attachments")
    grp.addoption('--disable-db-cache', action='store_true', default=False,
                  help="Disable cache for empty databases")
    grp.addoption('--runslow', action='store_true', default=False, help="Run slow tests")
    grp.addoption('--save-output', action='store_true', default=False, help="Save test std[out|err] output to files")
    grp.addoption('--skip-deselected', choices=[SKIP_PLATFORM, SKIP_VERSION, SKIP_ANY],
                  help="SKIP tests instead deselection")
    grp.addoption('--extend-xml', action='store_true', default=False, help="Extend XML JUnit report with additional information")
    grp.addoption('--install-terminal', action='store_true', default=False, help="Use our own terminal reporter")
    grp.addoption('--start-time', action='store_true', dest="start_time_info", default=False, help="Show tests start time info")

def pytest_report_header(config):
    """Returns plugin-specific test session header.

    .. seealso:: `pytest documentation <_pytest.hookspec.pytest_report_header>` for details.
    """
    return ["System:",
            f"  encodings: sys:{sys.getdefaultencoding()} locale:{locale.getpreferredencoding()} filesystem:{sys.getfilesystemencoding()}",
            "Firebird:",
            f"  configuration: {_vars_['driver-config']}",
            f"  ODS: {_vars_['ods'][0]}.{_vars_['ods'][1]}",
            f"  server: {_vars_['server']} [v{_vars_['version']}, {_vars_['server-arch']}, {_vars_['arch']}]",
            f"  home: {_vars_['home-dir']}",
            f"  bin: {_vars_['bin-dir']}",
            f"  client library: {_vars_['fbclient']}",
            ]

def set_tool(tool: str):
    "Helper function for `pytest_configure`."
    path: Path = _vars_['bin-dir'] / tool
    if not path.is_file():
        path = path.with_suffix('.exe')
        if not path.is_file():
            pytest.exit(f"Can't find '{tool}' in {_vars_['bin-dir']}")
    _vars_[tool] = path

def remove_dir(path: Path) -> None:
    if path.exists():
        for item in path.iterdir():
            if item.is_file():
                item.unlink()
            elif item.is_dir():
                remove_dir(item)
        path.rmdir()

class QATerminalReporter(TerminalReporter):
    """Custom `TerminalReporter`
    Prints tests start time if '--start-time' option is specified.
    Prints our QA test IDs instead pytest node IDs if '--install-terminal' option is specified.

    The code was directly taken from pytest source and adapted to our needs.
    """
    def __init__(self, config, install_terminals, start_time_info):
        super(QATerminalReporter, self).__init__(config)
        self.install_terminals = install_terminals
        self.start_time_info = start_time_info
    def _getfailureheadline(self, rep):
        head_line = rep.head_line
        if head_line:
            return rep.nodeid
        return "test session"  # XXX?
    def pytest_runtest_logstart(self, nodeid: str, location: Tuple[str, Optional[int], str]) -> None:
        # Ensure that the path is printed before the
        # 1st test of a module starts running.
        nodeid = _nodemap.get(nodeid, nodeid) if self.install_terminals else nodeid
        if self.showlongtestinfo:
            line = nodeid + ' '
            self.write_ensure_prefix(line, "")
            self.flush()
        elif self.showfspath:
            self.write_fspath_result(nodeid, "")
            self.flush()
    def pytest_runtest_logreport(self, report: pytest.TestReport) -> None:
        self._tests_ran = True
        rep = report
        res: Tuple[str, str, Union[str, Tuple[str, Mapping[str, bool]]]] = \
            self.config.hook.pytest_report_teststatus(report=rep, config=self.config)
        category, letter, word = res
        if not isinstance(word, tuple):
            markup = None
        else:
            word, markup = word
        self._add_stats(category, [rep])
        if not letter and not word:
            # Probably passed setup/teardown.
            return
        running_xdist = hasattr(rep, "node")
        if markup is None:
            was_xfail = hasattr(report, "wasxfail")
            if rep.passed and not was_xfail:
                markup = {"green": True}
            elif rep.passed and was_xfail:
                markup = {"yellow": True}
            elif rep.failed:
                markup = {"red": True}
            elif rep.skipped:
                markup = {"yellow": True}
            else:
                markup = {}
        if self.verbosity <= 0:
            self._tw.write(letter, **markup)
        else:
            self._progress_nodeids_reported.add(rep.nodeid)
            line = (rep._qa_id_ + ' ') if self.install_terminals else self._locationline(rep.nodeid, *rep.location)
            if not running_xdist:
                self.write_ensure_prefix(line, word, **markup)

                if self._show_progress_info == "count":
                    num_tests = self._session.testscollected
                    progress_length = len(" [{}/{}]".format(str(num_tests), str(num_tests)))
                else:
                    progress_length = len(" [100%]")

                if rep.skipped or hasattr(report, "wasxfail"):
                    available_width = (
                        (self._tw.fullwidth - self._tw.width_of_current_line)
                        - progress_length
                        - 1
                    )
                    reason = _get_raw_skip_reason(rep)
                    reason_ = _format_trimmed(" ({})", reason, available_width)
                    if reason and reason_ is not None:
                        self._tw.write(reason_)
                if self._show_progress_info:
                    self._write_progress_information_filling_space()
            else:
                self.ensure_newline()
                self._tw.write("[%s]" % rep.node.gateway.id)
                if self._show_progress_info:
                    self._tw.write(
                        self._get_progress_information_message() + " ", cyan=True
                    )
                else:
                    self._tw.write(" ")
                self._tw.write(word, **markup)
                self._tw.write(" " + line)
                self.currentfspath = -2
        self.flush()

    def write_ensure_prefix(self, prefix: str, extra: str = "", **kwargs) -> None:
        start_time = (datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3] + ' ') if self.start_time_info else ''
        if self.currentfspath != prefix:
            self._tw.line()
            self.currentfspath = prefix
            self._tw.write(f"{start_time}{prefix}")
        if extra:
            self._tw.write(extra, **kwargs)
            self.currentfspath = -2

    def write_fspath_result(self, nodeid: str, res, **markup: bool) -> None:
        start_time = (datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3] + ' ') if self.start_time_info else ''
        fspath = self.config.rootpath / nodeid.split("::")[0]
        if self.currentfspath is None or fspath != self.currentfspath:
            if self.currentfspath is not None and self._show_progress_info:
                self._write_progress_information_filling_space()
            self.currentfspath = fspath
            relfspath = bestrelpath(self.startpath, fspath)
            self._tw.line()
            self._tw.write(start_time + relfspath + " ")
        self._tw.write(res, flush=True, **markup)

@pytest.mark.tryfirst
def pytest_runtest_makereport(item, call):
    """Our replacement for pytest hook that is called to create a `TestReport` for each of
    the setup, call and teardown runtest phases of a test item.

    Our version makes a copy of all `_qa_*` attributes from item to returned report instance.

    .. seealso:: `pytest documentation <_pytest.hookspec.pytest_runtest_makereport>` for details.
    """
    result = pytest.TestReport.from_item_and_call(item, call)
    for attr in dir(item):
        if attr.startswith('_qa_'):
            setattr(result, attr, getattr(item, attr))
    return result

@pytest.mark.trylast
def pytest_configure(config):
    """Plugin configuration.

    Whta it does:

    - define `version`, `platform` and `slow` markers.
    - sets values for our globals (accessible as `Action.vars`)
    - if server section in driver-config contains client library specification,
      it's copied to global driver config.
    - installs our `QATerminalReporter`

    .. seealso:: `pytest documentation <_pytest.hookspec.pytest_configure>` for details.
    """
    # pytest.ini
    config.addinivalue_line("markers", "version(versions): Firebird version specifications")
    config.addinivalue_line("markers", "platform(platforms): Platform names")
    config.addinivalue_line("markers", "slow: Mark test as slow to run")
    config.addinivalue_line("markers", "encryption: Mark test as requiring the encryption plugin")
    if config.getoption('help'):
        return
    config_path: Path = Path.cwd() / config.getoption('driver_config')
    if config_path.is_file():
        driver_config.read(str(config_path))
        _vars_['firebird-config'] = config_path
    driver_config.register_database('pytest')
    #
    _vars_['driver-config'] = config.getoption('driver_config')
    _vars_['dbcache-disabled'] = config.getoption('disable_db_cache')
    _vars_['basetemp'] = config.getoption('basetemp')
    _vars_['runslow'] = config.getoption('runslow')
    _vars_['root'] = config.rootpath
    path: Path = config.rootpath / 'dbcache'
    path.mkdir(exist_ok=True)
    if platform.system != 'Windows':
        path.chmod(16895)
    _vars_['dbcache'] = path if path.is_dir() else config.rootpath
    path = config.rootpath / 'databases'
    _vars_['databases'] = path if path.is_dir() else config.rootpath
    path = config.rootpath / 'backups'
    _vars_['backups'] = path if path.is_dir() else config.rootpath
    path = config.rootpath / 'files'
    _vars_['files'] = path if path.is_dir() else config.rootpath
    _vars_['server'] = config.getoption('server')
    protocol = config.getoption('protocol')
    _vars_['protocol'] = NetProtocol._member_map_[protocol.upper()] if protocol else protocol
    _vars_['save-output'] = config.getoption('save_output')
    _vars_['skip-deselected'] = config.getoption('skip_deselected')
    _vars_['extend-xml'] = config.getoption('extend_xml')
    srv_conf = driver_config.get_server(_vars_['server'])
    _vars_['host'] = srv_conf.host.value if srv_conf is not None else ''
    _vars_['port'] = srv_conf.port.value if srv_conf is not None else ''
    _vars_['user'] = srv_conf.user.value
    _vars_['password'] = srv_conf.password.value
    #
    db_conf = driver_config.register_database('employee')
    db_conf.server.value = _vars_['server']
    db_conf.database.value = 'employee.fdb'
    # Handle server-specific "fb_client_library" configuration option
    _vars_['fbclient'] = 'UNKNOWN'
    cfg = ConfigParser(interpolation=ExtendedInterpolation())
    cfg.read(str(config_path))
    if cfg.has_option(_vars_['server'], 'fb_client_library'):
        fbclient = Path(cfg.get(_vars_['server'], 'fb_client_library'))
        if not fbclient.is_file():
            pytest.exit(f"Client library '{fbclient}' not found!")
        driver_config.fb_client_library.value = str(fbclient)
    cfg.clear()
    _vars_['fbclient'] = get_api().client_library_name
    # THIS should load the driver API, do not connect db or server earlier!
    with connect_server(_vars_['server'], user='SYSDBA',
                        password=_vars_['password']) as srv:
        _vars_['version'] = parse(srv.info.version.replace('-dev', ''))
        _vars_['home-dir'] = Path(srv.info.home_directory)
        if bindir := config.getoption('bin_dir'):
            _vars_['bin-dir'] = Path(bindir)
        else:
            bindir = _vars_['home-dir'] / 'bin'
            if not bindir.exists():
                bindir = _vars_['home-dir']
            _vars_['bin-dir'] = bindir
        _vars_['lock-dir'] = Path(srv.info.lock_directory)
        _vars_['bin-dir'] = Path(bindir) if bindir else _vars_['home-dir']
        _vars_['security-db'] = Path(srv.info.security_database)
        _vars_['arch'] = srv.info.architecture
    # tools
    for tool in ['isql', 'gbak', 'nbackup', 'gstat', 'gfix', 'gsec', 'fbsvcmgr']:
        set_tool(tool)
    # Load test_config.ini
    QA_GLOBALS.read(_vars_['files'] / 'test_config.ini')
    # Driver encoding for NONE charset
    CHARSET_MAP['NONE'] = 'utf-8'
    CHARSET_MAP[None] = 'utf-8'
    # Server architecture [CS,SS,SC] and sample directory
    _vars_['sample_dir'] = None
    with connect('employee') as con1, connect('employee') as con2:
        _vars_['ods'] = (con1.info.ods_version, con1.info.ods_minor_version)
        db_path = Path(con1.info.name)
        _vars_['sample_dir'] = db_path.parent
        sql = f"""
        select count(distinct a.mon$server_pid), min(a.mon$remote_protocol),
        max(iif(a.mon$remote_protocol is null, 1, 0))
        from mon$attachments a
        where a.mon$attachment_id in ({con1.info.id}, {con2.info.id}) or upper(a.mon$user) = upper('cache writer')
        """
        cur1 = con1.cursor()
        cur1.execute(sql)
        server_cnt, server_pro, cache_wrtr = cur1.fetchone()
        if server_pro is None:
            result = 'Embedded'
        elif cache_wrtr == 1:
            result = 'SuperServer'
        elif server_cnt == 2:
            result = 'Classic'
        else:
            f1 = con1.info.get_info(DbInfoCode.FETCHES)
            cur2 = con2.cursor()
            cur2.execute('select 1 from rdb$database').fetchall()
            f2 = con1.info.get_info(DbInfoCode.FETCHES)
            result = 'SuperClassic' if f1 == f2 else 'SuperServer'
    _vars_['server-arch'] = result
    # Create QA subdirectory in samle db directory
    if _vars_['sample_dir'] is None:
        raise Exception("Cannot determine the 'sample_dir'")
    samle_qa = _vars_['sample_dir'] / 'qa'
    remove_dir(samle_qa)
    samle_qa.mkdir(exist_ok=True)
    if platform.system != 'Windows':
        samle_qa.chmod(16895)
    # Change terminal reporter
    standard_reporter = config.pluginmanager.getplugin('terminalreporter')
    pspec_reporter = QATerminalReporter(standard_reporter.config, config.getoption('install_terminal'), config.getoption('start_time_info'))
    config.pluginmanager.unregister(standard_reporter)
    config.pluginmanager.register(pspec_reporter, 'terminalreporter')

def pytest_collection_modifyitems(session, config, items):
    """Post-processing of collected tests. deselects or mark for skip tests that are not
    intended for currently tested Firebird version or platform.

    Also parses doctring from test function or module to extract our metadata, and sets them
    as `_qa_*` attributes on `Item <_pytest.nodes.Item>` instance.

    .. seealso:: `pytest documentation <_pytest.hookspec.pytest_collection_modifyitems>` for details.
    """
    _version = _vars_['version']
    skip_slow = pytest.mark.skip(reason="need --runslow option to run")
    # Apply skip markers
    for item in items:
        if 'slow' in item.keywords and not _vars_['runslow']:
            item.add_marker(skip_slow)
    #
    skip = _vars_['skip-deselected']
    selected = []
    deselected = []
    platform_skip = pytest.mark.skip(reason=f"Not for {_platform}")
    version_skip = pytest.mark.skip(reason=f"Not for {_version}")
    for item in items:
        platform_ok = True
        for platforms in [mark.args for mark in item.iter_markers(name="platform")]:
            platform_ok = _platform in platforms
        if not platform_ok and skip in [SKIP_PLATFORM, SKIP_ANY]:
            selected.append(item)
            item.add_marker(platform_skip)
            continue
        versions = [mark.args for mark in item.iter_markers(name="version")]
        if versions:
            spec = SpecifierSet(','.join(list(versions[0])))
            if platform_ok and _version in spec:
                selected.append(item)
            else:
                if skip in [SKIP_VERSION, SKIP_ANY]:
                    selected.append(item)
                    item.add_marker(version_skip)
                else:
                    deselected.append(item)
        elif platform_ok:
            selected.append(item)
    items[:] = selected
    config.hook.pytest_deselected(items=deselected)
    # Add OUR OWN test metadata to Item
    for item in items:
        item._qa_id_ = item.nodeid
        item._qa_issue_ = None
        item._qa_jira_ = None
        item._qa_pytest_ = None
        item._qa_title_ = ''
        item._qa_description_ = ''
        item._qa_notes_ = ''
        module_doc = item.parent.obj.__doc__
        if module_doc is None:
            continue
        current_field = 'NONE'
        for line in module_doc.splitlines():
            uline = line.upper()
            if uline.startswith(FIELD_ID):
                current_field = FIELD_ID
                item._qa_id_ = line[len(FIELD_ID):].strip()
                _nodemap[item.nodeid] = item._qa_id_
            elif uline.startswith(FIELD_FBTEST):
                current_field = FIELD_FBTEST
                item._qa_pytest_ = line[len(FIELD_FBTEST):].strip()
            elif uline.startswith(FIELD_ISSUE):
                current_field = FIELD_ISSUE
                item._qa_issue_ = line[len(FIELD_ISSUE):].strip()
            elif uline.startswith(FIELD_JIRA):
                current_field = FIELD_JIRA
                item._qa_jira_ = line[len(FIELD_JIRA):].strip()
            elif uline.startswith(FIELD_TITLE):
                current_field = FIELD_TITLE
                item._qa_title_ = line[len(FIELD_TITLE):].strip()
            elif uline.startswith(FIELD_DECRIPTION):
                current_field = FIELD_DECRIPTION
                item._qa_description_ = line[len(FIELD_DECRIPTION):]
            elif uline.startswith(FIELD_NOTES):
                current_field = FIELD_NOTES
                item._qa_notes_ = line[len(FIELD_NOTES):]
            elif current_field == FIELD_TITLE:
                if item._qa_title_:
                    item._qa_title_ += ' '
                item._qa_title_ += line.strip()
            elif current_field == FIELD_DECRIPTION:
                if item._qa_description_:
                    item._qa_description_ += '\n'
                item._qa_description_ += line.strip()
            elif current_field == FIELD_NOTES:
                if item._qa_notes_:
                    item._qa_notes_ += '\n'
                item._qa_notes_ += line.strip()
            else:
                # Unknown field
                pass
        # Add metadata to XML output
        if _vars_['extend-xml']:
            item.user_properties.append(("id", item._qa_id_))
            item.user_properties.append(("fbtest", item._qa_pytest_))
            item.user_properties.append(("issue", item._qa_issue_))
            item.user_properties.append(("title", item._qa_title_))
            item.user_properties.append(("description", item._qa_description_))
            item.user_properties.append(("notes", item._qa_notes_))

def pytest_assertrepr_compare(config: Config, op: str, left: object, right: object) -> Optional[List[str]]:
    """Returns explanation for comparisons in failing assert expressions.

    If both objects are `str`, uses `difflib.ndiff` to provide explanation.
    """
    if isinstance(left, str) and isinstance(right, str) and op == "==":
        # 16.11.2023, pzotov: we have to put empty string at the beginning of each comparing lists.
        # Otherwise first diff will be at the same line as 'assert' phrase, which causes readability be poor.
        #
        left_lines = ['']
        left_lines.extend(left.splitlines())
        right_lines = ['']
        right_lines.extend(right.splitlines())

        # 16.11.2023, pzotov
        # ndiff output must be interpreted as following:
        #     * "E     - <some text>" ==> MISSED line (it was in EXPECTED text but absent in actual one).
        #     * "E     + <some_text>" ==> EXCESSIVE line (it is not in EXPECTED text but did appear in actual).
        # But for QA-purposes, this output must answer the question:
        #     "what must be changed in ACTUAL output so that it became equal to EXPECTED"
        #     (i.e. how to "REVERT" actual back to expected).
        # In order to see such result, we have to specify 'right_lines' to the 1st argument that is passed to ndiff().
        # ::: NB :::
        # We assume that all tests are written so that ACTUAL output is left side in 'assert' statement and EXPECTED
        # is right side, e.g: assert act.clean_stdout == act.clean_expected_stdout
        # This requirement is CRUCIAL if we use ndiff() instead of default pytest comparison method!
        #
        return list(ndiff(right_lines, left_lines))

    return None

def substitute_macros(text: str, macros: Dict[str, str]):
    """Helper function to substitute `$(name)` macros in text.

    .. important::

       Used only for backward compatibility with `fbtest`. The use of `$(name)` macros in
       tests is **deprecated**.
    """
    f_text = text
    for (pattern, replacement) in macros.items():
        replacement = replacement.replace(os.path.sep,'/')
        f_text = f_text.replace(f'$({pattern.upper()})', replacement)
    return f_text

class Database:
    """Object to access and manage single test database.

    Arguments:
        path: Path to directory where test database should be located.
        filename: Database filename, or alias prefixed by `#`.
        user: User name used to create/restore/connect the test database. Default is taken
            from server configuration.
        password: User password used to create/restore/connect the test database. Default
            is taken from server configuration.
        charset: Character set for test database, and also default charset for connections.
        config_name: Name for database configuration.

    .. important::

        Do not create instances of this class directly! Use **only** fixtures created by `db_factory`.
    """
    def __init__(self, path: Path, filename: str, user: Optional[str]=None,
                 password: Optional[str]=None, charset: Optional[str]=None, debug: str='',
                 config_name: str='pytest', utf8filename: bool=False):
        #: firebird-driver database configuration name
        self.config_name: str = config_name
        if driver_config.get_database(config_name) is None:
            driver_config.register_database(config_name)
        self._debug: str = debug
        #: Use utf8_filename DPB flag
        self.utf8filename = utf8filename
        #: Full path to test database, or database alias
        self.db_path: Union[Path, str] = filename[1:] if filename.startswith('#') else path / filename
        #: DSN to test database.
        self.dsn: str = None
        #: Firebird CHARACTER SET name for the database
        self.charset: str = 'NONE' if charset is None else charset.upper()
        if _vars_['host']:
            self.dsn = f"{_vars_['host']}:{str(self.db_path)}"
        else:
            self.dsn = str(self.db_path)
        #: Substitutions for `$(name)` macros. Do not use them directly, as use of
        #: `$(name)` macros in tests is **deprecated**, and supported only for backward
        #: compatibility with `fbtest`.
        self.subs = {'temp_directory': str(path / 'x')[:-1],
                     'database_location': str(path / 'x')[:-1],
                     'DATABASE_PATH': str(path / 'x')[:-1],
                     'DSN': self.dsn,
                     'files_location': str(_vars_['root'] / 'files'),
                     'backup_location': str(_vars_['root'] / 'backups'),
                     'suite_database_location': str(_vars_['root'] / 'databases'),
                     }
        srv_conf = driver_config.get_server(_vars_['server'])
        #: User name
        self.user: Optional[str] = srv_conf.user.value if user is None else user
        #: User password
        self.password: Optional[str] = srv_conf.password.value if password is None else password
    def _make_config(self, *, page_size: Optional[int]=None, sql_dialect: Optional[int]=None,
                     charset: Optional[str]=None, user: Optional[str]=None,
                     password: Optional[str]=None) -> None:
        """Helper method that sets `firebird-driver`_ configuration for `pytest` database
        to work with this particular test database instance. Used in methods that need to
        call `~firebird.driver.core.connect` or `~firebird.driver.core.create_database`.
        """
        db_conf = self.get_config()
        db_conf.clear()
        db_conf.server.value = _vars_['server']
        db_conf.database.value = str(self.db_path)
        db_conf.user.value = self.user if user is None else user
        db_conf.password.value = self.password if password is None else password
        db_conf.utf8filename.value = self.utf8filename
        if sql_dialect is not None:
            db_conf.db_sql_dialect.value = sql_dialect
            db_conf.sql_dialect.value = sql_dialect
        if page_size is not None:
            db_conf.page_size.value = page_size
        if charset is not None:
            db_conf.db_charset.value = charset
            db_conf.charset.value = charset
        if _vars_['protocol'] is not None:
            db_conf.protocol.value = _vars_['protocol']
    def get_config(self) -> DatabaseConfig:
        """Returns `firebird-driver`_ configuration for test database.
        """
        return driver_config.get_database(self.config_name)
    def create(self, page_size: Optional[int]=None, sql_dialect: Optional[int]=None,
               cache: DbCache=None) -> None:
        """Create the test database.

        Arguments:
            page_size: Database page size.
            sql_dialect: Database SQL dialect.

        .. note::

           Typically, this method is used only by `db_factory` to create the test database
           (during fixture setup) unless `do_not_create` argument is set to True.

        """
        __tracebackhide__ = True
        # Path means database file, str means alias
        use_cache: bool = isinstance(self.db_path, Path) and cache is not None and not _vars_['dbcache-disabled']
        src_path: Path = None
        if use_cache:
            src_path = cache.get_db(page_size=page_size, sql_dialect=sql_dialect, charset=self.charset)
        if src_path:
            charset = self.charset
            print(f"Cached db: {src_path.name} [{page_size=}, {sql_dialect=}, {charset=}")
            shutil.copyfile(src_path, self.db_path)
            # Fix permissions
            if platform.system != 'Windows':
                os.chmod(self.db_path, 33206)
        else:
            self._make_config(page_size=page_size, sql_dialect=sql_dialect, charset=self.charset)
            charset = self.charset
            print(f"Creating db: {self.dsn} [{page_size=}, {sql_dialect=}, {charset=}, user={self.user}, password={self.password}]")
            with create_database(self.config_name):
                pass
            if use_cache:
                cache.store_db(self.db_path, page_size=page_size, sql_dialect=sql_dialect,
                               charset=self.charset)

    def restore(self, backup: str) -> None:
        """Create the test database from backup.

        Arguments:
            backup: Backup filename.

        .. note::

           Typically, this method is used only by `db_factory` to create the test database
           (during fixture setup) unless `do_not_create` argument is set to True.

        """
        __tracebackhide__ = True
        fbk_file: Path = _vars_['backups'] / backup
        if not fbk_file.is_file():
            raise ValueError(f"Backup file '{fbk_file}' not found")
        print(f"Restoring db: {self.db_path} from {fbk_file}")
        result = run([_vars_['gbak'], '-r', '-v', '-user', self.user,
                      '-password', self.password,
                      str(fbk_file), str(self.dsn)], capture_output=True)
        if result.returncode:
            print(f"-- stdout {'-' * 20}")
            print(result.stdout)
            print(f"-- stderr {'-' * 20}")
            print(result.stderr)
            raise Exception("Database restore failed")
        return result
    def copy(self, filename: str) -> None:
        """Create the test database as copy of template database.

        Arguments:
            filename: Template database filename.

        .. note::

           Typically, this method is used only by `db_factory` to create the test database
           (during fixture setup) unless `do_not_create` argument is set to True.

        """
        __tracebackhide__ = True
        src_path = _vars_['databases'] / filename
        print(f"Copying db: {self.db_path} from {src_path}")
        shutil.copyfile(src_path, self.db_path)
        # Fix permissions
        if platform.system != 'Windows':
            os.chmod(self.db_path, 16895)
    def init(self, script: str) -> CompletedProcess:
        """Initialize the test database by running ISQL script.

        Arguments:
            script: SQL script content to be executed by ISQL.

        .. note::

           Typically, this method is used only by `db_factory` to initialize the test database
           (during fixture setup) unless `do_not_create` argument is set to True.

        """
        __tracebackhide__ = True
        params = [_vars_['isql'], '-user', self.user, '-password', self.password]
        if self.charset != 'NONE':
            params.extend(['-ch', self.charset])
            io_enc = CHARSET_MAP[self.charset]
        else:
            params.extend(['-ch', 'utf8'])
            io_enc = 'utf8'
        params.append(str(self.dsn))
        result = run(params, input=substitute_macros(script, self.subs), encoding=io_enc,
                     capture_output=True)
        if result.returncode:
            print(f"-- stdout {'-' * 20}")
            print(result.stdout)
            print(f"-- stderr {'-' * 20}")
            print(result.stderr)
            raise Error("Database init script execution failed", stderr=result.stderr)
        return result
    def drop(self) -> None:
        """Drop the test database.

        .. note::

           Typically, this method is used only by `db_factory` to drop the test database
           after test execution (during fixture teardown) unless `do_not_drop` argument is
           set to True.

        """
        __tracebackhide__ = True
        with connect_server(_vars_['server'], encoding='utf8' if self.utf8filename else None) as srv:
            srv.database.no_linger(database=self.db_path)
        self._make_config()
        with connect(self.config_name) as db:
            db._att._name = self._debug
            try:
                db.execute_immediate('delete from mon$attachments where mon$attachment_id != current_connection')
                db.commit()
            except:
                pass
            #print(f"Removing db: {self.db_path}")
            try:
                db.drop_database()
            except:
                pass
        if isinstance(self.db_path, Path) and self.db_path.is_file():
            self.db_path.unlink(missing_ok=True)
    def connect(self, *, user: Optional[str]=None, password: Optional[str]=None,
                role: Optional[str]=None, no_gc: Optional[bool]=None,
                no_db_triggers: Optional[bool]=None, dbkey_scope: Optional[DBKeyScope]=None,
                session_time_zone: Optional[str]=None, charset: Optional[str]=None,
                sql_dialect: Optional[int]=None, auth_plugin_list: Optional[str]=None) -> Connection:
        """Create new connection to test database.

        Arguments:
            user: User name for this connection.
            password: User password for this connection.
            role: Role name.
            no_gc: When `True`, the Garbage Collection is disabled for this connection.
            no_db_triggers: When `True`, the database triggers are disabled for this connection.
            dbkey_scope: Scope for `db_key's` for this connection.
            session_time_zone: Session time zone.
            charset: Character set for this connection.
            sql_dialect: SQL dialect used for connection.
            auth_plugin_list: List of authentication plugins override.

        .. tip::

           It's highly recommended to use the :ref:`with <with>` statement to ensure that
           the returned `~firebird.driver.core.Connection` is properly closed even if test
           fails or raises an error. Otherwise the teardown of test database fixture may
           fail as well, adding unnecessary clutter to test session report.
        """
        __tracebackhide__ = True
        self._make_config(user=user, password=password, charset=charset, sql_dialect=sql_dialect)
        result = connect(self.config_name, role=role, no_gc=no_gc, no_db_triggers=no_db_triggers,
                         dbkey_scope=dbkey_scope, session_time_zone=session_time_zone,
                         auth_plugin_list=auth_plugin_list)
        result._att._name = self._debug
        return result
    def set_async_write(self) -> None:
        "Set the database to `async write` mode."
        __tracebackhide__ = True
        with connect_server(_vars_['server'], encoding='utf8' if self.utf8filename else None) as srv:
            srv.database.set_write_mode(database=self.db_path, mode=DbWriteMode.ASYNC)
    def set_sync_write(self) -> None:
        "Set the database to `sync write` mode."
        __tracebackhide__ = True
        with connect_server(_vars_['server'], encoding='utf8' if self.utf8filename else None) as srv:
            srv.database.set_write_mode(database=self.db_path, mode=DbWriteMode.SYNC)

def existing_db_factory(*, filename: str='test.fdb', charset: Optional[str]=None,
                        user: Optional[str]=None, password: Optional[str]=None,
                        config_name: str='pytest', utf8filename: bool=False):
    """Factory function that returns :doc:`fixture <pytest:explanation/fixtures>` providing
    the `Database` instance to existing database.

    Arguments:
        filename: Test database filename. It's also possible to specify database alias using
            '#' as prefix, for example `#employee` means alias `employee`.
            The database with this alias must be defined in `databases.conf`.
        charset: Default charset for connections.
        user: User name used to connect the test database. Default is taken from server configuration.
        password: User password used to connect the test database. Default
            is taken from server configuration.
        config_name: Name for database configuration.
        utf8filename: Use utf8filename DPB flag.

    .. note::

       The returned instance must be assigned to module-level variable. Name of this variable
       is important, as it's used to reference the fixture in other fixture-factory functions
       that use the database, and the test function itself.
    """

    @pytest.fixture
    def existing_database_fixture(request: pytest.FixtureRequest) -> Database:
        db = Database(_vars_['databases'], filename, user, password, charset, debug=str(request.module),
                      config_name=config_name, utf8filename=utf8filename)
        yield db

    return existing_database_fixture

def db_factory(*, filename: str='test.fdb', init: Optional[str]=None,
               from_backup: Optional[str]=None, copy_of: Optional[str]=None,
               page_size: Optional[int]=None, sql_dialect: Optional[int]=None,
               charset: Optional[str]=None, user: Optional[str]=None,
               password: Optional[str]=None, do_not_create: bool=False,
               do_not_drop: bool=False, async_write: bool=True,
               config_name: str='pytest', utf8filename: bool=False):
    """Factory function that returns :doc:`fixture <pytest:explanation/fixtures>` providing
    the `Database` instance.

    Arguments:
        filename: Test database filename (without path). It's also possible to specify
            database alias using '#' as prefix, for example `#employee` means alias `employee`.
            The database with this alias must be defined in `databases.conf`.
        init:     Test database initialization script (isql format).
        from_backup: Backup filename (without path) from which the test database should be restored.
            File must be located in `backups` directory.
        copy_of:  Filename (without path) of the database that should be copied as test database.
            File must be located in `databases` directory.
        page_size: Test database page size.
        sql_dialect: SQL dialect for test database.
        charset: Character set for test database (and default charset for connections).
        user: User name used to create/restore/connect the test database. Default is taken
            from server configuration.
        password: User password used to create/restore/connect the test database. Default
            is taken from server configuration.
        do_not_create: When `True`, the preparation of test database is skipped. Use this
            option to define test databases created by test itself, so they are properly
            removed.
        do_not_drop: When `True`, the ficture will not drop the test database. Use this
           option when test database is removed by test itself (as part of test routine).
        async_write: When `True` [default], the database is set to async write before initialization.
        config_name: Name for database configuration.
        utf8filename: Use utf8filename DPB flag.

    .. note::

       The returned instance must be assigned to module-level variable. Name of this variable
       is important, as it's used to reference the fixture in other fixture-factory functions
       that use the database, and the test function itself.
    """

    @pytest.fixture
    def database_fixture(request: pytest.FixtureRequest, db_path, db_cache) -> Database:
        db = Database(db_path, filename, user, password, charset, debug=str(request.module),
                      config_name=config_name, utf8filename=utf8filename)
        if not do_not_create:
            if from_backup is None and copy_of is None:
                db.create(page_size, sql_dialect, db_cache)
            elif from_backup is not None:
                db.restore(from_backup)
            elif copy_of is not None:
                db.copy(copy_of)
            if async_write:
                db.set_async_write()
            if init: # Do not check for None, we want to skip empty scripts as well
                try:
                    db.init(init)
                except Error as exc:
                    if hasattr(exc, 'stderr'):
                        request.node.user_properties.append(("dbinit-stderr", exc.stderr))
                    raise
        yield db
        if not do_not_drop:
            db.drop()

    return database_fixture

@pytest.fixture
def db_path(tmp_path) -> Path:
    """Internal :doc:`fixture <pytest:explanation/fixtures>` function that returns path to
    temporary directory for database files.

    On non-Windows platforms makes sure that directory has permissions that user, group and
    other can write there.

    .. seealso:: `tmp_path <_pytest.tmpdir.tmp_path>` fixture.
    """
    if platform.system != 'Windows':
        base = _vars_['basetemp']
        if base is None:
            tmp_path.parent.chmod(16895)
            tmp_path.parent.parent.chmod(16895)
        else:
            Path(base).chmod(16895)
        tmp_path.chmod(16895)
    return tmp_path

class User:
    """Object to access and manage single Firebird test user.

    Arguments:
        db: Database used to manage test user.
        filename: Database filename.
        name: User name.
        password: User password.
        plugin: Secutiry plugin name used to manage this user.
        charset: Firebird CHARACTER SET used for connections that manage this user.
        active: Create user as active.
        tags: Tags for user.
        first_name: User's first name.
        middle_name: User's middle name.
        last_name: User's last name.
        admin: ADMIN flag.
        do_not_create: When `True`, the user is not created when `with` context is entered.

    .. note::

       Users are managed through SQL commands executed on connection to specified test
       database to ensure that security database configuration for this test database is
       respected.

    .. important::

       It's NOT RECOMMENDED to create instances of this class directly! The preffered way
       is to use fixtures created by `user_factory`.

       As test databases are managed by fixtures, it's necessary to ensure that users are
       created after database initialization, and removed before test database is removed.
       So, user `setup` and `teardown` is managed via context manager protocol and the
       :ref:`with <with>` statement that must be executed within scope of used database.
       Fixture created by `user_factory` does this automatically.
    """
    def __init__(self, db: Database, *, name: str, password: str, plugin: str, charset: str,
                 active: bool=True, tags: Optional[Dict[str]]=None,
                 first_name: Optional[str]=None, middle_name: Optional[str]=None,
                 last_name: Optional[str]=None, admin: bool=False, do_not_create: bool=False):
        #: Database used to manage test user.
        self.db: Database = db
        self.__name: str = name if name.startswith('"') else name.upper()
        self.__password: str = password
        self.__plugin: str = plugin
        self.__p = ''  if self.__plugin is None else f" USING PLUGIN {self.__plugin}"
        #: Firebird CHARACTER SET used for connections that manage this user.
        self.charset: str = charset
        self.__active: bool = active
        self.__first_name: Optional[str] = first_name
        self.__middle_name: Optional[str] = middle_name
        self.__last_name: Optional[str] = last_name
        self.__tags: Dict[str, str] = tags
        self.__admin: bool = admin
        self.__create: bool = not do_not_create
    def __enter__(self) -> User:
        if self.__create:
            self.create()
        return self
    def __exit__(self, exc_type, exc_value, traceback) -> None:
        if self.exists():
            self.drop()
    def exists(self) -> bool:
        "Returns `True` if user exists."
        __tracebackhide__ = True
        with self.db.connect(charset=self.charset) as con:
            c = con.cursor()
            name = self.name[1:-1] if self.name.startswith('"') else self.name
            cmd = f"SELECT COUNT(*) FROM SEC$USERS WHERE SEC$USER_NAME = '{name}'"
            if self.__plugin is not None:
                cmd += f" AND SEC$PLUGIN = '{self.__plugin}'"
            cnt = c.execute(cmd).fetchone()[0]
        return cnt > 0
    def create(self) -> None:
        """Creates user in security context of defined test database. Called automatically
        when `with` context is entered and `do_not_create` is `False`.

        .. note:: Sets all user attributes (names, tags etc.).

        .. important:: If user already exists, it's removed before it's re-created.
        """
        __tracebackhide__ = True
        if self.exists():
            self.drop()
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f"CREATE USER {self.name} PASSWORD '{self.__password}' {'ACTIVE' if self.__active else 'INACTIVE'}{self.__p}{' GRANT ADMIN ROLE' if self.__admin else ''}")
            if self.__first_name is not None:
                con.execute_immediate(f"ALTER USER {self.name} SET FIRSTNAME '{self.__first_name}'{self.__p}")
            if self.__middle_name is not None:
                con.execute_immediate(f"ALTER USER {self.name} SET MIDDLENAME '{self.__middle_name}'{self.__p}")
            if self.__last_name is not None:
                con.execute_immediate(f"ALTER USER {self.name} SET LASTNAME '{self.__last_name}'{self.__p}")
            if self.__tags is not None:
                tags = ', '.join([f"{name} = '{value}'" for name, value in self.__tags.items()])
                con.execute_immediate(f"ALTER USER {self.name} SET TAGS ({tags}){self.__p}")
            con.commit()
        print(f"CREATE user: {self.name} PLUGIN: {self.plugin}")
    def drop(self) -> None:
        """Drop user in security context of defined test database. Called automatically
        on `with` context exit.

        .. note:: If needed, also removes all grants for this user via REVOKE ALL ON ALL.
        """
        __tracebackhide__ = True
        with self.db.connect(charset=self.charset) as con:
            c = con.cursor()
            grants = c.execute('select count(*) from '\
                               '(select rdb$user as a from rdb$user_privileges '\
                               'union all select sec$user as a from sec$db_creators) '\
                               'where a = ?',
                               [self.name if self.name.startswith('"') else self.name.upper()]).fetchone()[0]
            if grants > 0:
                c.execute(f'revoke all on all from {self.name}')
                con.commit()
            #
            print(f"DROP user: {self.name} PLUGIN: {self.plugin}")
            con.execute_immediate(f"DROP USER {self.name}{self.__p}")
            con.commit()
    def set_tag(self, name: str, *, value: str) -> None:
        """Create/Set user tag value.

        Arguments:
           name: Tag name.
           value: Tag value.
        """
        __tracebackhide__ = True
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f"ALTER USER {self.name} TAGS ({name} = '{value}'){self.__p}")
            con.commit()
    def drop_tag(self, name: str) -> None:
        """Drop user tag.

        Arguments:
           name: Tag name.
        """
        __tracebackhide__ = True
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f"ALTER USER {self.name} TAGS (DROP {name}){self.__p}")
            con.commit()
    @property
    def name(self) -> str:
        "User name [R/O]"
        return self.__name
    @property
    def plugin(self) -> str:
        "Security plugin [R/O]"
        if self.__plugin is None:
            with self.db.connect() as con:
                c = con.cursor()
                self.__plugin = c.execute(f'SELECT SEC$PLUGIN FROM SEC$USERS').fetchone()[0].strip()
        return self.__plugin
    @property
    def password(self) -> str:
        "User password [R/W]"
        return self.__password
    @password.setter
    def password(self, value: str) -> None:
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f"ALTER USER {self.__name} SET PASSWORD '{value}'")
            con.commit()
        self.__password = value
    @property
    def first_name(self) -> str:
        "First name [R/W]"
        return self.__first_name
    @first_name.setter
    def first_name(self, value: str) -> None:
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f"ALTER USER {self.__name} SET FIRSTNAME '{value}'")
            con.commit()
        self.__first_name = value
    @property
    def middle_name(self) -> str:
        "Middle name [R/W]"
        return self.__middle_name
    @middle_name.setter
    def middle_name(self, value: str) -> None:
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f"ALTER USER {self.__name} SET MIDDLENAME '{value}'")
            con.commit()
        self.__middle_name = value
    @property
    def last_name(self) -> str:
        "Last name [R/W]"
        return self.__last_name
    @last_name.setter
    def last_name(self, value: str) -> None:
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f"ALTER USER {self.__name} SET LASTNAME '{value}'")
            con.commit()
        self.__last_name = value
    @property
    def tags(self) -> Dict[str]:
        "User tags [R/O]"
        return dict(self.__tags)

def user_factory(db_fixture_name: str, *, name: str, password: str='', plugin: Optional[str]=None,
                 charset: str='utf8', active: bool=True, tags: Optional[Dict[str]]=None,
                 first_name: Optional[str]=None, middle_name: Optional[str]=None,
                 last_name: Optional[str]=None, admin: bool=False, do_not_create: bool=False):
    """Factory function that returns :doc:`fixture <pytest:explanation/fixtures>` providing
    the `User` instance.

    Arguments:
        db_fixture_name: Name of database fixture.
        name: User name.
        password: User password.
        plugin: Secutiry plugin name used to manage this user.
        charset: Firebird CHARACTER SET used for connections that manage this user.
        active: Create user as active.
        tags: Tags for user.
        first_name: User's first name.
        middle_name: User's middle name.
        last_name: User's last name.
        admin: ADMIN flag.
        do_not_create: When `True`, the user is not created when `with` context is entered.

    .. important::

       The `db_fixture_name` must be name of variable that holds the fixture created by
       `db_factory` function.

       **Test must use both, user and database fixtures!**

    .. note::

       Database must exists before user is created by fixture, so you cannot use database
       fixtures created with `do_not_create` option, unless also the user is created with it!
    """

    @pytest.fixture
    def user_fixture(request: pytest.FixtureRequest) -> User:
        with User(request.getfixturevalue(db_fixture_name), name=name, password=password,
                  plugin=plugin, charset=charset, active=active, tags=tags,
                  first_name=first_name, middle_name=middle_name, last_name=last_name,
                  admin=admin, do_not_create=do_not_create) as user:
            yield user

    return user_fixture

def trace_thread(act: Action, b: Barrier, cfg: List[str], output: List[str], keep_log: bool,
                 encoding: str, encoding_errors: str, user: str, password: str,
                 role: str, stop: Event):
    """Function used by `TraceSession` for execution in separate thread to run trace session.

    Arguments:
        act: Action instance.
        b: Barrier instance used for synchronization.
        cfg: Trace configuration.
        output: List used to store trace session output.
        keep_log: When `True`, the trace session output is discarded.
        encoding: Encoding for trace session output.
        encoding_errors: Error handler for trace session output encoding.
        user: user name
        password: User password
        role: User role
        stop: Event used to stop the trace thread
    """
    connected = False
    try:
        with act.connect_server(encoding=encoding, encoding_errors=encoding_errors,
                                user=user, password=password) as srv:
            output.append(srv.trace.start(config='\n'.join(cfg)))
            b.wait()
            connected = True
            while not stop.is_set():
                line = srv.readline_timed(1)
                if line is not TIMEOUT:
                    if not line:
                        stop.set()
                    elif keep_log:
                        output.append(line)
    except:
        if not connected:
            b.wait()
        raise

class TraceSession:
    """Object to manage Firebird trace session.

    Arguments:
       act: Action instance.
       config: Trace session configuration.
       keep_log: When `False`, the trace session output is discarded.
       encoding: Encoding for trace session output.
       encoding_errors: Error handler for trace session output encoding.

    .. important::

       Do not create instances of this class directly! Always use `Action.trace` method and
       the :ref:`with <with>` statement.

       Example::

          with act.trace(db_events=trace_cfg):
              # your test here
          # Process trace output, for example by passing to stdout
          act_1.trace_to_stdout()

       The trace session is automatically stopped on exit from `with` context, and trace
       output is copied to `Action.trace_log` attribute.
    """
    def __init__(self, act: Action, config: List[str], user: str, password: str,
                 role: Optional[str]=None, keep_log: bool=True,
                 encoding: Optional[str]=None, encoding_errors: Optional[str]=None):
        #: Action instance.
        self.act: Action = act
        #: Trace session configuration.
        self.config: List[str] = config
        #: User name
        self.user: str = user
        #: User password
        self.password: str = password
        #: User role
        self.role: str = role
        #: List used to store trace session output.
        self.output: List[str] = []
        #: When `True`, the trace session output is discarded.
        self.keep_log: bool = keep_log
        #: Thread used to execute trace session
        self.trace_thread: Thread = None
        #: Encoding for trace session output.
        self.encoding: Optional[str] = encoding
        #: Encoding errors handling for trace session output.
        self.encoding_errors: Optional[str] = encoding_errors
        self.stop_event: Event = Event()
    def __enter__(self) -> TraceSession:
        b = Barrier(2)
        self.trace_thread = Thread(target=trace_thread, args=[self.act, b, self.config,
                                                              self.output, self.keep_log,
                                                              self.encoding,
                                                              self.encoding_errors,
                                                              self.user, self.password,
                                                              self.role, self.stop_event])
        self.trace_thread.start()
        b.wait()
        return self
    def __exit__(self, exc_type, exc_value, traceback) -> None:
        time.sleep(2)
        session = self.output.pop(0)
        self.stop_event.set()
        self.trace_thread.join(5.0)
        if self.trace_thread.is_alive():
            pytest.fail('Trace thread still alive')
        self.act.trace_log = self.output

class ServerKeeper:
    """Helper context manager class to temporary change the server used by `Action`.

    Arguments:
        act: Action instance.
        server: New server specification.

    Example::

       with ServerKeeper(act_1, None): # Use embedded server for trace
           with act_1.trace(config=trace_1):
               # Your test here
       # Process trace output, for example by passing to stdout
       act_1.trace_to_stdout()
    """
    def __init__(self, act: Action, server: str):
        self.act: Action = act
        self.server:str = server
        self.old_value = None
    def __enter__(self):
        self.old_value = self.act.vars['server']
        self.act.vars['server'] = self.server
        return self
    def __exit__(self, exc_type, exc_value, traceback) -> None:
        if self.old_value is not None:
            self.act.vars['server'] = self.old_value

class Envar:
    """Object to manage environment variables in tests.

    Arguments:
        name: Variable name.
        value: Variable value.

    .. important::

       It's NOT RECOMMENDED to create instances of this class directly! The preffered way
       is to use fixtures created by `envar_factory`.

       Environment variable `setup` and `teardown` is managed via context manager protocol
       and the :ref:`with <with>` statement. Fixture created by `envar_factory` does this
       automatically.

       .. note::

          If environment variable already exists when context is entered, its value is
          preserved and restored at context exit. If the variable does not exist before,
          it is deleted.
    """
    def __init__(self, name: str, value: str):
        self.name = name
        self.value = value
        self.old_value = None
    def __enter__(self) -> Envar:
        if self.name in os.environ:
            self.old_value = os.environ[self.name]
        os.environ[self.name] = self.value
        return self
    def __exit__(self, exc_type, exc_value, traceback) -> None:
        if self.old_value is not None:
            os.environ[self.name] = self.old_value
        else:
            del os.environ[self.name]

def envar_factory(*, name: str, value: str):
    """Factory function that returns :doc:`fixture <pytest:explanation/fixtures>` providing
    the `Envar` instance.

    Arguments:
        name: Variable name.
        value: Variable value.

    .. note::

       If environment variable already exists before test execution, its value is preserved
       and restored when test execution is finished. If the variable does not exist before,
       it is deleted.
    """

    @pytest.fixture
    def envar_fixture() -> User:
        with Envar(name, value) as envar:
            yield envar

    return envar_fixture

class Role:
    """Object to access and manage single Firebird test role.

    Arguments:
        database: Database used to manage test role.
        name: Role name.
        charset: Firebird CHARACTER SET used for connections that manage this role.
        do_not_create: When `True`, the role is not created when `with` context is entered.

    .. note::

       Roles are managed through SQL commands executed on connection to specified test
       database.

    .. important::

       It's NOT RECOMMENDED to create instances of this class directly! The preffered way
       is to use fixtures created by `role_factory`.

       As test databases are managed by fixtures, it's necessary to ensure that roles are
       created after database initialization, and removed before test database is removed.
       So, role `setup` and `teardown` is managed via context manager protocol and the
       :ref:`with <with>` statement that must be executed within scope of used database.
       Fixture created by `role_factory` does this automatically.
    """
    def __init__(self, database: Database, name: str, charset: str, do_not_create: bool):
        #: Database used to manage test role.
        self.db: Database = database
        #: Role name.
        self.name: str = name if name.startswith('"') else name.upper()
        #: Firebird CHARACTER SET used for connections that manage this role.
        self.charset = charset
        #: When `True`, the role is not created when `with` context is entered.
        self.do_not_create: bool = do_not_create
    def __enter__(self) -> Role:
        if not self.do_not_create:
            self.create()
        return self
    def __exit__(self, exc_type, exc_value, traceback) -> None:
        if self.exists():
            self.drop()
    def create(self) -> None:
        """Creates role in test database. Called automatically when `with` context is
        entered and `do_not_create` is `False`.

        .. important:: If role already exists, it's removed before it's re-created.
        """
        __tracebackhide__ = True
        if self.exists():
            self.drop()
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f'CREATE ROLE {self.name}')
            con.commit()
            print(f"CREATE role: {self.name}")
    def drop(self) -> None:
        """Drop role in defined test database. Called automatically on `with` context exit.
        """
        __tracebackhide__ = True
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f'DROP ROLE {self.name}')
            con.commit()
            print(f"DROP role: {self.name}")
    def exists(self) -> bool:
        "Returns `True` if role exists."
        __tracebackhide__ = True
        with self.db.connect(charset=self.charset) as con:
            c = con.cursor()
            name = self.name[1:-1] if self.name.startswith('"') else self.name
            cmd = f"SELECT COUNT(*) FROM RDB$ROLES WHERE RDB$ROLE_NAME = '{name}'"
            cnt = c.execute(cmd).fetchone()[0]
        return cnt > 0

def role_factory(db_fixture_name: str, *, name: str, charset: str='utf8', do_not_create: bool=False):
    """Factory function that returns :doc:`fixture <pytest:explanation/fixtures>` providing
    the `Role` instance.

    Arguments:
        db_fixture_name: Name of database fixture.
        name: User name.
        charset: Firebird CHARACTER SET used for connections that manage this role.
        do_not_create: When `True`, the role is not created when `with` context is entered.

    .. important::

       The `db_fixture_name` must be name of variable that holds the fixture created by
       `db_factory` function.

       **Test must use both, role and database fixtures!**

    .. note::

       Database must exists before role is created by fixture, so you cannot use database
       fixtures created with `do_not_create` option, unless also the role is created with it!
    """
    @pytest.fixture
    def role_fixture(request: pytest.FixtureRequest) -> User:
        with Role(request.getfixturevalue(db_fixture_name), name, charset, do_not_create) as role:
            yield role

    return role_fixture

class Mapping:
    """Object to access and manage Firebird mapping.

    Arguments:
        database: Database used to manage mapping.
        name: Mapping name.
        charset: Firebird CHARACTER SET used for connections that manage this mapping.
        do_not_create: When `True`, the mapping is not created when `with` context is entered.
        is_global: Whether mapping is global or not.
        source: Authentication plugin name, `ANY` for any plugin, `-` for mapping or `*` for
                any method.
        source_db: Database where authentication succeeded.
        serverwide: Work only with server-wide plugins or not.
        from_name: The name of the object from which mapping is performed. Could be `None`
                   for any value of given type.
        from_type: The type of that name — user name, role or OS group—depending upon the
                   plug-in that added that name during authentication.
        to_name: The name of the object TO which mapping is performed.
        to_type: The type, for which only `USER` or `ROLE` is valid.

    .. note::

       Mappings are managed through SQL commands executed on connection to specified test
       database.

    .. important::

       It's NOT RECOMMENDED to create instances of this class directly! The preffered way
       is to use fixtures created by `mapping_factory`.

       As test databases are managed by fixtures, it's necessary to ensure that mappings are
       created after database initialization, and removed before test database is removed.
       So, mapping `setup` and `teardown` is managed via context manager protocol and the
       :ref:`with <with>` statement that must be executed within scope of used database.
       Fixture created by `mapping_factory` does this automatically.
    """
    def __init__(self, database: Database, name: str, charset: str, do_not_create: bool,
                 is_global: bool, source: Database, source_db: Optional[Database],
                 serverwide: bool, from_name: str, from_type: str,
                 to_name: Optional[str], to_type: str):
        #: Database used to manage mapping.
        self.db: Database = database
        #: Mapping name.
        self.name: str = name if name.startswith('"') else name.upper()
        #: Firebird CHARACTER SET used for connections that manage this mapping.
        self.charset = charset
        #: When `True`, the mapping is not created when `with` context is entered.
        self.do_not_create: bool = do_not_create
        #: Whether mapping is global or not.
        self.is_global = is_global
        #: Authentication plugin name, `ANY` for any plugin, `-` for mapping or `*` for any method.
        self.source: str = source
        #: Database where authentication succeeded.
        self.source_db: Optional[Database] = source_db
        #: Work only with server-wide plugins or not.
        self.serverwide: bool = serverwide
        #: The name of the object from which mapping is performed. Could be `None` for any value of given type.
        self.from_name: str = from_name
        #: The type of that name — user name, role or OS group—depending upon the plug-in that added that name during authentication.
        self.from_type: str = from_type
        #: The name of the object TO which mapping is performed.
        self.to_name: str = to_name if to_name.startswith('"') else to_name.upper()
        #: Target type, for which only `USER` or `ROLE` is valid.
        self.to_type: str = to_type
    def __enter__(self) -> Role:
        if not self.do_not_create:
            self.create()
        return self
    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.drop()
    def create(self) -> None:
        """Creates mapping. Called automatically when `with` context is entered.
        """
        __tracebackhide__ = True
        with self.db.connect(charset=self.charset) as con:
            if self.source == '*':
                using = "'*'"
            elif self.source == '-':
                using = 'MAPPING'
            elif self.source == 'ANY':
                using = 'ANY PLUGIN'
                if self.serverwide:
                    using += ' SERVERWIDE'
            else:
                using = f'PLUGIN {self.source}'
            if self.source_db is not None:
                using += f' IN "{self.source_db.db_path}"'
            if self.from_name is None:
                from_spec = f'ANY {self.from_type}'
            else:
                from_spec = f'{self.from_type} {self.from_name}'
            cmd = f'''CREATE {"GLOBAL " if self.is_global else " "}MAPPING {self.name}
            USING {using}
            FROM {from_spec}
            TO {self.to_type} {self.to_name}
            '''
            con.execute_immediate(cmd)
            con.commit()
            print(f"CREATE mapping: {self.name}")
    def drop(self) -> None:
        """Drop role in defined test database. Called automatically on `with` context exit.
        """
        __tracebackhide__ = True
        with self.db.connect(charset=self.charset) as con:
            con.execute_immediate(f'DROP {"GLOBAL " if self.is_global else " "}MAPPING {self.name}')
            con.commit()
            print(f"DROP mapping: {self.name}")

def mapping_factory(db_fixture_name: str, *, name: str, is_global: bool, source: str,
                    from_name: str, from_type: str, to_name: Optional[str], to_type: str,
                    source_db_fixture_name: str=None, serverwide: bool=False,
                    charset: str='utf8', do_not_create: bool=False):
    """Factory function that returns :doc:`fixture <pytest:explanation/fixtures>` providing
    the `Mapping` instance.

    Arguments:
        db_fixture_name: Name of database fixture used to manage mapping.
        name: Mapping name.
        is_global: Whether mapping is global or not.
        source: Authentication plugin name, `ANY` for any plugin, `-` for mapping or `*` for
                any method.
        from_name: The name of the object from which mapping is performed. Could be `None`
                   for any value of given type.
        from_type: The type of that name — user name, role or OS group—depending upon the
                   plug-in that added that name during authentication.
        to_name: The name of the object TO which mapping is performed.
        to_type: The type, for which only `USER` or `ROLE` is valid.
        source_db_fixture_name: Name of database fixture for database where authentication succeeded.
        serverwide: Work only with server-wide plugins or not.
        charset: Firebird CHARACTER SET used for connections that manage this mapping.
        do_not_create: When `True`, the mapping is not created when `with` context is entered.

    .. important::

       The `db_fixture_name` and `source_db_fixture_name` must be names of variable that
       holds the fixture created by `db_factory` function.

       **Test must use both, mapping and database fixtures!**

    .. note::

       Database must exists before mapping is created by fixture, so you cannot use database
       fixtures created with `do_not_create` option, unless also the mapping is created with it!
    """
    @pytest.fixture
    def mapping_fixture(request: pytest.FixtureRequest) -> Mapping:
        if source_db_fixture_name is not None:
            source_db = request.getfixturevalue(source_db_fixture_name)
        else:
            source_db = None
        with Mapping(request.getfixturevalue(db_fixture_name), name, charset, do_not_create,
                     is_global, source, source_db, serverwide, from_name, from_type,
                     to_name, to_type) as mapping:
            yield mapping

    return mapping_fixture

class Action:
    """Class to manage and execute Firebird tests.

    Arguments:
      db: Primary test database.
      script: Test ISQL script.
      substitutions: REGEX substitutions to be applied on stdout/stderr on cleanup.
      outfile: Base path/filename for stdout/stderr protocols of external Firebird tool execution.
      node: `pytest.Item` node that uses this action.

    .. important::

        Do not create instances of this class directly! Use **ONLY** fixtures created by
        `isql_act` or `python_act`.
    """
    def __init__(self, db: Database, script: str, substitutions: Substitutions,
                 outfile: Path, node: pytest.Item):
        #: Weakref proxy to `pytest.Item` (i.e. test function) that uses this Action.
        #: Used primarily to add information into test report via `add_report_section`.
        self._node: pytest.Item = weakref.proxy(node)
        #: Primary test database.
        self.db: Database = db
        self._script: str = None
        self.script = script
        #: Return code from last execution of external Firebird tool.
        self.return_code: int = 0
        #: Content of standard output from last execution of external Firebird tool.
        self.stdout: str = ''
        self._clean_stdout: str = None
        #: Content of error output from last execution of external Firebird tool.
        self.stderr: str = ''
        self._clean_stderr: str = None
        #: Expected standard output.
        self.expected_stdout: str = ''
        self._clean_expected_stdout: str = None
        #: Expected error output.
        self.expected_stderr: str = ''
        self._clean_expected_stderr: str = None
        #: REGEX substitutions applied on stdout/stderr on cleanup.
        self.substitutions: Substitutions = [] if substitutions is None else [x for x in substitutions]
        #: Base path/filename for stdout/stderr protocols of external Firebird tool execution.
        self.outfile: Path = outfile
        #: Output from last executed trace session.
        self.trace_log: List[str] = []
    def strip_white(self, value: str) -> str:
        """Remove all leading and trailing whitespace characters on ALL lines in value.
        """
        value= re.sub('(?m)^\\s+', '', value)
        return re.sub('(?m)\\s+$', '', value)
    def strip_isql(self, value: str) -> str:
        """Remove all unwanted isql noise strings from value.
        """
        for regex in map(re.compile, ['(?m)Database:.*\\n?', 'SQL>[ \\t]*\\n?',
                                      'CON>[ \\t]*\\n?', '-->[ \\t]*\\n?']):
            value = re.sub(regex, '', value)
        return value
    def clean_string(self, value: str, substitutions: Substitutions=[], isql: bool=True,
                     remove_white: bool=True) -> str:
        """Returns string with removed isql noise strings and leading and trailing
        whitespace, and after other substitutions.

        Arguments:
          value: Value for cleansing.
          substitutions: List with substitution tuples (regex patern, replacement)
          isql: When True [default], all isql "noise" strings are removed.
          remove_white: When True [default], all leading and trailing whitespace characters
            on each line are removed.

        """
        if not value:
            return value
        if isql:
            value = self.strip_isql(value)
        for pattern, replacement in substitutions:
            value= re.compile(pattern, re.M).sub(replacement, value)
        if remove_white:
            value = self.strip_white(value)
        return value
    def execute(self, *, do_not_connect: bool=False, charset: Optional[str]=None,
                io_enc: Optional[str]=None, combine_output: bool=False) -> None:
        """Execute test `script` using ISQL.

        Arguments:
           do_not_connect: Do not connect to primary test database on ISQL execution.
           charset: Charset to be used for connection instead charset of primary test database.
           io_enc: Python encoding to be used to decode text output instead encoding that
              corresponds to used character set.
           combine_output: Combine stdout/stderr into stdout.

        By default, script is executed on primary test database with `Database` charset. ISQL
        output is captured into `stdout` and `stderr`, and exit code is stored into `return_code`.

        If pytest `--save-output` command-line option is used, the content of stdout/stderr
        is also stored into test protocol files with `.out`/`.err` suffix.

        .. important::

           If `return_code` is not zero, and script execution failure is not expected (either
           via defined `expected_stderr` value or using `combine_output` = True), it raises
           `ExecutionError` with "Test script execution failed" message, and adds content
           of stdout and stderr into test run report.
        """
        __tracebackhide__ = True
        out_file: Path = self.outfile.with_suffix('.out')
        err_file: Path = self.outfile.with_suffix('.err')
        if out_file.is_file():
            out_file.unlink()
        if err_file.is_file():
            err_file.unlink()
        charset = charset.upper() if charset else self.db.charset
        if io_enc is None:
            io_enc = CHARSET_MAP[charset]
        params = [_vars_['isql'], '-ch', charset]
        if not do_not_connect:
            params.extend(['-user', self.db.user, '-password', self.db.password, str(self.db.dsn)])
        if combine_output:
            result: CompletedProcess = run(params, input=substitute_macros(self.script, self.db.subs),
                                           encoding=io_enc, stdout=PIPE, stderr=STDOUT)
        else:
            result: CompletedProcess = run(params, input=self.script,
                                           encoding=io_enc, capture_output=True)
        if (result.returncode or result.stderr) and not bool(self.expected_stderr) and not combine_output:
            self._node.add_report_section('call', 'ISQL stdout', result.stdout)
            self._node.add_report_section('call', 'ISQL stderr', result.stderr)
            raise ExecutionError("Test script execution failed\n\n" + result.stderr)

        self.return_code: int = result.returncode
        self.stdout: str = result.stdout
        self.stderr: str = result.stderr
        # Store output
        if _vars_['save-output']:
            if self.stdout:
                out_file.write_text(self.stdout, encoding='utf8')
            if self.stderr:
                err_file.write_text(self.stderr, encoding='utf8')
    def extract_meta(self, *, from_db: Optional[Database]=None, charset: Optional[str]=None,
                     io_enc: Optional[str]=None) -> str:
        """Call ISQL to extract database metadata (using `-x` option).

        Arguments:
           from_db: Extract metadata from specified database instead primary test database.
           charset: Charset to be used for connection instead charset of primary test database.
           io_enc: Python encoding to be used to decode text output instead encoding that
              corresponds to used character set.

        By default, metadata are extracted from primary test database using `Database`
        charset. ISQL output is captured into `stdout` and `stderr`, and exit code is stored
        into `return_code`.

        If pytest `--save-output` command-line option is used, the content of stdout/stderr
        is also stored into test protocol files with `.out`/`.err` suffix.

        .. important::

           If `return_code` is not zero, it raises `ExecutionError` with
           "ISQL execution failed" message, and adds content of stdout and stderr into test
           run report.
        """
        __tracebackhide__ = True
        out_file: Path = self.outfile.with_suffix('.out')
        err_file: Path = self.outfile.with_suffix('.err')
        if out_file.is_file():
            out_file.unlink()
        if err_file.is_file():
            err_file.unlink()
        db = self.db if from_db is None else from_db
        charset = charset.upper() if charset else self.db.charset
        if io_enc is None:
            io_enc = CHARSET_MAP[charset]
        result = run([_vars_['isql'], '-x', '-ch', charset, '-user', db.user,
                      '-password', db.password, str(db.dsn)],
                     encoding=io_enc, capture_output=True)
        if result.returncode:
            self._node.add_report_section('call', 'ISQL stdout', result.stdout)
            self._node.add_report_section('call', 'ISQL stderr', result.stderr)
            raise ExecutionError("ISQL execution failed\n\n" + result.stderr)
        self.return_code: int = result.returncode
        self.stdout: str = result.stdout
        self.stderr: str = result.stderr
        # Store output
        if _vars_['save-output']:
            if self.stdout:
                out_file.write_text(self.stdout, encoding=io_enc)
            if self.stderr:
                err_file.write_text(self.stderr, encoding=io_enc)
        return self.stdout
    def gstat(self, *, switches: List[str], charset: Optional[str]=None,
              io_enc: Optional[str]=None, connect_db: bool=True, credentials: bool=True) -> None:
        """Run `gstat` utility.

        Arguments:
           switches: List with command-line switches (passed to `subprocess.run`).
           charset: Decode output using encoding that corresponds to this charset instead
             charset of primary test database.
           io_enc: Python encoding to be used to decode text output instead encoding that
              corresponds to used character set.
           connect_db: When `True` adds primary test database DSN to switches.
           credentials: When `True` adds switches to connect as primary test database user.

        By default, GSTAT is executed on primary test database, and output is captured into
        `stdout` and `stderr`, and exit code is stored to `return_code`.

        If pytest `--save-output` command-line option is used, the content of stdout/stderr
        is also stored into test protocol files with `.out`/`.err` suffix.

        Example::

           act.gstat(switches=['-h'])
           act.gstat(switches=['-h', str(test_db)], connect_db=False)

        .. important::

           If `return_code` is not zero, and gstat execution failure is not expected
           (`expected_stderr` value is not defined), it raises  `ExecutionError` with
           "gstat execution failed" message, and adds content of stdout and stderr into
           test run report.
        """
        __tracebackhide__ = True
        out_file: Path = self.outfile.with_suffix('.out')
        err_file: Path = self.outfile.with_suffix('.err')
        if out_file.is_file():
            out_file.unlink()
        if err_file.is_file():
            err_file.unlink()
        #
        charset = charset.upper() if charset else self.db.charset
        if io_enc is None:
            io_enc = CHARSET_MAP[charset]
        params = [_vars_['gstat']]
        if credentials:
            params.extend(['-user', self.db.user, '-password', self.db.password])
        params.extend(switches)
        if connect_db:
            params.append(str(self.db.dsn))
        result: CompletedProcess = run(params, encoding=io_enc, capture_output=True)
        if (result.returncode or result.stderr) and not bool(self.expected_stderr):
            self._node.add_report_section('call', 'gstat stdout', result.stdout)
            self._node.add_report_section('call', 'gstat stderr', result.stderr)
            raise ExecutionError("gstat execution failed\n\n" + result.stderr)
        self.return_code: int = result.returncode
        self.stdout: str = result.stdout
        self.stderr: str = result.stderr
        # Store output
        if _vars_['save-output']:
            if self.stdout:
                out_file.write_text(self.stdout, encoding=io_enc)
            if self.stderr:
                err_file.write_text(self.stderr, encoding=io_enc)
    def gsec(self, *, switches: Optional[List[str]]=None, charset: Optional[str]=None,
             io_enc: Optional[str]=None, input: Optional[str]=None, credentials: bool=True) -> None:
        """Run `gstat` utility.

        Arguments:
           switches: List with command-line switches (passed to `subprocess.run`).
           charset: Decode output using encoding that corresponds to this charset instead
             charset of primary test database.
           io_enc: Python encoding to be used to decode text output instead encoding that
              corresponds to used character set.
           input: Text to be passed to gsec via stdin.
           credentials: When `True` adds switches to connect as primary test database user.

        By default, GSEC is executed on primary test database, and output is captured into
        `stdout` and `stderr`, and exit code is stored to `return_code`.

        If pytest `--save-output` command-line option is used, the content of stdout/stderr
        is also stored into test protocol files with `.out`/`.err` suffix.

        Example::

           act.gsec(switches=['add', 'user_name', '-pw', 'password'])
           act.gsec(switches=['add', 'user_name', '-pw', 'password',
                              '-user', test_user.name, '-password', test_user.password],
                    credentials=False)

        .. important::

           If `return_code` is not zero, and gsec execution failure is not expected
           (`expected_stderr` value is not defiend), it raises `ExecutionError` with
           "gsec execution failed" message, and adds content of stdout and stderr into
           test run report.
        """
        __tracebackhide__ = True
        out_file: Path = self.outfile.with_suffix('.out')
        err_file: Path = self.outfile.with_suffix('.err')
        if out_file.is_file():
            out_file.unlink()
        if err_file.is_file():
            err_file.unlink()
        #
        charset = charset.upper() if charset else self.db.charset
        if io_enc is None:
            io_enc = CHARSET_MAP[charset]
        params = [_vars_['gsec']]
        if switches is not None:
            params.extend(switches)
        if credentials:
            params.extend(['-user', self.db.user, '-password', self.db.password])
        result: CompletedProcess = run(params, input=input,
                                       encoding=io_enc, capture_output=True)
        if (result.returncode or result.stderr) and not bool(self.expected_stderr):
            self._node.add_report_section('call', 'gsec stdout', result.stdout)
            self._node.add_report_section('call', 'gsec stderr', result.stderr)
            raise ExecutionError("gsec execution failed\n\n" + result.stderr)
        self.return_code: int = result.returncode
        self.stdout: str = result.stdout
        self.stderr: str = result.stderr
        # Store output
        if _vars_['save-output']:
            if self.stdout:
                out_file.write_text(self.stdout, encoding=io_enc)
            if self.stderr:
                err_file.write_text(self.stderr, encoding=io_enc)
    def gbak(self, *, switches: Optional[List[str]]=None, charset: Optional[str]=None,
             io_enc: Optional[str]=None, credentials: bool=True, combine_output: bool=False) -> None:
        """Run `gbak` utility.

        Arguments:
           switches: List with command-line switches (passed to `subprocess.run`).
           charset: Firebird CHARACTER SET name.
           io_enc: Python encoding to be used to decode text output instead encoding that
              corresponds to used character set.
           credentials: When `True` adds switches to connect as primary test database user.
           combine_output: Combine stdout/stderr into stdout.

        By default, GBAK is executed on primary test database, and output is captured into
        `stdout` and `stderr`, and exit code is stored to `return_code`.

        If pytest `--save-output` command-line option is used, the content of stdout/stderr
        is also stored into test protocol files with `.out`/`.err` suffix.

        Example::

           act.gbak(switches=['-b', act.db.dsn, fbk_filename])
           act.gbak(switches=['-b', '-user', test_user.name, '-pas', test_user.password,
                              act.db.dsn, fbk_filename], credentials=False)

        .. important::

           If `return_code` is not zero, and script execution failure is not expected (either
           via defined `expected_stderr` value or using `combine_output` = True), it raises and
           `ExecutionError` with "gbak execution failed" message, and adds content of stdout
           and stderr into test run report.
        """
        __tracebackhide__ = True
        out_file: Path = self.outfile.with_suffix('.out')
        err_file: Path = self.outfile.with_suffix('.err')
        if out_file.is_file():
            out_file.unlink()
        if err_file.is_file():
            err_file.unlink()
        #
        charset = charset.upper() if charset else self.db.charset
        if io_enc is None:
            io_enc = CHARSET_MAP[charset]
        params = [_vars_['gbak']]
        if credentials:
            params.extend(['-user', self.db.user, '-password', self.db.password])
        if switches is not None:
            params.extend(switches)
        if combine_output:
            result: CompletedProcess = run(params, encoding=io_enc, stdout=PIPE, stderr=STDOUT)
        else:
            result: CompletedProcess = run(params, encoding=io_enc, capture_output=True)
        if (result.returncode or result.stderr) and not (bool(self.expected_stderr) or combine_output):
            self._node.add_report_section('call', 'gbak stdout', result.stdout)
            self._node.add_report_section('call', 'gbak stderr', result.stderr)
            raise ExecutionError("gbak execution failed\n\n" + result.stderr)
        self.return_code: int = result.returncode
        self.stdout: str = result.stdout
        self.stderr: str = result.stderr
        # Store output
        if _vars_['save-output']:
            if self.stdout:
                out_file.write_text(self.stdout, encoding=io_enc)
            if self.stderr:
                err_file.write_text(self.stderr, encoding=io_enc)
    def nbackup(self, *, switches: List[str], charset: Optional[str]=None,
                io_enc: Optional[str]=None, credentials: bool=True, combine_output: bool=False) -> None:
        """Run `nbackup` utility.

        Arguments:
           switches: List with command-line switches (passed to `subprocess.run`).
           charset: Firebird CHARACTER SET name.
           io_enc: Python encoding to be used to decode text output instead encoding that
              corresponds to used character set.
           credentials: When `True` adds switches to connect as primary test database user.
           combine_output: Combine stdout/stderr into stdout.

        By default, NBACKUP is executed on primary test database, and output is captured into
        `stdout` and `stderr`, and exit code is stored to `return_code`.

        If pytest `--save-output` command-line option is used, the content of stdout/stderr
        is also stored into protocol files with `.out`/`.err` suffix.

        Example::

           act.nbackup(switches=['-l', act.db.dsn])
           act.nbackup(switches=['-l', act.db.dsn, '-u', test_user.name, '-p', test_user.password],
                       credentials=False)

        .. important::

           If `return_code` is not zero, and script execution failure is not expected (either
           via defined `expected_stderr` value or using `combine_output` = True), it raises and
           `ExecutionError` with "nbackup execution failed" message, and adds content of stdout
           and stderr into test run report.
        """
        __tracebackhide__ = True
        out_file: Path = self.outfile.with_suffix('.out')
        err_file: Path = self.outfile.with_suffix('.err')
        if out_file.is_file():
            out_file.unlink()
        if err_file.is_file():
            err_file.unlink()
        #
        charset = charset.upper() if charset else self.db.charset
        if io_enc is None:
            io_enc = CHARSET_MAP[charset]
        params = [_vars_['nbackup']]
        params.extend(switches)
        if credentials:
            params.extend(['-user', self.db.user, '-password', self.db.password])
        if combine_output:
            result: CompletedProcess = run(params, encoding=io_enc, stdout=PIPE, stderr=STDOUT)
        else:
            result: CompletedProcess = run(params, encoding=io_enc, capture_output=True)
        if (result.returncode or result.stderr) and not (bool(self.expected_stderr) or combine_output):
            self._node.add_report_section('call', 'nbackup stdout', result.stdout)
            self._node.add_report_section('call', 'nbackup stderr', result.stderr)
            raise ExecutionError("nbackup execution failed\n\n" + result.stderr)
        self.return_code: int = result.returncode
        self.stdout: str = result.stdout
        self.stderr: str = result.stderr
        # Store output
        if _vars_['save-output']:
            if self.stdout:
                out_file.write_text(self.stdout, encoding=io_enc)
            if self.stderr:
                err_file.write_text(self.stderr, encoding=io_enc)
    def gfix(self, *, switches: Optional[List[str]]=None, charset: Optional[str]=None,
             io_enc: Optional[str]=None, credentials: bool=True, combine_output: bool=False) -> None:
        """Run `gfix` utility.

        Arguments:
           switches: List with command-line switches (passed to `subprocess.run`).
           charset: Firebird CHARACTER SET name.
           io_enc: Python encoding to be used to decode text output instead encoding that
              corresponds to used character set.
           credentials: When `True` adds switches to connect as primary test database user.
           combine_output: Combine stdout/stderr into stdout.

        By default, GFIX is executed on primary test database, and output is captured into
        `stdout` and `stderr`, and exit code is stored to `return_code`.

        If pytest `--save-output` command-line option is used, the content of stdout/stderr
        is also stored into protocol files with `.out`/`.err` suffix.

        Example::

           act.gfix(switches=['-sweep', act.db.dsn])
           act.gfix(switches=['-user', test_user.name, '-password', test_user.password,
                              '-sweep', act.db.dsn], credentials=False)

        .. important::

           If `return_code` is not zero, and script execution failure is not expected (either
           via defined `expected_stderr` value or using `combine_output` = True), it raises and
           `ExecutionError` with "gfix execution failed" message, and adds content of stdout
           and stderr into test run report.
        """
        __tracebackhide__ = True
        out_file: Path = self.outfile.with_suffix('.out')
        err_file: Path = self.outfile.with_suffix('.err')
        if out_file.is_file():
            out_file.unlink()
        if err_file.is_file():
            err_file.unlink()
        #
        charset = charset.upper() if charset else self.db.charset
        if io_enc is None:
            io_enc = CHARSET_MAP[charset]
        params = [_vars_['gfix']]
        if switches is not None:
            params.extend(switches)
        if credentials:
            params.extend(['-user', self.db.user, '-password', self.db.password])
        if combine_output:
            result: CompletedProcess = run(params, encoding=io_enc, stdout=PIPE, stderr=STDOUT)
        else:
            result: CompletedProcess = run(params, encoding=io_enc, capture_output=True)
        if (result.returncode or result.stderr) and not (bool(self.expected_stderr) or combine_output):
            self._node.add_report_section('call', 'gfix stdout', result.stdout)
            self._node.add_report_section('call', 'gfix stderr', result.stderr)
            raise ExecutionError("gfix execution failed\n\n" + result.stderr)
        self.return_code: int = result.returncode
        self.stdout: str = result.stdout
        self.stderr: str = result.stderr
        # Store output
        if _vars_['save-output']:
            if self.stdout:
                out_file.write_text(self.stdout, encoding=io_enc)
            if self.stderr:
                err_file.write_text(self.stderr, encoding=io_enc)
    def isql(self, *, switches: Optional[List[str]]=None, charset: Optional[str]=None,
             io_enc: Optional[str]=None, input: Optional[str]=None,
             input_file: Optional[Path]=None, connect_db: bool=True,
             credentials: bool=True, combine_output: bool=False,
             use_db: Optional[Database]=None) -> None:
        """Run `isql` utility.

        Arguments:
           switches: List with command-line switches (passed to `subprocess.run`).
           charset: Firebird CHARACTER SET name.
           io_enc: Python encoding to be used to decode text output instead encoding that
              corresponds to used character set.
           input: String passed to isql via stdin.
           input_file: Path to isql input file (passed with -i switch).
           connect_db: When `True`, database DSN is added to isql switches.
           credentials: When `True` adds switches to connect as primary test database user.
           combine_output: Combine stdout/stderr into stdout.
           use_db: Database to be used instead primary test database.

        By default, isql is executed on test database with `Database` charset, and output
        is captured into `stdout` and `stderr`, and exit code is stored to `return_code`.

        If pytest `--save-output` command-line option is used, the content of stdout/stderr
        is also stored into protocol files with `.out`/`.err` suffix.

        Example::

           act.isql(switches=[], input="show version;")
           act.isql(switches=['-u', test_user.name, '-p', test_user.password],
                    credentials=False, use_db=another_db)

        .. important::

           If `return_code` is not zero, and script execution failure is not expected (either
           via defined `expected_stderr` value or using `combine_output` = True), it raises and
           `ExecutionError` with "ISQL execution failed" message, and adds content of stdout
           and stderr into test run report.
        """
        __tracebackhide__ = True
        out_file: Path = self.outfile.with_suffix('.out')
        err_file: Path = self.outfile.with_suffix('.err')
        if out_file.is_file():
            out_file.unlink()
        if err_file.is_file():
            err_file.unlink()
        #
        db = self.db if use_db is None else use_db
        charset = charset.upper() if charset else db.charset
        params = [_vars_['isql'], '-ch', charset]
        if io_enc is None:
            io_enc = CHARSET_MAP[charset]
        if credentials:
            params.extend(['-user', db.user, '-password', db.password])
        if switches is not None:
            params.extend(switches)
        if input_file is not None:
            params.extend(['-i', str(input_file)])
        if connect_db:
            params.append(str(db.dsn))
        if combine_output:
            result: CompletedProcess = run(params, input=input,
                                           encoding=io_enc, stdout=PIPE, stderr=STDOUT)
        else:
            result: CompletedProcess = run(params, input=input,
                                           encoding=io_enc, capture_output=True)
        if (result.returncode or result.stderr) and not (bool(self.expected_stderr) or combine_output):
            self._node.add_report_section('call', 'ISQL stdout', result.stdout)
            self._node.add_report_section('call', 'ISQL stderr', result.stderr)
            raise ExecutionError("ISQL execution failed\n\n" + result.stderr)
        self.return_code: int = result.returncode
        self.stdout: str = result.stdout
        self.stderr: str = result.stderr
        # Store output
        if _vars_['save-output']:
            if self.stdout:
                out_file.write_text(self.stdout, encoding=io_enc)
            if self.stderr:
                err_file.write_text(self.stderr, encoding=io_enc)
    def svcmgr(self, *, switches: Optional[List[str]]=None, charset: Optional[str]=None,
               io_enc: Optional[str]=None, connect_mngr: bool=True) -> None:
        """Run `fbsvcmgr` utility.

        Arguments:
           switches: List with command-line switches (passed to `subprocess.run`).
           charset: Firebird CHARACTER SET name.
           io_enc: Python encoding to be used to decode text output instead encoding that
              corresponds to used character set.
           connect_mngr: When `True`, adds switches to connect service manager of tested server
             using credentials for primary test database.

        By default, primary test database charset is used to determine the encoding for strings,
        and output is captured into `stdout` and `stderr`, and exit code is stored to
        `return_code`.

        If pytest `--save-output` command-line option is used, the content of stdout/stderr
        is also stored into protocol files with `.out`/`.err` suffix.

        Example::

           act.svcmgr(switches=['info_server_version'])
           act.svcmgr(switches=['service_mgr', 'user', test_user.name, 'password', test_user.password,
                                'info_server_version'], connect_mngr=False)

        .. important::

           If `return_code` is not zero, and script execution failure is not expected
           (`expected_stderr` value is not defined), it raises and `ExecutionError` with
           "fbsvcmgr execution failed" message, and adds content of stdout and stderr into
           test run report.
        """
        __tracebackhide__ = True
        out_file: Path = self.outfile.with_suffix('.out')
        err_file: Path = self.outfile.with_suffix('.err')
        if out_file.is_file():
            out_file.unlink()
        if err_file.is_file():
            err_file.unlink()
        #
        charset = charset.upper() if charset else self.db.charset
        if io_enc is None:
            io_enc = CHARSET_MAP[charset]
        params = [_vars_['fbsvcmgr']]
        if connect_mngr:
            params.extend([f"{_vars_['host']}:service_mgr" if _vars_['host'] else 'service_mgr',
                           'user', self.db.user, 'password', self.db.password])
        if switches is not None:
            params.extend(switches)
        result: CompletedProcess = run(params, encoding=io_enc, capture_output=True)
        if (result.returncode or result.stderr) and not bool(self.expected_stderr):
            self._node.add_report_section('call', 'fbsvcmgr stdout', result.stdout)
            self._node.add_report_section('call', 'fbsvcmgr stderr', result.stderr)
            raise ExecutionError("fbsvcmgr execution failed\n\n" + result.stderr)
        self.return_code: int = result.returncode
        self.stdout: str = result.stdout
        self.stderr: str = result.stderr
        # Store output
        if _vars_['save-output']:
            if self.stdout:
                out_file.write_text(self.stdout, encoding=io_enc)
            if self.stderr:
                err_file.write_text(self.stderr, encoding=io_enc)
    def connect_server(self, *, user: Optional[str | User]=None, password: Optional[str]=None,
                       role: Optional[str | Role]=None, encoding: Optional[str]=None,
                       encoding_errors: Optional[str]=None) -> Server:
        """Returns `~firebird.driver.Server` instance connected to service manager of tested
        server.

        Arguments:
          user: User name [Default taken from server configuration].
          password: User password [Default taken from server configuration].
          role: User role.
          encoding: Encoding for service output override.
          encoding_errors: Error handler for service output encoding override.

        .. tip::

           It's highly recommended to use the :ref:`with <with>` statement to ensure that
           the returned `~firebird.driver.core.Server` is properly closed even if test
           fails or raises an error.
        """
        __tracebackhide__ = True
        if isinstance(user, User):
            user = user.name
            if password is None:
                password = user.password
        if isinstance(role, Role):
            role = role.name
        return connect_server(_vars_['server'],
                              user=_vars_['user'] if user is None else user,
                              password=_vars_['password'] if password is None else password,
                              role=role, encoding=encoding, encoding_errors=encoding_errors)
    def get_firebird_log(self) -> List[str]:
        """Returns content of Firebird server log.
        """
        __tracebackhide__ = True
        with self.connect_server() as srv:
            srv.info.get_log()
            return srv.readlines()
    def is_version(self, version_spec: str) -> bool:
        """Returns `True` if version of tested server matches the version specification.

        Arguments:
          version_spec: Version specification that conforms to :pep:`440`.
        """
        spec = SpecifierSet(version_spec)
        return _vars_['version'] in spec
    def get_server_architecture(self) -> str:
        """Returns server architecture: `SuperServer`, `Classic`, `SuperClassic` or `Embedded`.
        """
        return _vars_['server-arch']
    def get_dsn(self, filename: Union[str, Path], protocol: Optional[NetProtocol]=None) -> str:
        """Returns DSN for connection to database.

        Arguments:
          filename: Database filename.
          protocol: Network protocol to be used for connection.
        """
        return _connect_helper('', self.host, self.port, str(filename),
                               protocol if protocol is not None else self.protocol)
    def reset(self) -> None:
        """Reset internal variables used to store output from execution of external tool.

        .. important::

           It's necessary to call this method between executions of external tools through
           `.isql()`, `.execute()`, `.extract_meta()`, `.gstat()`, `.gfix()`, `.gsec()`,
           `.gbak()`, `.nbackup` or `.svcmgr()`.
        """
        self.return_code: int = 0
        self._clean_stdout = None
        self._clean_stderr = None
        self._clean_expected_stdout = None
        self._clean_expected_stderr = None
        #
        self.expected_stdout = ''
        self.expected_stderr = ''
        self.stdout = ''
        self.stderr = ''
    def print_data(self, cursor: Cursor) -> None:
        """Print data from cursor in tabular format, like standard printout in ISQL.
        """
        # Print a header.
        for fieldDesc in cursor.description:
            print (fieldDesc[DESCRIPTION_NAME].ljust(fieldDesc[DESCRIPTION_DISPLAY_SIZE]),end=' ')
        print('')
        for fieldDesc in cursor.description:
            print ("-" * max((len(fieldDesc[DESCRIPTION_NAME]),fieldDesc[DESCRIPTION_DISPLAY_SIZE])),end=' ')
        print('')
        # For each row, print the value of each field left-justified within
        # the maximum possible width of that field.
        fieldIndices = range(len(cursor.description))
        for row in cursor:
            for fieldIndex in fieldIndices:
                fieldValue = row[fieldIndex]
                if not isinstance(fieldValue, str):
                    fieldValue = str(fieldValue)
                fieldMaxWidth = max((len(cursor.description[fieldIndex][DESCRIPTION_NAME]),cursor.description[fieldIndex][DESCRIPTION_DISPLAY_SIZE]))
                print (fieldValue.ljust(fieldMaxWidth), end=' ')
            print('')
    def print_data_list(self, cursor: Cursor, *, prefix: str='') -> None:
        """Print data from cursor in list format, like list printout in ISQL.
        """
        for row in cursor:
            i = 0
            for fieldDesc in cursor.description:
                print(f'{prefix}{fieldDesc[DESCRIPTION_NAME].ljust(32)}{row[i]}')
                i += 1
            print()
    def trace(self, *, db_events: Optional[List[str]]=None, svc_events: Optional[List[str]]=None,
              config: Optional[List[str]]=None, keep_log: bool=True,
              encoding: Optional[str]=None, encoding_errors: Optional[str]=None,
              database: Optional[str]=None, user: Optional[str | User]=None,
              password: Optional[str]=None, role: Optional[str | Role]=None) -> TraceSession:
        """Run Firebird trace session.

        Arguments:
          db_events: List of database events to be traced.
          svc_events: List of service events to be traced.
          config: Full trace config.
          keep_log: When `False`, the trace session output is discarded. Otherwise is stored
            in `.trace_log`.
          encoding: Encoding used to decode trace session output returned by server.
          encoding_errors: Handler for encoding errors used to decode trace session output returned by server.
          database: Traced database filename specification. If value is not specified, uses
            primary test database.

        .. note::

           There are two (mutually exclusive) methods how to configure the trace session:

           1. Using `db_events` and/or `svc_events` lists, and optional `database` specification.
           2. Using `config`.

        .. important::

           It's absolutely necessary to use the :ref:`with <with>` statement to manage the
           trace session!

           Example::

             with act.trace(db_events=['log_connections = true']):
                 # Actions that would be traced
                 act.execute()
             # Actions that are not traced
        """
        if isinstance(user, User):
            user = user.name
            if password is None:
                password = user.password
        if isinstance(role, Role):
            role = role.name
        if config is not None:
            return TraceSession(self, config, user=_vars_['user'] if user is None else user,
                                password=_vars_['password'] if password is None else password,
                                role=role, keep_log=keep_log, encoding=encoding,
                                encoding_errors=encoding_errors)
        else:
            config = []
            if db_events:
                database = self.db.db_path.name if database is None else database
                config.extend([f'database=%[\\\\/]{database}', '{', 'enabled = true',
                               'log_initfini = false'])
                config.extend(db_events)
                config.append('}')
            if svc_events:
                config.extend(['services', '{', 'enabled = true', 'log_initfini = false'])
                config.extend(svc_events)
                config.append('}')
            return TraceSession(self, config, user=_vars_['user'] if user is None else user,
                                password=_vars_['password'] if password is None else password,
                                role=role, keep_log=keep_log, encoding=encoding,
                                encoding_errors=encoding_errors)
    def trace_to_stdout(self, *, upper: bool=False) -> None:
        """Copy trace session log to `.stdout`.

        Arguments:
          upper: Convert trace log to uppercase.
        """
        log = ''.join(self.trace_log)
        self.stdout = log.upper() if upper else log
    def envar(self, name: str, value: str) -> Envar:
        """Temporary set an environment variable.

        Arguments:
          name: Variable name.
          value: Variable value.

        .. important::

           It's absolutely necessary to use the :ref:`with <with>` statement to manage the
           environment variable!

           Example::

             with act.envar(name='ISC_PASSWORD', test_user.password):
                 act.isql(switches=[], input='show database;', credentials=False)
        """
        return Envar(name, value)
    def match_any(self, line: str, patterns: Sequence) -> bool:
        """Helper function that `search` for patterns in string. Returns `True` is any pattern
        is found.

        Arguments:
          line: String to be searched.
          patterns: Sequence of compiled `re` patterns.
        """
        for pattern in patterns:
            if pattern.search(line):
                return True
        return False
    def print_callback(self, line: str) -> None:
        """Helper function that could be passed as callback to Firebird services.

        Example::

            with act.connect_server() as srv:
                srv.database.get_statistics(database=act.db.db_path, tables=['TEST_01'],
                                            flags=SrvStatFlag.DATA_PAGES | SrvStatFlag.IDX_PAGES,
                                            callback=act.print_callback)
            act.stdout = capsys.readouterr().out

        """
        print(line, end='')
    def get_config(self, key: str, database: str='employee') -> Optional[str]:
        """Returns value for specified key from RDB$CONFIG table.

        Arguments:
           key: Configuration option name.
           database: Database name.

        Database name is passed to `~firebird.driver.connect`. By default, it uses `employee`
        database. To get database-specific configuration values, you should always specify
        the `database` value using `Database.config_name`.

        Example::

          if 'Legacy_Auth' not in act.get_config('AuthServer',act.db.config_name):
              pytest.skip('Legacy_Auth required')

        .. important::

           This function work only with Firebird 4+ !
        """
        with connect(database) as con:
            c = con.cursor()
            row = c.execute('select RDB$CONFIG_VALUE from rdb$config where upper(RDB$CONFIG_NAME) = ?', [key.upper()]).fetchone()
        return row[0] if row else None
    @property
    def script(self) -> str:
        """Test script that should be executed by `~Action.execute`.
        """
        return self._script
    @script.setter
    def script(self, value: str) -> None:
        self._script = substitute_macros(value, self.db.subs)
    @property
    def clean_stdout(self) -> str:
        """Content of `stdout` passed through `.clean_string()` [R/O].

        .. important::

           Value is cached once evaluated. Use `.reset()` before new `.stdout` value is assigned
           to force re-evaluation.
        """
        if self._clean_stdout is None:
            self._clean_stdout = self.clean_string(self.stdout, self.substitutions)
        return self._clean_stdout
    @property
    def clean_stderr(self) -> str:
        """Content of `stderr` passed through `.clean_string()` [R/O].

        .. important::

           Value is cached once evaluated. Use `.reset()` before new `.stderr` value is
           assigned to force re-evaluation.
        """
        if self._clean_stderr is None:
            self._clean_stderr = self.clean_string(self.stderr, self.substitutions)
        return self._clean_stderr
    @property
    def clean_expected_stdout(self) -> str:
        """Content of `expected_stdout` passed through `.clean_string()` [R/O].

        .. important::

           Value is cached once evaluated. Use `.reset()` before new `.expected_stdout` value
           is assigned to force re-evaluation.
        """
        if self._clean_expected_stdout is None:
            self._clean_expected_stdout = self.clean_string(self.expected_stdout, self.substitutions)
        return self._clean_expected_stdout
    @property
    def clean_expected_stderr(self) -> str:
        """Content of `expected_stderr` passed through `.clean_string()` [R/O].

        .. important::

           Value is cached once evaluated. Use `.reset()` before new `.expected_stderr` value
           is assigned to force re-evaluation.
        """
        if self._clean_expected_stderr is None:
            self._clean_expected_stderr = self.clean_string(self.expected_stderr, self.substitutions)
        return self._clean_expected_stderr
    @property
    def vars(self) -> Dict[str]:
        "Dictionary with various configuration parameters and variables used by plugin."
        return _vars_
    @property
    def host(self) -> str:
        "Host machine of tested Firebird server."
        return _vars_['host']
    @property
    def port(self) -> str:
        "Port of tested Firebird server."
        return _vars_['port']
    @property
    def protocol(self) -> Optional[NetProtocol]:
        "Default protocol for connections to databases."
        return _vars_['protocol']
    @property
    def security_db(self) -> str:
        "Path to Firebird security database."
        return _vars_['security-db']
    @property
    def home_dir(self) -> Path:
        "Path to Firebird home directory."
        return _vars_['home-dir']
    @property
    def bin_dir(self) -> Path:
        "Path to directory with Firebird utilities."
        return _vars_['bin-dir']
    @property
    def files_dir(self) -> Path:
        "Path to directory with test suite data files."
        return _vars_['files']
    @property
    def platform(self) -> str:
        "Current execution platform identifier."
        return _platform

def isql_act(db_fixture_name: str, script: str='', *, substitutions: Optional[Substitutions]=None):
    """Factory function that returns :doc:`fixture <pytest:explanation/fixtures>` providing
    the `Action` instance.

    Arguments:
        db_fixture_name: Name of database fixture.
        script: SQL script that tests the server.
        substitutions: List of additional substitution for stdout/stderr.
    """

    @pytest.fixture
    def isql_act_fixture(request: pytest.FixtureRequest) -> Action:
        db: Database = request.getfixturevalue(db_fixture_name)
        f: Path = Path.cwd() / 'out' / request.module.__name__.replace('.', '/')
        if _vars_['save-output'] and not f.parent.exists():
            f.parent.mkdir(parents=True)
        f = f.with_name(f'{f.stem}-{request.function.__name__}.out')
        result: Action = Action(db, script, substitutions, f, request.node)
        return result

    return isql_act_fixture

python_act = isql_act

def temp_file(filename: Union[str, Path]):
    """Factory function that returns :doc:`fixture <pytest:explanation/fixtures>` providing
    the `Path` to temporary file.

    Arguments:
        filename: File name.

    .. note::

       Created fixture has only one purpose: if specified temporary file exists at fixture
       `teardown`, it is removed.
    """

    @pytest.fixture
    def temp_file_fixture(tmp_path) -> Path:
        tmp_file = tmp_path / filename
        yield tmp_file
        if tmp_file.is_file():
            tmp_file.unlink()

    return temp_file_fixture

def temp_files(filenames: List[Union[str, Path]]):
    """Factory function that returns :doc:`fixture <pytest:explanation/fixtures>` providing
    the list of `Path` instances to temporary files.

    Arguments:
        filenames: List of file names.

    .. note::

       Created fixture has only one purpose: if any from specified temporary file exists
       at fixture `teardown`, it is removed.
    """

    @pytest.fixture
    def temp_files_fixture(tmp_path) -> List[Path]:
        tmp_files = []
        for filename in filenames:
            tmp_files.append(tmp_path / filename)
        yield tmp_files
        for tmp_file in tmp_files:
            if tmp_file.is_file():
                tmp_file.unlink()

    return temp_files_fixture

@dataclass
class ConfigManagerBackup:
    action: str
    config_file: Path
    backup_file: Path = None

class ConfigManager:
    """Object to replace specified server configuration file.

    Arguments:
        tmp_path: Path to directory where backup will be stored.
        old_config: Old config file which will be keeped in backup (e.g. databases.conf)

    .. important::

        Do not create instances of this class directly! Use **only** fixtures created by `store_config`.
    """

    def __init__(self, tmp_path: Path):
        self.__tmp_path = tmp_path
        self.__bak_configs: Dict[str, ConfigManagerBackup] = {}

    def _backup(self, config_name: str):
        old_config = _vars_['home-dir'] / config_name
        if config_name in self.__bak_configs:
            return old_config
        if old_config.exists():
            backup = self.__tmp_path / (config_name.replace('/', '_') + '.bak')
            if backup.exists():
                backup.unlink()
            shutil.copy(str(old_config), str(backup))
            self.__bak_configs[config_name] = ConfigManagerBackup('replace', old_config, backup)
        else:
            self.__bak_configs[config_name] = ConfigManagerBackup('delete', old_config)
        return old_config

    def replace(self, config_name: str, new_config: Union[Path, str]):
        """
            config_name: Relative path to config in server
            new_config: Path to new config or content of config
        """
        old_config = self._backup(config_name)
        if isinstance(new_config, Path):
            shutil.copy(str(new_config), str(old_config))
        else:
            with open(old_config, 'w') as old:
                old.write(new_config)

    def add(self, config_name: str, new_config: Union[Path, str]):
        """
            config_name: Relative path to config in server
            new_config: Path to new config or content of config
        """
        old_config = self._backup(config_name)
        new_content = new_config.read_text() if isinstance(new_config, Path) else new_config
        with open(old_config, 'a') as old:
            old.write(new_content)

    def restore(self, final=False):
        for backup in self.__bak_configs.values():
            if backup.action == 'replace':
                shutil.copy(str(backup.backup_file), str(backup.config_file))
                if final:
                    backup.backup_file.unlink()
            elif backup.action == 'delete':
                backup.config_file.unlink()
        self.__bak_configs = {}

@pytest.fixture
def store_config(db_path) -> ConfigManager:
    manager = ConfigManager(db_path)
    yield manager
    manager.restore(final=True)
