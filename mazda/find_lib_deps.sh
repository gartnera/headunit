#!/bin/bash
set -e

# Given a shared library, print the symbols it uses from other libraries it
# directly depends on.

LIB=$1
# Use readelf rather than ldd here to only get direct dependencies.
DEPS=$(readelf -d $LIB | awk '/Shared library:/{ print substr($5, 2, length($5) - 2) }')

UNDEF_SYMS_FILE=solib-deps-$(basename $LIB)-undef.txt

# Get a list of all undefined symbols in our target library.
# We strip off symbol version information,
# e.g. "AES_cbc_encrypt@@libcrypto.so.10" becomes "AES_cbc_encrypt".
./m3-toolchain/bin/arm-cortexa9_neon-linux-gnueabi-nm $LIB | sed -ne 's/[ ]\+U \([^ @]\+\)\(@@.*$\|$\)/\1/p' | sort > $UNDEF_SYMS_FILE

# For each library listed as a dependency, get the list of symbols it exports,
# and print those that are also in the list of undefined symbols we generated
# above.
for dep in $DEPS
do
    # Use ldd here to find the full path to the library.
    # Regex workaround here for libs including '+', e.g. libstdc++.
    dep_regex=$(echo $dep | sed -e 's/\+/\\+/g')
    DEP_FILE=$(./m3-toolchain/bin/arm-cortexa9_neon-linux-gnueabi-ldd --root ./m3-toolchain/arm-cortexa9_neon-linux-gnueabi/sysroot $LIB | awk "/$dep_regex =>/{ print \$3 }")
    if [ x$DEP_FILE = "xnot" ]
    then
        echo "W: Couldn't find a file for $dep. Skipping."
        echo
        continue
    fi
    DEP_PROVIDES_FILE=solib-deps-$(basename $dep)-provides.txt
    ./m3-toolchain/bin/arm-cortexa9_neon-linux-gnueabi-readelf -DsW ./m3-toolchain/arm-cortexa9_neon-linux-gnueabi/sysroot$DEP_FILE | awk '/GLOBAL DEFAULT/{ print $9 }' | sort > $DEP_PROVIDES_FILE
    echo $DEP_FILE:
    comm -12 $UNDEF_SYMS_FILE $DEP_PROVIDES_FILE | sed -e 's/^/  /'
    echo
    rm -f $DEP_PROVIDES_FILE
done
