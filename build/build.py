#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
#
# Copyright Saab AB, 2009-2014 (http://safirsdkcore.com)
#
# Created by: Lars Hagstrom / lars.hagstrom@consoden.se
#
###############################################################################
#
# This file is part of Safir SDK Core.
#
# Safir SDK Core is free software: you can redistribute it and/or modify
# it under the terms of version 3 of the GNU General Public License as
# published by the Free Software Foundation.
#
# Safir SDK Core is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Safir SDK Core.  If not, see <http://www.gnu.org/licenses/>.
#
###############################################################################
"""Build script for safir-sdk-core. Check the help to get some hints on what it can do."""
import os
import glob
import sys
import subprocess
import platform
import xml.dom.minidom
import re
import time
import shutil
import argparse
import codecs
from xml.sax.saxutils import escape

from os.path import join, isfile, isdir
import itertools

try:
    from shutil import which
except:
    from distutils.spawn import find_executable as which

if platform.system() == 'Windows':
    try:
        import winreg
    except:
        import _winreg as winreg
    from os import environ
else:
    # Mock winreg and environ so the module can be imported on this platform.

    class winreg:
        HKEY_USERS = None
        HKEY_CURRENT_USER = None
        HKEY_LOCAL_MACHINE = None
        HKEY_CLASSES_ROOT = None

    environ = {}


#Make linux_distribution available from some suitable package, falling back to returning that we don't know.
try:
    from distro import linux_distribution
except:
    try:
        from platform import linux_distribution
    except:
        def linux_distribution():
            return ("unknown",)

#a few constants
known_configs = set(["Release", "Debug", "MinSizeRel", "RelWithDebInfo"])


class FatalError(Exception):
    """our own exception for "die" fcn"""


def die(message):
    """Just raise an exception with a message"""
    raise FatalError(message)


def is_64_bit():
    """Detecting this is a lot more complex than it should be.
    See http://stackoverflow.com/questions/2764356/python-get-windows-os-version-and-architecture
    and http://bytes.com/topic/python/answers/509764-detecting-64bit-vs-32bit-linux
    This will work reasonably well on our supported systems:"""
    if sys.platform.startswith("linux"):
        return platform.architecture()[0] == "64bit"
    else:
        processor_architecture = os.environ.get("PROCESSOR_ARCHITECTURE")
        processor_architew6432 = os.environ.get("PROCESSOR_ARCHITEW6432")
        return processor_architecture == "AMD64" or processor_architew6432 == "AMD64"


def mkdir(newdir):
    """works the way a good mkdir should :)
        - already exists, silently complete
        - regular file in the way, raise an exception
        - parent directory(ies) does not exist, make them as well
    """
    if os.path.isdir(newdir):
        pass
    elif os.path.isfile(newdir):
        raise OSError("a file with the same name as the desired dir, '{}', already exists.".format(newdir))
    else:
        head, tail = os.path.split(newdir)
        if head and not os.path.isdir(head):
            mkdir(head)
        if tail:
            os.mkdir(newdir)


def remove(path):
    """Remove a file or directory recursively"""
    if not os.path.exists(path):
        return
    if os.path.isfile(path) or os.path.islink(path):
        try:
            os.remove(path)
            return
        except OSError as exc:
            die("Failed to remove file {}. Got exception {}".format(path,str(exc)))

    for name in os.listdir(path):
        if os.path.isdir(os.path.join(path, name)):
            remove(os.path.join(path, name))
        else:
            try:
                os.remove(os.path.join(path, name))
            except OSError as exc:
                die("Failed to remove file " + os.path.join(path, name) + ". Got exception " + str(exc))

    try:
        os.rmdir(path)
    except OSError as exc:
        die("Failed to remove directory " + path + ". Got exception " + str(exc))


def num_cpus():
    """Detects the number of CPUs on a system."""
    # Linux, Unix and MacOS:
    if hasattr(os, "sysconf"):
        if "SC_NPROCESSORS_ONLN" in os.sysconf_names:
            # Linux & Unix:
            ncpus = os.sysconf("SC_NPROCESSORS_ONLN")
            if isinstance(ncpus, int) and ncpus > 0:
                return ncpus
    # Windows:
    if "NUMBER_OF_PROCESSORS" in os.environ:
        ncpus = int(os.environ["NUMBER_OF_PROCESSORS"])
        if ncpus > 0:
            return ncpus
    return 1  # Default


