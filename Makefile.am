include $(top_srcdir)/profile

CXXFLAGS=$(FLAGS)

INCLUDES=

includedir=$(prefix)/include

include_HEADERS=\
    conn_define.h\
    conn_pool.h\
    conn_pool_error.h\
    transfer_pack.h

ezyclidir=$(prefix)/lib

ezycli_LIBRARIES=libezycli.a

libezycli_a_SOURCES=\
    conn_pool.cpp\
    transfer_pack.cpp

