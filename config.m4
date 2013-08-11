dnl $Id$
dnl config.m4 for extension phpkv

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(phpkv, for phpkv support,
Make sure that the comment is aligned:
[  --with-phpkv             Include phpkv support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(phpkv, whether to enable phpkv support,
dnl Make sure that the comment is aligned:
dnl [  --enable-phpkv           Enable phpkv support])

if test "$PHP_PHPKV" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-phpkv -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/phpkv.h"  # you most likely want to change this
  dnl if test -r $PHP_PHPKV/$SEARCH_FOR; then # path given as parameter
  dnl   PHPKV_DIR=$PHP_PHPKV
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for phpkv files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       PHPKV_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$PHPKV_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the phpkv distribution])
  dnl fi

  dnl # --with-phpkv -> add include path
  dnl PHP_ADD_INCLUDE($PHPKV_DIR/include)

  dnl # --with-phpkv -> check for lib and symbol presence
  dnl LIBNAME=phpkv # you may want to change this
  dnl LIBSYMBOL=phpkv # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $PHPKV_DIR/lib, PHPKV_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_PHPKVLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong phpkv lib version or lib not found])
  dnl ],[
  dnl   -L$PHPKV_DIR/lib -lm
  dnl ])
  dnl
  dnl PHP_SUBST(PHPKV_SHARED_LIBADD)
  LDFLAGS=-lmysqlclient -L/usr/local/mysql/lib/mysql -lstdc++ -lpthread -ltokyocabinet -lboost_iostreams -lboost_thread -lboost_system -L/usr/local/lib -lboost_filesystem
  dnl LDFLAGS="$LDFLAGS -lstdc++ -lpthread /usr/local/mysql/lib/libmysqlclient.a /usr/local/lib/libtokyocabinet.a  /usr/local/lib/libboost_iostreams.a /usr/local/lib/libboost_thread.a /usr/local/lib/libboost_system.a /usr/local/lib/libboost_filesystem.a"
  PHP_ADD_INCLUDE(/usr/local/mysql/include)
  PHP_SUBST(PHPKV_SHARED_LIBADD)
  PHP_REQUIRE_CXX() 
  PHP_NEW_EXTENSION(phpkv, phpkv.cpp vocabcache.cpp, $ext_shared)
fi
