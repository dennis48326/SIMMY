# cmake -DSRC=<file> -DDST=<file> -P copy_if_missing.cmake: copies SRC to DST unless DST exists.
if (NOT EXISTS "${DST}")
  configure_file("${SRC}" "${DST}" COPYONLY)
endif ()
