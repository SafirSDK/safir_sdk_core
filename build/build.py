#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
#
# Copyright Saab AB, 2009 (http://www.safirsdk.com)
#
# Created by: Lars Hagstrom / stlrha
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
import os, glob, sys, subprocess


#Load some environment variables that are needed throughout as globals
SAFIR_RUNTIME = os.environ.get("SAFIR_RUNTIME")
SAFIR_SDK = os.environ.get("SAFIR_SDK")

#a few constants
known_configs = set(["Release", "Debug", "MinSizeRel", "RelWithDebInfo"])

#define some global variables
verbose = False
skip_list= None
clean = False
force_config = None
force_extra_config = None
target_architecture = None
ada_support = False
java_support = False

class FatalError(Exception):pass

def die(message):
    raise FatalError(message)

def is_64_bit():
    return sys.maxsize > 2**32

def copy_dob_files(source_dir, target_dir):
    """Copy dou and dom files from the source directory to the given subdirectory in the dots_genereted directory"""
    import os
    import re
    import shutil
    dots_generated_dir = os.path.join(SAFIR_SDK,"dots","dots_generated")
    abs_target_dir = os.path.join(dots_generated_dir, target_dir)

    log("Copying dob files from " + source_dir + " to " + abs_target_dir, True)
    
    if not os.path.isdir(dots_generated_dir):
        mkdir(dots_generated_dir)

    pattern = re.compile("[a-zA-Z0-9\.\-]*\.do[um]$")
    pattern2 = re.compile("[a-zA-Z0-9\.]*-java\.namespace\.txt$")
    for filename in os.listdir(source_dir):
        if pattern.match(filename) or pattern2.match(filename):
            
            if not os.path.isdir(abs_target_dir):
                mkdir(abs_target_dir)
                
            shutil.copy2(os.path.join(source_dir, filename), abs_target_dir)

def mkdir(newdir):
    """works the way a good mkdir should :)
        - already exists, silently complete
        - regular file in the way, raise an exception
        - parent directory(ies) does not exist, make them as well
    """
    if os.path.isdir(newdir):
        pass
    elif os.path.isfile(newdir):
        raise OSError("a file with the same name as the desired " \
                      "dir, '%s', already exists." % newdir)
    else:
        head, tail = os.path.split(newdir)
        if head and not os.path.isdir(head):
            mkdir(head)
        if tail:
            os.mkdir(newdir)

            
def remove(path):
    if not os.path.exists(path):
        return
    if os.path.isfile(path):
        try:
            os.remove(path)
            return
        except Exception as e:
            die ("Failed to remove file " + path + ". Got exception " + str(e))
            
    for name in os.listdir(path):
        if os.path.isdir(os.path.join(path,name)):
            remove(os.path.join(path,name))
        else:
            try:
                os.remove(os.path.join(path,name))
            except Exception as e:
                die ("Failed to remove file " + os.path.join(path,name) + ". Got exception " + str(e))

    try:
        os.rmdir(path)
    except Exception as e:
        die ("Failed to remove directory " + path + ". Got exception " + str(e))


def num_cpus():
    """Detects the number of CPUs on a system. Cribbed from pp."""
    # Linux, Unix and MacOS:
    if hasattr(os, "sysconf"):
        if "SC_NPROCESSORS_ONLN" in os.sysconf_names:
            # Linux & Unix:
            ncpus = os.sysconf("SC_NPROCESSORS_ONLN")
            if isinstance(ncpus, int) and ncpus > 0:
                return ncpus
        else: # OSX:
            return int(os.popen2("sysctl -n hw.ncpu")[1].read())
    # Windows:
    if "NUMBER_OF_PROCESSORS" in os.environ:
            ncpus = int(os.environ["NUMBER_OF_PROCESSORS"]);
            if ncpus > 0:
                return ncpus
    return 1 # Default


def log(message,ignore_verbose = False):
    if verbose or ignore_verbose:
        print(message)
        sys.stdout.flush()


def check_environment():
    if SAFIR_RUNTIME == None or SAFIR_SDK == None:
        die("You need to have both SAFIR_RUNTIME and SAFIR_SDK set");
    #TODO check cmake?! and other needed stuff