def physical_memory():
    """Detect physical memory in computer"""
    if sys.platform.startswith("linux"):
        with open("/proc/meminfo", "rb") as a_file:
            meminfo = a_file.read().decode("ascii")
        match = re.search(r"MemTotal:\s*([0-9]*) kB", meminfo)
        return int(match.group(1)) / 1024
    else:
        return None


def num_jobs():
    """We need to limit ourselves a little bit in how many parallel jobs we perform. Each job may use
    up to 400Mb of memory."""
    try:
        num = num_cpus() + 1

        mem_per_job = 400
        memory = physical_memory()
        if memory is not None and memory / num < mem_per_job:
            num = max(1, int(memory / mem_per_job))
    except:  # pylint: disable=bare-except
        num = 2
    return num


def read_version():
    """Parse the VERSION.txt file to find out our version"""
    parts = {}
    with open("VERSION.txt", 'r') as version_file:
        for line in version_file:
            line = line.strip()
            if len(line) == 0 or line.startswith("#"):
                continue
            key, value = line.split("=")
            parts[key] = value
    return ((parts["MAJOR"], parts["MINOR"], parts["PATCH"], parts["SUFFIX"]),
            parts["MAJOR"] + "." + parts["MINOR"] + "." + parts["PATCH"] + parts["SUFFIX"])


class DummyLogger():
    """A logger that can be used until we know what actual logger should be used.
    It only logs to stdout"""
    @staticmethod
    def log(data, tag=None):
        """Log some data, to stdout in this case"""
        sys.stdout.write("{}{}\n".format((tag+': ') if tag is not None else '',data))
        sys.stdout.flush()

    @staticmethod
    def close():
        """Close the logger"""

    @staticmethod
    def log_output(process):
        """This logger cant actually do this"""
        raise Exception("DummyLogger doesnt support process output logging. " +
                        "You should investigate why the real logger is not instantiated by now...")


class Logger():
    """The main logger"""
    LogLevel = ("Brief", "Verbose")
    Tags = set(["header", "brief", "normal", "detail", "command_description", "command", "output"])

    def __init__(self, level):
        if level not in Logger.LogLevel:
            die("Bad log level")
        self.__log_level = level
        self.__last_tag = None
        if hasattr(sys.stdout, "reconfigure"):
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        self.__buildlog = codecs.open("buildlog.html", mode="w", encoding="utf-8", errors="replace")
        self.__buildlog.write("<html><head>"
                              "<script type=\"text/javascript\">"
                              "function refreshPage () {"
                              "var page_y = document.getElementsByTagName(\"body\")[0].scrollTop;"
                              "window.location.href = window.location.href.split('?')[0] + '?page_y=' + page_y;"
                              "}"
                              "window.onload = function () {"
                              "setTimeout(refreshPage, 10000);"
                              "if ( window.location.href.indexOf('page_y') != -1 ) {"
                              "var match = window.location.href.split('?')[1].split(\"&\")[0].split(\"=\");"
                              "document.getElementsByTagName(\"body\")[0].scrollTop = match[1];"
                              "}"
                              "}"
                              "</script>"
                              "<title>Safir SDK Core Build Log</title>")
        self.__buildlog.write("<body>\n")
        self.__buildlog.write("<h1>Safir SDK Core Build Log</h1>")
        self.__buildlog.write("<b>Command line:</b> " + " ".join(sys.argv) + "<br/>")
        self.__buildlog.write("<b>Working directory:</b> " + os.getcwd() + "<br/>")
        self.__buildlog.write("<b>Start time (local time)</b>: " + time.asctime() + "<br/>")
        self.__buildlog.write("<h2>Starting build</h2>\n")

    def close(self):
        """Close the log"""
        self.__buildlog.write("\n<p/>End time (local time): " + time.asctime())
        self.__buildlog.write("\n</body>\n")
        self.__buildlog.close()

    @staticmethod
    def __print(data):
        try:
            sys.stdout.write(data)
            sys.stdout.write("\n")
            sys.stdout.flush()
        except UnicodeEncodeError:
            sys.stdout.write("Failed to decode something in data to be printed. Sorry.\n")
            sys.stdout.flush()

    def __log_stdout(self, data, tag):
        if tag not in Logger.Tags:
            die("unknown logging tag")
        if self.__log_level == "Brief":
            if tag in ("header", "normal", "brief"):
                self.__print(data)
        elif self.__log_level == "Verbose":
            if tag == "brief":
                pass
            elif tag == "header":
                self.__print("\n==== " + data + " ====")
            elif tag == "command_description":
                self.__print("+ " + data + ": ")
            elif tag == "command":
                self.__print("'" + data + "'")
            else:
                self.__print(data)

    def __log_file(self, data, tag):
        log = self.__buildlog

        if self.__last_tag == "output" and self.__last_tag != tag:
            log.write("</pre>\n")

        if tag == "header":
            log.write("<h3>" + data + "</h3>\n")
        elif tag == "brief":
            pass
        elif tag == "normal":
            log.write(data + "<br/>\n")
        elif tag == "detail":
            log.write(data + "<br/>\n")
        if tag == "command_description":
            log.write("<h4>" + data + "</h4>\n")
        elif tag == "command":
            log.write("<pre style=\"color: green\">" + data + "</pre>\n")
        elif tag == "output":
            if self.__last_tag != tag:
                log.write("<pre>" + data)
            else:
                log.write("\n" + data)
        log.flush()
        self.__last_tag = tag

    def log(self, data, tag="normal"):
        """log some data"""
        if data is None:
            return

        self.__log_stdout(data, tag)
        self.__log_file(data, tag)

    def log_output(self, process):
        """log the output of a process"""
        output = []
        while True:
            line = process.stdout.readline()
            if not line:
                break
            #CMake does some strange thing with a carriage return alone on a line, which we get rid of like this.
            line = line.decode("utf8", errors="replace").rstrip("\r")
            if len(line) != 0:
                line = line.rstrip()
                self.log(line, "output")
                output += (line, )
        process.wait()
        if process.returncode != 0:
            self.log("Failure, return code is " + str(process.returncode))
        self.log("", "output")
        return "\n".join(output)


