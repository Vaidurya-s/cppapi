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

# Utility rule file for samples.

# Include any custom commands dependencies for this target.
include src/CMakeFiles/samples.dir/compiler_depend.make

# Include the progress variables for this target.
include src/CMakeFiles/samples.dir/progress.make

src/CMakeFiles/samples: src/Samples/AdvancedPublish
src/CMakeFiles/samples: src/Samples/AdvancedSubscribe
src/CMakeFiles/samples: src/Samples/AverageFrequencyCalculator
src/CMakeFiles/samples: src/Samples/DynamicMetadataPublish
src/CMakeFiles/samples: src/Samples/FilterExpressionTests
src/CMakeFiles/samples: src/Samples/InstancePublish
src/CMakeFiles/samples: src/Samples/InstanceSubscribe
src/CMakeFiles/samples: src/Samples/InteropTest
src/CMakeFiles/samples: src/Samples/LatencyTest
src/CMakeFiles/samples: src/Samples/ReversePublish
src/CMakeFiles/samples: src/Samples/ReverseSubscribe
src/CMakeFiles/samples: src/Samples/SimpleSubscribe
src/CMakeFiles/samples: src/Samples/SimplePublish

samples: src/CMakeFiles/samples
samples: src/CMakeFiles/samples.dir/build.make
.PHONY : samples

# Rule to build all files generated by this target.
src/CMakeFiles/samples.dir/build: samples
.PHONY : src/CMakeFiles/samples.dir/build

src/CMakeFiles/samples.dir/clean:
	cd /home/vaidurya/sttp/cppapi/build/src && $(CMAKE_COMMAND) -P CMakeFiles/samples.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/samples.dir/clean

src/CMakeFiles/samples.dir/depend:
	cd /home/vaidurya/sttp/cppapi/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vaidurya/sttp/cppapi /home/vaidurya/sttp/cppapi/src /home/vaidurya/sttp/cppapi/build /home/vaidurya/sttp/cppapi/build/src /home/vaidurya/sttp/cppapi/build/src/CMakeFiles/samples.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : src/CMakeFiles/samples.dir/depend

