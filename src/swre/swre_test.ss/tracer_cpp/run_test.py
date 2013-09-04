#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
#
# Copyright Saab AB, 2011 (http://www.safirsdk.com)
#
# Created by: Lars Hagstrom (lars@foldspace.nu)
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
import subprocess, os, time, sys, signal, re
import syslog_server

if sys.platform == "win32":
    config_type = os.environ.get("CMAKE_CONFIG_TYPE")
    exe_path = config_type if config_type else ""
else:
    exe_path = "."
    
sender_path = os.path.join(exe_path,"tracer_sender")

syslog = syslog_server.SyslogServer()

o1 = subprocess.check_output(sender_path)
o2 = subprocess.check_output(sender_path)
o3 = subprocess.check_output(sender_path)

stdout_output = o1.decode("utf-8") + o2.decode("utf-8") + o3.decode("utf-8")
syslog_output = syslog.get_data(1)

def fail(message):
    print ("STDOUT OUTPUT:")
    print(stdout_output)
    print ("SYSLOG OUTPUT:")
    print(syslog_output)
    print("Failed! Wrong number of",message)
    sys.exit(1)

if stdout_output.count("\n") != 24 or syslog_output.count("\n") != 24:
    fail("lines")

if stdout_output.count("Rymd-B@rje: blahonga") != 6 or syslog_output.count("Rymd-Börje: blahonga") != 6:
    fail("blahonga")

if stdout_output.count("Rymd-B@rje: blahonga\n") != 3 or syslog_output.count("Rymd-Börje: blahonga\n") != 3:
    fail("blahonga newlines")

if stdout_output.count("Razor: brynanuppafj@ssasponken\n") != 3 or syslog_output.count("Razor: brynanuppafjässasponken\n") != 3:
    fail("brynanuppa")

if stdout_output.count("Rymd-B@rje: blahong@a\n") != 3 or syslog_output.count("Rymd-Börje: blahong®a\n") != 3:
    fail("blahong®a")

if stdout_output.count("Rymd-B@rje: blahonga@@@\n") != 3 or syslog_output.count("Rymd-Börje: blahongaåäö\n") != 3:
    fail("åäö")

if stdout_output.count("Razor: 123.1\n") != 3 or syslog_output.count("Razor: 123.1\n") != 3:
    fail("123.1")

if stdout_output.count("Razor: foobar\n") != 3 or syslog_output.count("Razor: foobar\n") != 3:
    fail("foobar")
    
if stdout_output.count("Razor: this is the end\n") != 3 or syslog_output.count("Razor: this is the end\n") != 3:
    fail("this is the end")

if stdout_output.count("Razor: my only friend\n") != 3 or syslog_output.count("Razor: my only friend\n") != 3:
    fail("my only friend")
    

print("success")
sys.exit(0)
