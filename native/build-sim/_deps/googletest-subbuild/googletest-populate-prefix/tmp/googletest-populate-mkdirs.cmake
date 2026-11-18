# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/agent/repos/AndroCrypter/native/build-sim/_deps/googletest-src"
  "/agent/repos/AndroCrypter/native/build-sim/_deps/googletest-build"
  "/agent/repos/AndroCrypter/native/build-sim/_deps/googletest-subbuild/googletest-populate-prefix"
  "/agent/repos/AndroCrypter/native/build-sim/_deps/googletest-subbuild/googletest-populate-prefix/tmp"
  "/agent/repos/AndroCrypter/native/build-sim/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp"
  "/agent/repos/AndroCrypter/native/build-sim/_deps/googletest-subbuild/googletest-populate-prefix/src"
  "/agent/repos/AndroCrypter/native/build-sim/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/agent/repos/AndroCrypter/native/build-sim/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/agent/repos/AndroCrypter/native/build-sim/_deps/googletest-subbuild/googletest-populate-prefix/src/googletest-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