def suppress(help_string):
    """
    Use this function to wrap help strings for arguments that should not be visible
    in the help when this script is run as dobmake-batch.
    """
    try:
        if os.path.basename(__file__).startswith("dobmake-batch"):
            return argparse.SUPPRESS
    except OSError:
        pass
    return help_string


def add_win32_options(parser):
    """add windows options to the parser"""
    parser.add_argument("--use-studio",
                        help="The visual studio to use for building (vs2015, vs2017, vs2019, vs2022)",
                        action="store")
    parser.add_argument("--arch",
                        default="amd64" if is_64_bit() else "x86",
                        choices=["x86","amd64"],
                        help="Architecture to build. Note that you may not be able to run tests if you cross-compile to an arch you can't run.")
    parser.add_argument("--32-bit",
                        action="store_true",
                        help=argparse.SUPPRESS)
    parser.add_argument("--configs",
                        default=("Debug", "RelWithDebInfo"),
                        nargs='*',
                        choices=known_configs,
                        help="The configurations to build. Debug and RelWithDebInfo is the default.")


def add_linux_options(parser):
    """Add linux opitons to the parser"""
    parser.add_argument("--config",
                        dest="configs",
                        nargs=1,
                        default=("RelWithDebInfo", ),
                        choices=known_configs,
                        help="The configuration to build. RelWithDebInfo is the default.")