def parse_command_line(builder):
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option("--command-file", "-f",action="store",type="string",dest="command_file",
                      help="The command to execute")
    parser.add_option("--target",action="store",type="string",dest="target_architecture",default="x86",
                          help="Target architecture, x86 or x64")
    parser.add_option("--no-ada-support", action="store_false",dest="ada_support",default=True,
                      help="Disable Ada support")
    parser.add_option("--no-java-support", action="store_false",dest="java_support",default=True,
                      help="Disable Java support")
    parser.add_option("--skip-list",action="store",type="string",dest="skip_list",
                      help="A space-separated list of regular expressions of lines in the command file to skip")
    parser.add_option("--clean", action="store_true",dest="clean",default=False,
                      help="Run 'clean' before building each subsystem.")
    parser.add_option("--force-config", action="store",type="string",dest="force_config",
                      help="Build for the given config irrespective of what the command file says about config")
    parser.add_option("--force-extra-config", action="store",type="string",dest="force_extra_config",
                      help="Build for the given extra config irrespective of what the command file says about extra_config")    
    parser.add_option("--verbose", "-v", action="store_true",dest="verbose",default=False,
                      help="Print more stuff about what is going on")
    builder.setup_command_line_options(parser)
    (options,args) = parser.parse_args()

    if options.target_architecture == "x64":
        if not is_64_bit():
            die("Target x64 can't be set since this is not a 64 bit OS")
        if options.ada_support:
            die("64 bit Ada build is currently not supported")
        if options.java_support:
            die("64 bit Java build is currently not supported")            
    elif options.target_architecture != "x86":
        die("Unknown target architecture " + options.target_architecture)
    global target_architecture
    target_architecture = options.target_architecture

    global ada_support
    ada_support = options.ada_support
    global java_support
    java_support = options.java_support
    
    
    global skip_list;
    if (options.skip_list == None):
        skip_list = list();
    else:
        skip_list = options.skip_list.split();
        
    global clean
    clean = options.clean

    global verbose
    verbose = options.verbose
    
    if (options.command_file == None):
        die("You need to specify the command file to use")
        
    if not os.path.isfile(options.command_file):
        die("The specified command file could not be found")
    global command_file
    command_file = open(options.command_file,'r')

    global force_config;
    if (options.force_config != None):
        force_config = options.force_config

    global force_extra_config;
    if (options.force_extra_config != None):
        force_extra_config = options.force_extra_config        
        
    builder.handle_command_line_options(options)

def find_sln():
    sln_files = glob.glob("*.sln")
    if (len(sln_files) != 1):
        die("There is not exactly one sln file in " + os.getcwd() +
            ", either cmake failed to generate one or there is another one coming from somewhere else!")
    return sln_files[0]
    
            
