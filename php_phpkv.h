/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id: header 321634 2012-01-01 13:15:04Z felipe $ */

#ifndef PHP_PHPKV_H
#define PHP_PHPKV_H

extern zend_module_entry phpkv_module_entry;
#define phpext_phpkv_ptr &phpkv_module_entry

#ifdef PHP_WIN32
#	define PHP_PHPKV_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_PHPKV_API __attribute__ ((visibility("default")))
#else
#	define PHP_PHPKV_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif
#include "vocabcache.h"

PHP_MINIT_FUNCTION(phpkv);
PHP_MSHUTDOWN_FUNCTION(phpkv);
PHP_RINIT_FUNCTION(phpkv);
PHP_RSHUTDOWN_FUNCTION(phpkv);
PHP_MINFO_FUNCTION(phpkv);

PHP_FUNCTION(confirm_phpkv_compiled);	/* For testing, remove later. */
PHP_FUNCTION(kv_build);	/* For testing, remove later. */
PHP_FUNCTION(kv_get);	/* For testing, remove later. */
PHP_FUNCTION(kv_get_batch);	/* For testing, remove later. */
PHP_FUNCTION(kv_get_batch_quick);	/* For testing, remove later. */
PHP_FUNCTION(get_vocab);	/* For testing, remove later. */
PHP_FUNCTION(add_vocab);	/* For testing, remove later. */
PHP_FUNCTION(del_vocab);	/* For testing, remove later. */

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     
*/

ZEND_BEGIN_MODULE_GLOBALS(phpkv)
	std::map<std::string,int> map;
ZEND_END_MODULE_GLOBALS(phpkv)

/* In every utility function you add that needs to use variables 
   in php_phpkv_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as PHPKV_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define PHPKV_G(v) TSRMG(phpkv_globals_id, zend_phpkv_globals *, v)
#else
#define PHPKV_G(v) (phpkv_globals.v)
#endif

#endif	/* PHP_PHPKV_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