def parse_command_line():
    """parse the command line"""
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    action = parser.add_mutually_exclusive_group()

    action.add_argument("--package",
                        action="store_true",
                        default=False,
                        help=suppress("Build everything and package the results for the current platform."))

    action.add_argument("--package-noclean",
                        action="store_true",
                        default=False,
                        help=suppress("Same as --package, but attempt to continue from previous build"))

    action.add_argument("--install",
                        metavar="PATH",
                        help="Build the source in the current directory and install it to "
                        "PATH. If PATH is set to 'None' the install step will be run "
                        "without setting CMAKE_INSTALL_PREFIX, useful if your "
                        "CMakeLists.txt has absolute paths in the INSTALL directives.")

    parser.add_argument("--skip-tests", action="store_true", help=suppress("Skip running the unit tests"))

    parser.add_argument("--jenkins",
                        action="store_true",
                        default=False,
                        help=suppress("Increase verbosity and obey build matrix variables."))

    parser.add_argument("--verbose",
                        "-v",
                        action="count",
                        default=0,
                        help="Print more stuff about what is going on. Use twice to get very verbose output.")

    if sys.platform.startswith("linux"):
        add_linux_options(parser)
    else:
        add_win32_options(parser)

    arguments = parser.parse_args()

    if arguments.jenkins:
        arguments.verbose += 1
    if arguments.verbose >= 2:
        os.environ["VERBOSE"] = "1"
    if arguments.package_noclean:
        arguments.package = True

    #Fix backward compatibility options for windows
    if sys.platform == "win32":
        if vars(arguments)["32_bit"]:
            arguments.arch = "x86"

        if arguments.use_studio == "2015":
            arguments.use_studio = "vs" + arguments.use_studio
        if arguments.use_studio not in ["vs2015", "vs2017", "vs2019", "vs2022"]:
            die("Unknown studio version")

    global LOGGER
    LOGGER = Logger("Brief" if arguments.verbose == 0 else "Verbose")

    return arguments


class BuilderBase(object):
    """Base class for builders"""
    def __init__(self, arguments):
        self.num_jobs = num_jobs()

        # We want Ninja for building
        if which("ninja") is None:
            die("Need ninja to build!")

        self.total_tests = 0
        self.failed_tests = 0

        self.arguments = arguments
        self.__handle_command_line_arguments()

    def __handle_command_line_arguments(self):
        self.configs = self.arguments.configs

        self.debug_only = False
        if self.arguments.jenkins:
            if os.environ.get("BUILD_TYPE") == "DebugOnly":
                LOGGER.log("Using Config 'DebugOnly', building everything in Debug only.")
                self.configs = ("Debug", )
                self.debug_only = True

        self.stagedir = os.path.join(os.getcwd(), "stage") if self.arguments.package else None

        self.install_prefix = None  #derived classes can override if arguments.package is true
        if self.arguments.install and self.arguments.install != "None":
            self.install_prefix = self.arguments.install

    def build(self):
        """Build the project"""
        for config in self.configs:
            olddir = os.getcwd()
            mkdir(config)
            os.chdir(config)

            self.__build_internal(os.pardir, config)
            os.chdir(olddir)

        if self.arguments.package:
            self.__package()

    def __package(self):
        LOGGER.log("Building installation package", "header")
        self.stage_package()

    def __configure(self, srcdir, config):

        command = ("cmake", "-G", "Ninja", "-D", "CMAKE_BUILD_TYPE:string=" + config)

        if self.install_prefix is not None:
            command += ("-D", "CMAKE_INSTALL_PREFIX=" + self.install_prefix)

        command += (srcdir, )
        self._run_command(command, "Configure for " + config + " build")

    def __build_internal(self, srcdir, config):
        LOGGER.log(" - in config " + config, "brief")

        self.__configure(srcdir, config)

        command = ("cmake", "--build", ".")

        if self.arguments.verbose >= 2:
            command += ("-v",)

        self._run_command(command, "Build " + config)
        if not self.arguments.skip_tests:
            LOGGER.log("   + testing", "brief")
            self.test()
            translate_results_to_junit(config)

        if self.arguments.package:
            LOGGER.log("   + installing to staging area", "brief")
            self.__stage_install()
        elif self.arguments.install:
            self.__install()

    def __stage_install(self):
        for component in ("Runtime", "Tools", "Development", "TestSuite"):
            command = ("cmake", "-DCOMPONENT=" + component, "-P", "cmake_install.cmake")
            env = os.environ.copy()
            env["DESTDIR"] = os.path.join(self.stagedir, component)
            self._run_command(command, "Staged install " + component, env=env)

    def __install(self):
        command = ("cmake", "-P", "cmake_install.cmake")
        self._run_command(command, "Installing to " + self.arguments.install)

    def stage_package(self):
        LOGGER.log(" ! Packaging not implemented in this builder !", "brief")

    def test(self):
        """run ctest in current directory"""
        if not os.path.isfile("DartConfiguration.tcl"):
            dummyfile = open("DartConfiguration.tcl", "w")
            dummyfile.close()

        output = self._run_command(("ctest", "--output-on-failure", "-T", "Test", "--no-compress-output"),
                                   "Test",
                                   allow_fail=True)
        self.interpret_test_output(output)

    def _run_command(self, cmd, description, allow_fail=False, env=None):
        """Run a command"""

        LOGGER.log(description, "command_description")
        LOGGER.log(" ".join(cmd), "command")

        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
        output = LOGGER.log_output(process)
        if process.returncode != 0:
            if not allow_fail:
                die("Failed to run '" + " ".join(cmd) + "' in " + os.getcwd())
            else:
                LOGGER.log("This command failed, but failure of this particular command " +
                           "is non-fatal to the build process, so I'm continuing")

        return output

    def interpret_test_output(self, output):
        LOGGER.log("Checking test output", "detail")
        match = re.search(r"tests passed, ([0-9]+) tests failed out of ([0-9]+)", output)
        if not match:
            if output.find("No tests were found") == -1:
                LOGGER.log("Failed to parse test output!")
            return
        self.total_tests += int(match.group(2))
        self.failed_tests += int(match.group(1))


