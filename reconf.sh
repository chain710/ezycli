#!/bin/sh
# 重新产生一遍MAKEFILE 

if [ $# -gt 0 ] ; then
    prefix=$1
fi


aclocal
autoheader
autoconf
libtoolize  -f -c
automake -a

if [ -n "$prefix" ]; then
    ./configure --prefix=${prefix}
else
    ./configure
fi

#./configure \
#			--srcdir=${srcdir} \
#			--prefix=${prefix} \
#			--bindir=${prefix}/bin \
#			--sbindir=${prefix}/sbin \
#			--datadir=${prefix}/share \
#			--sysconfdir=${prefix}/conf \
#			--localstatedir=${prefix}/var

# 
#
#
#
#
#
#
#
