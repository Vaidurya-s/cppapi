# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.28

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/vaidurya/sttp/cppapi

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/vaidurya/sttp/cppapi/build

# Include any dependencies generated for this target.
include src/CMakeFiles/InstanceSubscribe.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include src/CMakeFiles/InstanceSubscribe.dir/compiler_depend.make

# Include the progress variables for this target.
include src/CMakeFiles/InstanceSubscribe.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/InstanceSubscribe.dir/flags.make

src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.o: src/CMakeFiles/InstanceSubscribe.dir/flags.make
src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.o: /home/vaidurya/sttp/cppapi/src/samples/InstanceSubscribe/InstanceSubscribe.cpp
src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.o: src/CMakeFiles/InstanceSubscribe.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/vaidurya/sttp/cppapi/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.o"
	cd /home/vaidurya/sttp/cppapi/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.o -MF CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.o.d -o CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.o -c /home/vaidurya/sttp/cppapi/src/samples/InstanceSubscribe/InstanceSubscribe.cpp

src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.i"
	cd /home/vaidurya/sttp/cppapi/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/vaidurya/sttp/cppapi/src/samples/InstanceSubscribe/InstanceSubscribe.cpp > CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.i

src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.s"
	cd /home/vaidurya/sttp/cppapi/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/vaidurya/sttp/cppapi/src/samples/InstanceSubscribe/InstanceSubscribe.cpp -o CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.s

src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.o: src/CMakeFiles/InstanceSubscribe.dir/flags.make
src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.o: /home/vaidurya/sttp/cppapi/src/samples/InstanceSubscribe/SubscriberHandler.cpp
src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.o: src/CMakeFiles/InstanceSubscribe.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/vaidurya/sttp/cppapi/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.o"
	cd /home/vaidurya/sttp/cppapi/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.o -MF CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.o.d -o CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.o -c /home/vaidurya/sttp/cppapi/src/samples/InstanceSubscribe/SubscriberHandler.cpp

src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.i"
	cd /home/vaidurya/sttp/cppapi/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/vaidurya/sttp/cppapi/src/samples/InstanceSubscribe/SubscriberHandler.cpp > CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.i

src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.s"
	cd /home/vaidurya/sttp/cppapi/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/vaidurya/sttp/cppapi/src/samples/InstanceSubscribe/SubscriberHandler.cpp -o CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.s

# Object files for target InstanceSubscribe
InstanceSubscribe_OBJECTS = \
"CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.o" \
"CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.o"

# External object files for target InstanceSubscribe
InstanceSubscribe_EXTERNAL_OBJECTS =

src/Samples/InstanceSubscribe: src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/InstanceSubscribe.cpp.o
src/Samples/InstanceSubscribe: src/CMakeFiles/InstanceSubscribe.dir/samples/InstanceSubscribe/SubscriberHandler.cpp.o
src/Samples/InstanceSubscribe: src/CMakeFiles/InstanceSubscribe.dir/build.make
src/Samples/InstanceSubscribe: src/Libraries/libsttp.a
src/Samples/InstanceSubscribe: src/CMakeFiles/InstanceSubscribe.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/vaidurya/sttp/cppapi/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable Samples/InstanceSubscribe"
	cd /home/vaidurya/sttp/cppapi/build/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/InstanceSubscribe.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/InstanceSubscribe.dir/build: src/Samples/InstanceSubscribe
.PHONY : src/CMakeFiles/InstanceSubscribe.dir/build

src/CMakeFiles/InstanceSubscribe.dir/clean:
	cd /home/vaidurya/sttp/cppapi/build/src && $(CMAKE_COMMAND) -P CMakeFiles/InstanceSubscribe.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/InstanceSubscribe.dir/clean

src/CMakeFiles/InstanceSubscribe.dir/depend:
	cd /home/vaidurya/sttp/cppapi/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vaidurya/sttp/cppapi /home/vaidurya/sttp/cppapi/src /home/vaidurya/sttp/cppapi/build /home/vaidurya/sttp/cppapi/build/src /home/vaidurya/sttp/cppapi/build/src/CMakeFiles/InstanceSubscribe.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : src/CMakeFiles/InstanceSubscribe.dir/depend