class VisualStudioBuilder(BuilderBase):
    def __init__(self, arguments):
        super(VisualStudioBuilder, self).__init__(arguments)

        self.install_target = "Install"

        self.__setup_build_environment()

        #Disable msvc leak detection in boost test unit tests. This detects loads of spurious memory
        #leaks, which makes it useless.
        os.environ["BOOST_TEST_DETECT_MEMORY_LEAK"] = "0"
    @staticmethod
    def can_use():
        return sys.platform == "win32"

    @staticmethod
    def __find_vcvarsall_vc2015():
        try:
            key = winreg.OpenKey(
                winreg.HKEY_LOCAL_MACHINE,
                r"Software\Microsoft\VisualStudio\SxS\VC7",
                0,
                winreg.KEY_READ | winreg.KEY_WOW64_32KEY
            )
        except OSError:
            return None

        best_version = 0
        best_dir = None
        with key:
            for i in itertools.count():
                try:
                    v, vc_dir, vt = winreg.EnumValue(key, i)
                except OSError:
                    break
                if v and vt == winreg.REG_SZ and isdir(vc_dir):
                    try:
                        version = int(float(v))
                    except (ValueError, TypeError):
                        continue
                    if version >= 14 and version > best_version:
                        best_version, best_dir = version, vc_dir
        return best_dir

    @staticmethod
    def __find_vcvarsall_vc2017_or_later():
        root = environ.get("ProgramFiles(x86)") or environ.get("ProgramFiles")
        if not root:
            return None

        try:
            path = subprocess.check_output([
                join(root, "Microsoft Visual Studio", "Installer", "vswhere.exe"),
                "-latest",
                "-prerelease",
                "-requiresAny",
                "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-requires", "Microsoft.VisualStudio.Workload.WDExpress",
                "-property", "installationPath",
                "-products", "*",
            ]).decode(encoding="mbcs", errors="strict").strip()
        except (subprocess.CalledProcessError, OSError, UnicodeDecodeError):
            return None

        path = join(path, "VC", "Auxiliary", "Build")
        if isdir(path):
            return path

        return None

    @staticmethod
    def __find_vcvarsall():
        best_dir = VisualStudioBuilder.__find_vcvarsall_vc2017_or_later()
        version = "new"
        LOGGER.log("__find_vcvarsall_vc2017_or_later() result: " + str(best_dir))
        if not best_dir:
            best_dir = VisualStudioBuilder.__find_vcvarsall_vc2015()
            version = "old"
            LOGGER.log("__find_vcvarsall_vc2015() result: " + str(best_dir))

        if not best_dir:
            return None, None

        vcvarsall = join(best_dir, "vcvarsall.bat")
        if not isfile(vcvarsall):
            return None, None

        return vcvarsall,version


    def __run_vcvarsall(self, vcvarsall, version):
        if version == "old":
            if self.arguments.use_studio not in  ("any", "vs2015"):
                die("Could only find vs2015")
            cmd = '"{}" {} & set'.format(vcvarsall, self.arguments.arch)
        else:
            if self.arguments.use_studio == "vs2015":
                vcver = "14.0"
            elif self.arguments.use_studio == "vs2017":
                vcver = "14.1"
            elif self.arguments.use_studio == "vs2019":
                vcver = "14.2"
            elif self.arguments.use_studio == "vs2022":
                vcver = "14.4" #vc 17.10.4 or later
            cmd = '"{}" {} -vcvars_ver={} & set'.format(vcvarsall, self.arguments.arch, vcver)

        LOGGER.log("Running '" + cmd + "' to extract environment")
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        output = proc.communicate()[0]
        if proc.returncode != 0:
            die("Failed to fetch environment variables out of vcvarsall.bat: " + output)
        return output

    def __setup_build_environment(self):
        """
        Find vcvarsall.bat and load the relevant environment variables from it.  This function
        is inspired (but not copied, for licensing reasons) by the one in python's setuptools.
        """

        vcvarsall, vcvarsall_kind = self.__find_vcvarsall()

        #use uppercase only in this variable!
        required_variables = set(["LIB", "LIBPATH", "PATH", "INCLUDE", "VSINSTALLDIR"])
        optional_variables = set([
            "PLATFORM",
        ])
        wanted_variables = required_variables | optional_variables  #union

        LOGGER.log("Loading Visual Studio Environment", "header")
        output = self.__run_vcvarsall(vcvarsall, vcvarsall_kind)

        found_variables = set()

        for line in output.split("\n"):
            if '=' not in line:
                continue
            line = line.strip()
            name, value = line.split('=', 1)
            name = name.upper()
            if name in wanted_variables:
                if value.endswith(os.pathsep):
                    value = value[:-1]
                if os.environ.get(name) is None:
                    LOGGER.log("Will set '" + name + "' to '" + value + "'", "detail")
                else:
                    LOGGER.log("Will change '" + name + "' from '" + os.environ.get(name) + "' to '" + value + "'",
                               "detail")
                os.environ[name] = value
                found_variables.add(name)

        if len(required_variables - found_variables) != 0:
            die("Failed to find all expected variables in vcvarsall.bat")

        VisualStudioBuilder.__conan_tweaks(vcvarsall, vcvarsall_kind)

        #we also need to put the java 32 bit vm first in the path, if we're running an x86 build
        dont_build_java = os.getenv("SAFIR_DONT_BUILD_JAVA", 'False').lower() in ('true', '1', 't')
        if not dont_build_java and self.arguments.arch == "x86":
            for path in os.environ["PATH"].split(";"):
                candidate = os.path.join(path,"java.exe")
                if os.path.exists(candidate):
                    try:
                        res = subprocess.check_output((candidate,"-XshowSettings:properties","-version"),
                                                      stderr=subprocess.STDOUT).decode(encoding="mbcs",
                                                                                       errors="replace").strip()
                        if "sun.arch.data.model = 32" in res:
                            LOGGER.log("Will add '" + path + "' to beginning of PATH", "detail")
                            os.environ["PATH"] = path + ";" + os.environ["PATH"]
                            break
                    except subprocess.SubprocessError as e:
                        LOGGER.log("Failed to check whether JDK iss 32 or 64 bit, Java part of the build may not " +
                                   "work. Consider setting environment variable SAFIR_DONT_BUILD_JAVA to 1 to " +
                                   "avoid building java interfaces altogether.")

    @staticmethod
    def __conan_tweaks(vcvarsall, vcvarsall_kind):
        """
        Here are some tweaks to allow us to build with vc versions that are installed
        under a newer Visual Studio version. For example VS2022 can have Visual C++ 16 compilers
        installed (i.e. the compiler from VS2019).
        This is a bit messy, and is currently only tested on our build setup. So if you get
        messages from conan wanting compiler version configs this might need to be adjusted.
        """
        if not os.path.exists("conanfile.py"):
            return

        if "Microsoft Visual Studio 14.0" in vcvarsall and vcvarsall_kind == "old":
            ide_version = 14
        elif "2017" in vcvarsall and vcvarsall_kind == "new":
            ide_version = 15
        elif "2019" in vcvarsall and vcvarsall_kind == "new":
            ide_version = 16
        elif "2022" in vcvarsall and vcvarsall_kind == "new":
            ide_version = 17
        else:
            die("Unsupported Visual Studio version")
        arg = f"--conf:all tools.microsoft.msbuild:vs_version={ide_version}"
        os.environ["CONAN_EXTRA_ARGUMENTS"] = arg
        LOGGER.log(f"Setting environment variable CONAN_EXTRA_ARGUMENTS to '{arg}', " +
                   "to encourage conan packages to build correctly.")


    def stage_package(self):
        _, version_string = read_version()

        #If we're cross compiling we need to rename directories a bit.
        if is_64_bit() and self.arguments.arch == "x86":
            for base in ["Runtime", "Development", "Tools", "TestSuite"]:
                os.rename(os.path.join(self.stagedir,base,"Program Files (x86)"), os.path.join(self.stagedir,base,"Program Files"))

        #Convert arch string to nsis format
        arch = self.arguments.arch
        if self.arguments.arch == "amd64":
            arch = "x86-64"

        command = ("makensis", "/DARCH=" + arch, "/DSTUDIO=" + self.arguments.use_studio.replace("vs",""),
                   "/DVERSION=" + version_string)

        if self.debug_only:
            command += ("/DDEBUGONLY", )

        command += (os.path.join("build", "packaging", "windows", "installer.nsi"), )

        self._run_command(command, "Packaging ")


