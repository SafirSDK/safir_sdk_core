#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
#
# Copyright Saab AB, 2012 (http://www.safirsdk.com)
#
# Created by: Lars Hagstrom (lars.hagstrom@consoden.se)
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
from __future__ import print_function
import subprocess, os, time, sys, re, signal

if sys.platform == "win32":
    config_type = os.environ.get("CMAKE_CONFIG_TYPE")
    exe_path = config_type if config_type else ""
else:
    exe_path = "."

sleeper_exe = os.path.join(exe_path,"sleeper")

print ("stdout isatty:", sys.stdout.isatty())
print ("stderr isatty:", sys.stderr.isatty())
print ("stdin isatty:", sys.stdin.isatty())
print()
errors = 0
 
def test_signal(reason, expectCallback = False, expectedReturncode = None):
    global errors
    print("Testing signal", str(reason) + ":")
    cf = subprocess.CREATE_NEW_PROCESS_GROUP if sys.platform == "win32" else 0
    sleeper = subprocess.Popen(sleeper_exe,
                               stdout=subprocess.PIPE, 
                               stderr=subprocess.STDOUT,
                               creationflags = cf)
    line = sleeper.stdout.readline().decode("ascii")
    if not line.startswith("Started"):
        print("Strange starting line:", line)
        errors += 1
        return

    sleeper.send_signal(reason)

    result = sleeper.communicate()[0].decode("ascii")
    if (result.find("callback") != -1) != expectCallback:
        print("CrashReporter", "didn't call callback" if expectCallback else "called callback!")
        errors += 1
        return
    if expectCallback:
        match = re.search(r"dumpPath = '(.*)'",result)
        if match is None:
            print("Failed to find dumpPath in output")
            print(result)
            errors += 1
            return
            
        dumpPath = match.group(1)
        
        if not os.path.isfile(dumpPath):
            print("No dumpfile appears to have been generated")
            print("expected to find", dumpPath)
            errors += 1
            return

    if expectedReturncode is None:
        expectedReturncode = -reason
    if sleeper.returncode != expectedReturncode:
        print("Sleeper program exited successfully (it is meant to exit with a signal!), exit code = ", sleeper.returncode)
        errors += 1
        return

        
        

if sys.platform == "win32":  
    test_signal(signal.CTRL_BREAK_EVENT, expectedReturncode = -1073741510)    
else:
    test_signal(signal.SIGHUP)
    test_signal(signal.SIGINT)
    test_signal(signal.SIGQUIT)
    test_signal(signal.SIGKILL)
    test_signal(signal.SIGALRM)
    test_signal(signal.SIGTERM)
    test_signal(signal.SIGUSR1)
    test_signal(signal.SIGUSR2)

    test_signal(signal.SIGILL, True)
    test_signal(signal.SIGSEGV, True)
    test_signal(signal.SIGFPE, True)
    test_signal(signal.SIGABRT, True)

if errors == 0:
    print("success")
else:
    print(errors, "test(s) failed")
sys.exit(errors)