class VisualStudioBuilder(object):
    def __init__(self):    
        self.tmpdir = os.environ.get("TEMP")
        if self.tmpdir is None:
            self.tmpdir = os.environ.get("TMP")
            if self.tmpdir is None:
                die("Failed to find a temp directory!")
        if not os.path.isdir(self.tmpdir):
            die("I can't seem to use the temp directory " + self.tmpdir)        

    def set_studio_version(self):
        VS80 = os.environ.get("VS80COMNTOOLS")
        VS90 = os.environ.get("VS90COMNTOOLS")
        VS100 = os.environ.get("VS100COMNTOOLS")

        VSCount = 0;

        if VS80 is not None:
            VSCount = VSCount + 1;
        if VS90 is not None:
            VSCount = VSCount + 1;
        if VS100 is not None:
            VSCount = VSCount + 1;

        self.studio = None
        self.generator = None
        self.studio_install_dir = None
        self.vcvarsall_arg = None
        
        if VSCount > 1:
            log("I found several Visual Studio installations, will need command line arg!")
        elif VS80 is not None:
            self.studio = VS80
            if target_architecture == "x86":
                self.generator = "Visual Studio 8 2005"
            elif target_architecture == "x64":
                self.generator = "Visual Studio 8 2005 Win64"
            else:
                die("Target architecture " + target_architecture + " is not supported for Visual Studio 8 2005 !")                
        elif VS90 is not None:
            self.studio = VS90
            if target_architecture == "x86":
                self.generator = "Visual Studio 9 2008"
            elif target_architecture == "x64":
                self.generator = "Visual Studio 9 2008 Win64"
            else:
                die("Target architecture " + target_architecture + " is not supported for Visual Studio 9 2008 !")        
        elif VS100 is not None:
            self.studio = VS100
            if target_architecture == "x86":
                self.generator = "Visual Studio 10"
            elif target_architecture == "x64":
                self.generator = "Visual Studio 10 Win64"
            else:
                die("Target architecture " + target_architecture + " is not supported for Visual Studio 10 !")
        else:
            die("Could not find a supported compiler to use!")
            
    @staticmethod
    def can_use():
        VS80 = os.environ.get("VS80COMNTOOLS")
        VS90 = os.environ.get("VS90COMNTOOLS")
        VS100 = os.environ.get("VS100COMNTOOLS")
        return VS80 is not None or VS90 is not None or VS100 is not None

    def setup_command_line_options(self,parser):
        parser.add_option("--use-studio",action="store",type="string",dest="use_studio",
                          help="The visual studio to use for building, can be '2005', '2008' or '2010'")

    def handle_command_line_options(self,options):
        self.set_studio_version()
        if self.studio is None and options.use_studio is None:
            die("Please specify which visual studio you want to use for building.\n" +
                "Use the --use-studio command line switch, can be '2005', '2008' or '2010'")
        elif options.use_studio is not None:
            if options.use_studio == "2005":
                self.studio = os.environ.get("VS80COMNTOOLS")
                if target_architecture == "x86":
                    self.generator = "Visual Studio 8 2005"
                elif target_architecture == "x64":
                    self.generator = "Visual Studio 8 2005 Win64"
                else:
                    die("Target architecture " + target_architecture + " is not supported for Visual Studio 8 2005 !")  
            elif options.use_studio == "2008":
                self.studio = os.environ.get("VS90COMNTOOLS")
                if target_architecture == "x86":
                    self.generator = "Visual Studio 9 2008"
                elif target_architecture == "x64":
                    self.generator = "Visual Studio 9 2008 Win64"
                else:
                    die("Target architecture " + target_architecture + " is not supported for Visual Studio 9 2008 !")  
            elif options.use_studio == "2010":
                self.studio = os.environ.get("VS100COMNTOOLS")
                if target_architecture == "x86":
                    self.generator = "Visual Studio 10"
                elif target_architecture == "x64":
                    self.generator = "Visual Studio 10 Win64"
                else:
                    die("Target architecture " + target_architecture + " is not supported for Visual Studio 10 !")  
            else:
                die ("Unknown visual studio " + options.use_studio)

        self.studio_install_dir = os.path.join(self.studio,"..", "..")

        #work out what compiler tools to use
        if target_architecture == "x86":
            self.vcvarsall_arg = "x86"
        elif target_architecture == "x64":
            self.vcvarsall_arg = "amd64"
        else:
            die("Unknown target architecture " + target_architecture)    

    def build(self, directory, configs):
        log(" - in config(s): " + str(list(configs)),True)
        if self.__can_use_studio_build(directory):
            self.__studio_build(directory,configs)
        else:
            self.__nmake_build(directory,configs)
            
    def dobmake(self):
        """run the dobmake command"""
        ada = ""
        if not ada_support:
            ada = " --no-ada "
        java = ""
        if  not java_support:
            java = " --no-java "
            
        batpath = os.path.join(self.tmpdir,"build2.bat")
        bat = open(batpath,"w")
        bat.write("@echo off\n" +
                  "call \"" + os.path.join(self.studio_install_dir,"VC","vcvarsall.bat") +  "\" "  + self.vcvarsall_arg + "\n" +
                  "\"" + os.path.join(SAFIR_RUNTIME,"bin","dobmake.py") + "\" -b --html-output --rebuild" + ada + java + " --target " + target_architecture + "\n") #batch mode (no gui)
        bat.close()
        process = subprocess.Popen(batpath,stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        result = process.communicate()
        buildlog.write(result[0])
        if process.returncode != 0:
            die("Failed to run dobmake")

    def __can_use_studio_build(self,directory):
        cmakelists = open("CMakeLists.txt","r")
        contents = cmakelists.read().lower()
        cmakelists.close()
        bad_keywords = ("add_subdirectory",
                        "add_custom_target",
                        "add_custom_command",
                        "add_cs_library",
                        "dotnet",
                        "java",
                        "gprmake")
        log(" - Checking if I can use Visual Studio to speed the build up.")
        for bad in bad_keywords:
            if contents.find(bad) != -1:
                log("  - No, CMakeLists.txt contains " + bad)
                return False

        good_keywords = ("add_executable",
                         "add_library")

        for good in good_keywords:
            if contents.find(good) != -1:
                log("  - Yes, CMakeLists.txt contains " + good)
                return True
        log("  - No, failed to find either good or bad keywords!")
        return False

    def __studio_build(self,directory,configs):
        if clean:
            remove(self.generator)
            
        mkdir(self.generator)
        olddir = os.getcwd();
        os.chdir(self.generator)
    
        self.__run_command(("cmake -D SAFIR_BUILD_SYSTEM_VERSION:string=2 " +
                            "-G \"" + self.generator + "\" " +
                            ".."),
                           "Configure", directory)

        solution = find_sln()
        
        if clean:
            for config in configs:
                self.__run_command("devenv.com " + solution + " /clean " + config,
                                   "Clean " + config, directory)


        for config in configs:
            self.__run_command("devenv.com " + solution + " /build  " + config,
                               "Build " + config, directory)

            self.__run_command("devenv.com " + solution + " /build " +config +" /Project INSTALL",
                               "Install " + config, directory)
        
        os.chdir(olddir)

    def __nmake_build(self,directory,configs):
        """build a directory using nmake"""
        for config in configs:                
            self.__run_command("cmake -D CMAKE_BUILD_TYPE:string=" + config + " " +
                               "-D SAFIR_ADA_SUPPORT:boolean=" + str(ada_support) + " " +
                               "-D SAFIR_JAVA_SUPPORT:boolean=" + str(java_support) + " " +
                               "-D SAFIR_BUILD_SYSTEM_VERSION:string=2 " +
                               "-D SAFIR_BUILD_TARGET_ARCHITECTURE:string=" + target_architecture + " " +
                               "-G \"NMake Makefiles\" .",
                               "Configure " + config, directory)
            if clean:
                self.__run_command("nmake /NOLOGO clean",
                                   "Clean " + config, directory, allow_fail=True)
            self.__run_command("nmake /NOLOGO",
                               "Build " + config, directory)
            self.__run_command("nmake /NOLOGO install",
                               "Install " + config, directory)

    def __run_command(self, cmd, description, what, allow_fail = False):
        """Run a command"""
        batpath = os.path.join(self.tmpdir,"build.bat")
        bat = open(batpath,"w")
        bat.write("@echo off\n" +
                  "call \"" + os.path.join(self.studio_install_dir,"VC","vcvarsall.bat") +  "\" " + self.vcvarsall_arg + "\n" +
                  cmd)
        bat.close()
        buildlog.write("<h4>" + description + " '" + what + "'</h4><pre style=\"color: green\">" + cmd + "</pre>\n")
        process = subprocess.Popen(batpath,stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        result = process.communicate()
        buildlog.write("<pre>")
        buildlog.write(result[0])
        buildlog.write("</pre>")
        if process.returncode != 0:
            if not allow_fail:
                die("Failed to run '" + cmd + "' for " + what)
            else:
                buildlog.write("This command failed, but failure of this particular command is non-fatal to the build process, so I'm continuing\n")


class UnixGccBuilder(object):
    def __init__(self):
        #We need to limit ourselves a little bit in how
        #many parallel jobs we perform. Each job may use
        #up to 400Mb of memory. Maybe we should try 
        #to work out how much memory there is as well?
        if num_cpus() > 1:
            self.num_jobs = 3
        else:
            self.num_jobs = 2

    @staticmethod
    def can_use():
        import sys
        return sys.platform == "linux2"

    def setup_command_line_options(self,parser):
        pass

    def handle_command_line_options(self,options):
        pass

    def build(self, directory, configs):
        """build a directory using make"""
        config = configs[0]
        log(" - in config: " + config, True)
        self.__run_command(("cmake",
                            "-D", "CMAKE_BUILD_TYPE:string=" + config,
                            "-D SAFIR_ADA_SUPPORT:boolean=" + str(ada_support) + " " +
                            "-D SAFIR_JAVA_SUPPORT:boolean=" + str(java_support) + " " +
                            "-D", "SAFIR_BUILD_SYSTEM_VERSION:string=2",
                            "."),
                           "Configure " + config, directory)
        if clean:
            self.__run_command(("make", "clean"),
                               "Clean " + config, directory, allow_fail=True)
        self.__run_command(("make","-j", str(self.num_jobs)),
                           "Build " + config, directory)
        self.__run_command(("make", "install","-j", str(self.num_jobs)),
                           "Install " + config, directory)

    def dobmake(self):
        """run the dobmake command"""
        ada = ""
        if not ada_support:
            ada = " --no-ada "
        java = ""
        if not java_support:
            java = " --no-java "
            
        process = subprocess.Popen((os.path.join(SAFIR_RUNTIME,"bin","dobmake.py"), "-b", "--html-output", "--rebuild", ada, java), #batch mode (no gui)
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT,
                                   universal_newlines=True)
        result = process.communicate()
        buildlog.write(result[0])
        if process.returncode != 0:
            die("Failed to run dobmake")

    def __run_command(self, cmd, description, what, allow_fail = False):
        """Run a command"""
        buildlog.write("<h4>" + description + " '" + what + "'</h4><pre style=\"color: green\">" + " ".join(cmd) + "</pre>\n")
        process = subprocess.Popen(cmd,stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        result = process.communicate()
        buildlog.write("<pre>")
        buildlog.write(result[0])
        buildlog.write("</pre>")
        if process.returncode != 0:
            if not allow_fail:
                die("Failed to run '" + " ".join(cmd) + "' for " + what)
            else:
                buildlog.write("This command failed, but failure of this particular command is non-fatal to the build process, so I'm continuing<br>")



def in_skip_list(line):
    "Check the argument against all regexps in the skip-list"
    import re
    for expr in skip_list:
        p=re.compile(expr)
        if p.search(line):
            return True
    return False;


def build_dir(directory, configs, builder):
    if not os.path.isdir(directory):
        die("Failed to enter " + directory + ", since it does not exist (or is not a directory)")
    olddir = os.getcwd();
    os.chdir(directory)
    try:
        buildlog.write("<h1>BUILDING " + directory + "</h1>\n");
        if not os.path.isfile("CMakeLists.txt"):
            die ("Couldn't find a CMakeLists.txt in " + directory + ".")
        builder.build(directory,configs)
    finally:
        os.chdir(olddir)



def dobmake(builder):
    buildlog.write("<h1>Running dobmake</h1>\n");
    builder.dobmake()

def get_builder():
    if VisualStudioBuilder.can_use():
        return VisualStudioBuilder()
    elif UnixGccBuilder.can_use():
        return UnixGccBuilder()
    else:
        die("Failed to work out what builder to use!")
    
def main():
    check_environment()
    builder = get_builder()    
    parse_command_line(builder)

    global buildlog
    


    olddir = os.getcwd();

    for line in command_file:
        if line[0] == '#' or line.strip() == "":
            continue

        split_line = line.split()
        
        if in_skip_list(line.strip()):
            log("- Skipping " + str(split_line) + ", matches skip-list",True)
            sys.stdout.flush()
            continue
                
        command = split_line[0]

        if command == "build_dir":
            if len(split_line) > 1:
                directory = split_line[1]

            configs = list()
            if len(split_line) > 2:
                if (force_config != None):
                    configs.append(force_config)
                else:
                    configs.append(split_line[2])
            
            if len(split_line) > 3:
                if (force_extra_config != None):
                    configs.append(force_extra_config)
                else:    
                    configs.append(split_line[3])
                
            if not set(configs) <= known_configs:
                die ("Unknown build kind '" + str(configs) + "' for " + directory)
            if len(configs) < 1:
                die ("Need at least one config for " + directory)
            log("Building " + directory,True)
            build_dir(directory, configs,builder)
        elif command == "copy_dob_files":
            if len(split_line) < 3:
                die ("Need both a source and target directory")
            if len(split_line) > 3:
                die ("To many parameters for " + command)
            copy_dob_files(os.path.normpath(split_line[1]), os.path.normpath(split_line[2]))  
        elif command == "dobmake":
            log("Running dobmake",True)
            dobmake(builder)
        else:
            die("Got unknown command '" + command + "'")
        
        sys.stdout.flush()
        sys.stderr.flush()

    os.chdir(olddir)
    
    log("Build was successful, but there may have been some warnings (or errors?) while building.",True)
    log("Look in buildlog.html for clues",True)


#actual code starts here

logpath = "buildlog.html"

try:
    os.remove(logpath)
except OSError:
    pass
buildlog = open(logpath,'w', 0)
buildlog.write("<html><title>Build Log</title><body>")

try:
    main()
except FatalError as e:
    log("Build script failed: " + str(e),True)
    sys.exit(1)

buildlog.write("</body>")
buildlog.close()
sys.exit(0)