class UnixGccBuilder(BuilderBase):
    def __init__(self, arguments):
        super(UnixGccBuilder,self).__init__(arguments)

        self.install_target = "install"

    @staticmethod
    def can_use():
        return sys.platform.startswith("linux")

class DebianPackager():
    """this builder has nothing in common with the other builders, really. Which is why it does
       not inherit from BuilderBase..."""
    def __init__(self, arguments):
        self.num_jobs = num_jobs()

        #this builder doesnt support exposing test results.
        self.total_tests = -1
        self.failed_tests = -1

        self.arguments = arguments
        if len(self.arguments.configs) != 1:
            die("DebianPackager can only build one config")
        if which("conan") is None:
            die("Could not find conan executable")

        self.noclean = arguments.package_noclean and os.path.exists("tmp")

        if self.arguments.jenkins:
            if os.environ.get("BUILD_TYPE") == "DebugOnly":
                LOGGER.log("Using Config 'DebugOnly', building everything in Debug only.")
                self.arguments.configs = ("Debug", )

    @staticmethod
    def can_use():
        """Can be used on debian based distros"""
        return sys.platform.startswith("linux") and \
            linux_distribution()[0] in ("Debian GNU/Linux", "Ubuntu")

    @staticmethod
    def __run(cmd, description):
        """Run a command"""

        LOGGER.log(description, "command_description")
        LOGGER.log(" ".join(cmd), "command")

        with subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT) as process:
            output = LOGGER.log_output(process)
            if process.returncode != 0:
                die("Failed to run '" + " ".join(cmd) + "' in " + os.getcwd())

        return output

    def build(self):
        """Run the build"""
        version_string = read_version()[1]
        if not self.noclean:
            remove("tmp")
            mkdir("tmp")
            self.__run(("git", "archive",
                        "--prefix", "safir-sdk-core_" + version_string + "/",
                        "-o", "tmp/safir-sdk-core_" + version_string + ".orig.tar.gz",
                        "HEAD"), "creating tar archive")
        os.chdir("tmp")
        if not self.noclean:
            self.__run(("/bin/tar", "xfz",
                        "safir-sdk-core_" + version_string + ".orig.tar.gz"),
                       "extracting archive")
        os.chdir("safir-sdk-core_" + version_string)
        if not self.noclean:
            shutil.copytree(os.path.join("build", "packaging", "debian"), "debian")
        command = ["debuild",
                    "--prepend-path", os.path.dirname(which("conan"))]

        #set up some build options.
        options = "config=" + self.arguments.configs[0]
        if self.arguments.configs[0] == "Debug":
            options += " noopt"
        if self.arguments.skip_tests:
            options += " nocheck"
        command += ("--set-envvar", "DEB_BUILD_OPTIONS=" + options)

        #Pass along a few select environment variables to debuild, since by default no environment
        #is passed over to the build process by debuild.
        for var in ("SAFIR_SKIP_SLOW_TESTS",):
            val = os.environ.get(var)
            if val is not None:
                command += ("--set-envvar", var + "=" + val)

        command += ["-us", "-uc", "-nc"]

        self.__run(command,
                   "building packages")
        os.chdir(glob.glob("obj-*")[0])
        if not self.arguments.skip_tests:
            translate_results_to_junit("debhelper")

