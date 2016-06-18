import os, glob, platform

print "Running on " + platform.system()

#make sure the release folder exists, and remove and files that might be there already
if not os.path.exists( "release" ):
    os.makedirs( "release" )

os.chdir( "release" )
files = glob.glob( "*.*" )
for f in files:
    os.remove( f )
os.chdir( ".." )


#make sure the debug folder exists, and remove and files that might be there already
if not os.path.exists( "debug" ):
    os.makedirs( "debug" )

os.chdir( "debug" )
files = glob.glob( "*.*" );
for f in files:
    os.remove( f )
os.chdir( ".." )

#find all the cpp files in /source.  We'll compile all of them
os.chdir( "source" )
cpp_files = glob.glob( "*.cpp" );
os.chdir( ".." )

#specify the search paths/dependencies/options for gcc
include_paths = [ "../include" ]
link_paths = [ "../lib" ]
link_dependencies = [ "-lSaleaeDevice" ] #refers to libSaleaeDevice.dylib

debug_compile_flags = "-O0 -w -c -fpic -g"
release_compile_flags = "-O3 -w -c -fpic"

#loop through all the cpp files, build up the gcc command line, and attempt to compile each cpp file
for cpp_file in cpp_files:

    #g++
    command = "/usr/bin/g++-4.4 "

    #include paths
    for path in include_paths: 
        command += "-I\"" + path + "\" "

    release_command = command
    release_command  += release_compile_flags
    release_command += " -o\"release/" + cpp_file.replace( ".cpp", ".o" ) + "\" " #the output file
    release_command += "\"" + "source/" + cpp_file + "\"" #the cpp file to compile

    debug_command = command
    debug_command  += debug_compile_flags
    debug_command += " -o\"debug/" + cpp_file.replace( ".cpp", ".o" ) + "\" " #the output file
    debug_command += "\"" + "source/" + cpp_file + "\"" #the cpp file to compile

    #run the commands from the command line
    print release_command
    os.system( release_command )
    print debug_command
    os.system( debug_command )
    
#lastly, link
#g++

if platform.system().lower() == "darwin":
    command = "g++ "
else:
    command = "/usr/bin/g++-4.4 -Wl,-rpath,'$ORIGIN:$ORIGIN/../../lib' "
    

#add the library search paths
for link_path in link_paths:
    command += "-L\"" + link_path + "\" "

#add libraries to link against
for link_dependency in link_dependencies:
    command += link_dependency + " "

#figgure out what the name of this Project is
project_name = os.path.realpath(__file__)
project_name = os.path.dirname( project_name )
project_name = os.path.basename( project_name )
print "project name is: " + project_name

#the files to create
release_command = command + "-o release/" + project_name + " " 
debug_command = command + "-o debug/" + project_name + " " 

#add all the object files to link
for cpp_file in cpp_files:
    release_command += "release/" + cpp_file.replace( ".cpp", ".o" ) + " "
    debug_command += "debug/" + cpp_file.replace( ".cpp", ".o" ) + " "
    
#run the commands from the command line
print release_command
os.system( release_command )
print debug_command
os.system( debug_command )

if platform.system().lower() == "darwin":
    cmd = "cp ../lib/*.dylib debug"
    print cmd
    os.system( cmd )
    cmd = "cp ../lib/*.dylib release"
    print cmd
    os.system( cmd ) 
