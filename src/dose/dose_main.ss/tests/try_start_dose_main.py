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

import subprocess, os, time, sys

SAFIR_RUNTIME = os.environ.get("SAFIR_RUNTIME")

print "This test program expects to be killed off after about two minutes unless it has finished successfully before then."

proc = subprocess.Popen(os.path.join(SAFIR_RUNTIME,"bin","dose_main"), stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
lines = list()
for i in range(3):
    lines.append(proc.stdout.readline().rstrip("\n\r"))
    print "Line", i, ": '" + lines[-1] + "'"

#give it one second to output any spurious stuff...
time.sleep(1)

proc.terminate()
for i in range (10):
    if proc.poll() is not None:
        break
    time.sleep(0.1)
if proc.poll() is None:
    try:
        proc.kill()
    except OSError:
        pass
    proc.wait()
res = proc.communicate()[0]

if len(res) != 0:
    print "More than three lines output! Trailing data is\n'"+res + "'"
    sys.exit(1)

if lines[0] != "dose_main is waiting for persistence data!":
    print "Failed to find string 'dose_main is waiting for persistence data!'"
    sys.exit(1)
if lines[1] != "Running in Standalone mode":
    print "Failed to find string 'Running in Standalone mode'"
    sys.exit(1)
if lines[2] != "dose_main running (release)..." and lines[2] != "dose_main running (debug)...":
    print "Failed to find string 'dose_main running (release)...' or 'dose_main running (debug)...'"
    sys.exit(1)

print "success"
sys.exit(0)