def getText(nodelist):
    """Get text data out of a xml dom"""
    rc = []
    for node in nodelist:
        if node.nodeType == node.TEXT_NODE:
            rc.append(node.data)
    return ''.join(rc)


def translate_results_to_junit(suite_name):
    """Translate ctest output to junit output"""
    with open(os.path.join("Testing", "TAG"), 'rb') as tag_file:
        dirname = tag_file.readline().decode("utf-8").strip()
    with open(suite_name + ".junit.xml", "w") as junitfile:
        junitfile.write("<?xml version=\"1.0\"?>\n<testsuite>\n")

        dom = xml.dom.minidom.parse(os.path.join("Testing", dirname, "Test.xml"))

        testing = dom.getElementsByTagName("Testing")[0]
        for child in testing.childNodes:
            if child.nodeType == xml.dom.Node.ELEMENT_NODE:
                if child.tagName == "Test":
                    test_name = getText(child.getElementsByTagName("Name")[0].childNodes)
                    #test_target = os.path.split(getText(child.getElementsByTagName("Path")[0].childNodes))[-1]
                    test_status = child.getAttribute("Status")
                    for meas in child.getElementsByTagName("NamedMeasurement"):
                        if meas.getAttribute("name") == "Exit Code":
                            exit_code = getText(meas.getElementsByTagName("Value")[0].childNodes)
                        if meas.getAttribute("name") == "Exit Value":
                            exit_value = getText(meas.getElementsByTagName("Value")[0].childNodes)
                        if meas.getAttribute("name") == "Execution Time":
                            execution_time = float(getText(meas.getElementsByTagName("Value")[0].childNodes))

                    meas = child.getElementsByTagName("Measurement")[0]

                    junitfile.write("  <testcase name=\"" + test_name + "\" classname=\"" + suite_name + "\" time=\"" +
                                    str(execution_time) + "\">\n")
                    output = escape(getText(meas.getElementsByTagName("Value")[0].childNodes))
                    if test_status == "passed":
                        #success
                        junitfile.write("<system-out>" + output + "\n</system-out>\n")
                    else:
                        #failure

                        junitfile.write("<error message=\"" + exit_code + "(" + exit_value + ")\">" + output +
                                        "\n</error>\n")
                    junitfile.write("  </testcase>\n")
        junitfile.write("</testsuite>")


