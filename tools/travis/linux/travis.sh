#!/bin/sh
#
# Build script for travis-ci.org builds to handle compiles 
# and static analysis when ANALYZE=true.
#
if [ $ANALYZE = "true" ]; then
	# build cppcheck
	cd ~/
	git clone https://github.com/danmar/cppcheck.git
	cd cppcheck
	make -j2 -pipe
	export PATH=$PATH:~/cppcheck
	cd "$TRAVIS_BUILD_DIR"

	# run cppcheck
        cppcheck --version
        cppcheck \
          --template "{file}({line}): {severity} ({id}): {message}" \
          --enable=warning,information,performance,unusedFunction \
          --force --std=c++11 -j2 ./source \
          1> /dev/null 2> cppcheck.txt

	# if cppcheck.txt exists defect were found in the code
	# we could exit with 1 do make the Travis build appear as failed
	# but we don't to that because there is a lot to be cleaned up first
        if [ -s cppcheck.txt ]; then
            cat cppcheck.txt
            exit 0
        fi
else # no static analysis, do regular build
    ./tools/travis/linux/dependencies.sh
    ./tools/travis/linux/build.sh
fi
