add_test([=[VfsSmoke.ListRootAndReadSmallFile]=]  /agent/repos/AndroCrypter/native/build-vfs/vfs_smoke [==[--gtest_filter=VfsSmoke.ListRootAndReadSmallFile]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VfsSmoke.ListRootAndReadSmallFile]=]  PROPERTIES WORKING_DIRECTORY /agent/repos/AndroCrypter/native/build-vfs SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  vfs_smoke_TESTS VfsSmoke.ListRootAndReadSmallFile)