def get_builder(arguments):
    """"Get the correct builder for platform and arguments"""
    if VisualStudioBuilder.can_use():
        return VisualStudioBuilder(arguments)
    elif arguments.package and DebianPackager.can_use():
        return DebianPackager(arguments)
    elif UnixGccBuilder.can_use():
        return UnixGccBuilder(arguments)
    else:
        die("Failed to work out what builder to use!")


def main():
    """Woo! Main function"""

    arguments = parse_command_line()
    builder = get_builder(arguments)

    builder.build()

    return (builder.total_tests, builder.failed_tests)


#### actual code starts here ####

# create a dummy logger that we use until we have the real thing
LOGGER = DummyLogger()

#reduce process priority (currently only done on unix platforms)
if hasattr(os, "nice"):
    try:
        if os.nice(0) == 0:
            result = os.nice(10)
    except OSError as exception:
        LOGGER.log("Failed to set process niceness: " + str(exception))

try:
    (tests, failed) = main()
    LOGGER.log("Result", "header")
    LOGGER.log("Build completed successfully!")
    if tests == -1:
        pass
    elif tests == 0:
        LOGGER.log("No tests were performed")
    elif failed == 0:
        LOGGER.log("All tests ran successfully!")
    else:
        LOGGER.log(str(failed) + " tests failed out of " + str(tests) + ".", "brief")
    RESULT = 0
except FatalError as exception:
    LOGGER.log("Result", "header")
    LOGGER.log("Build script failed:")
    LOGGER.log(str(exception), "output")
    LOGGER.log(str(exception), "brief")
    RESULT = 1

LOGGER.close()
sys.exit(RESULT)
