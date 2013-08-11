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
extern "C"{
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
}
#include "ext/standard/php_smart_str.h"
#include <string>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <tcutil.h>
#include <tcbdb.h>
#include <mysql.h>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/pool/object_pool.hpp>
#include <map>
#include "php_phpkv.h"

/* If you declare any globals in php_phpkv.h uncomment this:
*/
ZEND_DECLARE_MODULE_GLOBALS(phpkv)

/* True global resources - no need for thread safety here */
static int le_phpkv;

/*  phpkv_functions[]
 *
 * Every user visible function must have an entry in phpkv_functions[].
 */
zend_function_entry phpkv_functions[] = {
    PHP_FE(kv_build,	NULL)
    PHP_FE(kv_get,	NULL)
    PHP_FE(kv_get_batch,	NULL)
    PHP_FE(kv_get_batch_quick,	NULL)
    PHP_FE(get_vocab,	NULL)
    PHP_FE(add_vocab,	NULL)
    PHP_FE(del_vocab,	NULL)
    {
        NULL, NULL, NULL
    }
};
/*  */

/*  phpkv_module_entry
 */
zend_module_entry phpkv_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"phpkv",
	phpkv_functions,
	PHP_MINIT(phpkv),
	PHP_MSHUTDOWN(phpkv),
	PHP_RINIT(phpkv),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(phpkv),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(phpkv),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/*  */

#ifdef COMPILE_DL_PHPKV
ZEND_GET_MODULE(phpkv)
#endif

/*  PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("phpkv.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_phpkv_globals, phpkv_globals)
    STD_PHP_INI_ENTRY("phpkv.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_phpkv_globals, phpkv_globals)
PHP_INI_END()
*/
/*  */

/*  php_phpkv_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_phpkv_init_globals(zend_phpkv_globals *phpkv_globals)
{
	phpkv_globals->global_value = 0;
	phpkv_globals->global_string = NULL;
}
*/
/*  */
typedef std::map<std::string,std::string> CStringMap;
typedef boost::shared_ptr< CStringMap > CStringMapPtr;
class CFinder
{
public:
    virtual void * find(const char *key,int key_size,int *len) = 0;
};
class CBuilder {
public:
    virtual bool save(const std::string &k,const std::string &v) = 0;
};
typedef boost::shared_ptr<CFinder> CFinderPtr;
class CKVDatabase
{
public:
    CFinderPtr _full;
    CStringMapPtr _update;
    boost::filesystem::path _dir;
};


typedef std::map<std::string,CKVDatabase> CDBMap;
static CDBMap _db_map;

clock_t disp_progress(clock_t start,clock_t prev,size_t n1, size_t n2) {
    clock_t last =  clock();
    double t1= (double)(last - prev)/CLOCKS_PER_SEC;
    double t2 = (double)(last - start)/CLOCKS_PER_SEC;
    size_t r1= (size_t)(n1 / t1);
    size_t r2 = (size_t)(n2 / t2);
    std::cout << "total:" << n2 << " rate:" << r1 << " avg rate:" << r2 << std::endl << std::flush;
    return last;
}

class CTCFinder : public CFinder {
    TCBDB *_db;
    void clear()
    {
        if (!_db)  return;
        tcbdbclose(_db);
        tcbdbdel(_db);
        _db = NULL;
    }
public:
    CTCFinder():_db(NULL)
    {
    }
    ~CTCFinder()
    {
        clear();
    }
    void* find(const char *key,int key_size,int *len)
    {
        return tcbdbget(_db, key, key_size, len);
    }
    bool open(boost::filesystem::path  path)
    {
        clear();
        _db = tcbdbnew();
        if (!tcbdbsetmutex(_db))
            return false;
        if (!tcbdbopen(_db,path.string().c_str(), BDBOREADER)) {
	    std::cout << tcbdberrmsg(tcbdbecode(_db)) << std::endl;
            clear();
            return false;
        }
        return true;
    }
};


class CMapBuilder : public CBuilder {
    CStringMapPtr _map;
public:
    CMapBuilder(CStringMapPtr m):_map(m) {
    }
    bool save(const std::string &k,const std::string &v)
    {
        (*_map)[k] = v;
        return true;
    }
};
#define BDBDEFLMEMB    128               // default number of members in each leaf
#define BDBDEFNMEMB    256               // default number of members in each node
#define BDBDEFBNUM     32749             // default bucket number
#define BDBDEFAPOW     8                 // default alignment power
#define BDBDEFFPOW     10                // default free block pool power
class CTCBuilder : public CBuilder
{
    TCBDB *_db;
public:
    void close()
    {
        if (!_db) return;
        tcbdbclose(_db);
        tcbdbdel(_db);
        _db = NULL;
    }
    ~CTCBuilder()
    {
        close();
    }
    CTCBuilder()
        :_db(NULL)
    {
    }
    bool save(const std::string &k,const std::string &v)
    {
        return tcbdbput(_db,
                        k.data(),k.size(),
                        v.data(),v.size());
    }
    bool flush()
    {
        /*
        12亿数据，tcbdboptimize需要20+小时以上
            return tcbdboptimize(_db,
                                 BDBDEFLMEMB, BDBDEFNMEMB,
                                 BDBDEFBNUM, BDBDEFAPOW, BDBDEFFPOW, 0);
        						 */
        return true;
    }
    bool open(const char *file)
    {
        close();
        unlink(file);
        _db = tcbdbnew();
        const int N  = 5;
        const int M = (int)log2(N);
        if (!tcbdbtune(_db,
                       BDBDEFLMEMB *N,
                       BDBDEFNMEMB *N,
                       BDBDEFBNUM * N,
                       BDBDEFAPOW * M,
                       BDBDEFFPOW * M,
                       0))
        {
            close();
            return false;
        }
        if (!tcbdbopen(_db,file, BDBOWRITER | BDBOCREAT)) {
	    std::cout << tcbdberrmsg(tcbdbecode(_db)) << std::endl;
            close();
            return false;
        }
        return true;
    }
};

class MySQLLoader
{
    MYSQL _mysql;
public:
    MySQLLoader()
    {
        mysql_init(&_mysql);
    }
    ~MySQLLoader()
    {
        mysql_close(&_mysql);
    }
    bool load(boost::property_tree::ptree &pt,CBuilder *builder,const std::string &mode)
    {
        int N =  pt.get<int>(mode +".N",500000);
        std::string host = boost::trim_copy( pt.get<std::string>(mode +".host","127.0.0.1"));
        std::string user = boost::trim_copy( pt.get<std::string>(mode +".user","root"));
        std::string pwd = boost::trim_copy( pt.get<std::string>(mode +".pwd",""));
        std::string query = boost::trim_copy( pt.get<std::string>(mode +".query"));
        if ( query.empty() || host.empty() || user.empty()) {
            std::cerr << "query,host,user can't empty" << std::endl;
            return false;
        }
        std::cout << "host:" << host << std::endl;
        std::cout << "user:" << user << std::endl;
        std::cout << "query:" << query << std::endl;
        if (!mysql_real_connect(&_mysql,host.c_str(),user.c_str(),pwd.c_str(),NULL,0,NULL,0))
        {
            std::cerr << "Failed to connect to database: Error: " <<
                      mysql_error(&_mysql) << std::endl;
            return false;
        }
        if ( mysql_query(&_mysql, query.c_str()))
        {
            std::cerr <<   mysql_error(&_mysql) << std::endl;
            return false;
        }
        MYSQL_RES *result     = mysql_use_result(&_mysql);
        MYSQL_ROW row;
        unsigned int   num_fields = mysql_num_fields(result);
        if (num_fields <2) {
            std::cerr <<  "num fields:" << num_fields << std::endl;
            return false;
        }
        clock_t start = clock();
        clock_t prev = start;
        size_t cnt = 0;
        while (row = mysql_fetch_row(result))
        {
            unsigned long *lengths = mysql_fetch_lengths(result);
            std::string k,v;
            if ( lengths[0] && row[0])
            {
                k = std::string(row[0],lengths[0] );
            } else {
                continue;
            }
            if ( lengths[1] && row[1])
            {
                v = std::string(row[1],lengths[1] );
            } else {
                continue;
            }
            builder->save(k,v);
            cnt++;
            if (!(cnt % N)) {
                prev = disp_progress(start,prev,N,cnt);
            }
        }
        mysql_free_result(result);
        disp_progress(start,prev,N,cnt);
        return true;
    }
};

static CStringMapPtr kv_build_mysql(boost::property_tree::ptree &pt)
{
    MySQLLoader loader;
    CStringMapPtr map(new CStringMap());
    CMapBuilder builder(map);
    if (!loader.load(pt,&builder,"update")) {
        std::cerr << "build update failed" << std::endl;
        map.reset();
    }
    return map;
}

/*  PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(phpkv)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/
    std::string host = "";
    std::string user = "";
    std::string pwd = "";
    std::string vocabdb = "";
    std::string table = "";
    boost::filesystem::path conf1("/etc/phpkv.conf");
    if (!boost::filesystem::exists(conf1)) {
        std::cerr << "can't found /etc/phpkv.conf"<< std::endl;
        return FAILURE;
    }
    boost::property_tree::ptree pt1;
    boost::property_tree::ini_parser::read_ini(conf1.string(), pt1);
    BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, pt1.get_child("vocab"))
    {
        std::string dbname = v.first;
        std::string path =  v.second.get_value<std::string>();//v.second;
        std::cout << "vocab:[" << dbname << "] path:[" << path <<"]"<< std::endl;
        CKVDatabase db;
        db._dir = boost::filesystem::path(path);
        boost::filesystem::path conf1(db._dir/"db.conf" );
        if (!boost::filesystem::exists(db._dir)) {
            boost::system::error_code e;
            boost::filesystem::create_directories(db._dir,e);
            if (e) {
                std::cerr << "can't found create_directories:" << db._dir.string() << std::endl;
                return FAILURE;
            }
        }
        boost::property_tree::ptree pt1;
        if (boost::filesystem::exists(conf1)) {
            boost::property_tree::ini_parser::read_ini(conf1.string(), pt1);
            host = boost::trim_copy( pt1.get<std::string>("vocab.host"));
            user = boost::trim_copy( pt1.get<std::string>("vocab.user"));
            pwd = boost::trim_copy( pt1.get<std::string>("vocab.pwd"));
            vocabdb = boost::trim_copy( pt1.get<std::string>("vocab.db"));
            table = boost::trim_copy( pt1.get<std::string>("vocab.table"));
        } else {
            pt1.put("vocab.host","127.0.0.1");
            pt1.put("vocab.user","");
            pt1.put("vocab.pwd","");
            pt1.put("vocab.db","");
            pt1.put("vocab.table","");

            boost::property_tree::ini_parser::write_ini(conf1.string(),pt1);
        }
        _db_map[dbname] = db;
	}
    loadVocabFromMysql(phpkv_globals.map,host.c_str(),user.c_str(),pwd.c_str(),vocabdb.c_str(),table.c_str());


    boost::filesystem::path conf("/etc/phpkv.conf");
    if (!boost::filesystem::exists(conf)) {
        std::cerr << "can't found /etc/phpkv.conf"<< std::endl;
        return FAILURE;
    }
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(conf.string(), pt);
    BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, pt.get_child("db"))
    {
        std::string dbname = v.first;
        std::string path =  v.second.get_value<std::string>();//v.second;
        std::cout << "db:[" << dbname << "] path:[" << path <<"]"<< std::endl;
        CKVDatabase db;
        db._dir = boost::filesystem::path(path);
        boost::filesystem::path conf(db._dir/"db.conf" );
        if (!boost::filesystem::exists(db._dir)) {
            boost::system::error_code e;
            boost::filesystem::create_directories(db._dir,e);
            if (e) {
                std::cerr << "can't found create_directories:" << db._dir.string() << std::endl;
                return FAILURE;
            }
        }
        boost::property_tree::ptree pt;
        if (boost::filesystem::exists(conf)) {
            boost::property_tree::ini_parser::read_ini(conf.string(), pt);
            std::string full = boost::trim_copy( pt.get<std::string>("full.path"));
            CFinderPtr finder(new CTCFinder());
            if (((CTCFinder *) finder.get())->open(db._dir /full)) {
                boost::atomic_store(&db._full,finder);
            }
            CStringMapPtr update = kv_build_mysql(pt);
            if (update) {
                boost::atomic_store(&db._update,update);
            }
        } else {
            pt.put("full.host","127.0.0.1");
            pt.put("full.user","");
            pt.put("full.pwd","");
            pt.put("full.query","");
            pt.put("full.path","");

            pt.put("update.host","127.0.0.1");
            pt.put("update.user","");
            pt.put("update.pwd","");
            pt.put("update.query","");
            pt.put("update.path","");
            boost::property_tree::ini_parser::write_ini(conf.string(),pt);
        }
        _db_map[dbname] = db;
    }
	return SUCCESS;
}
/*  */

/*  PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(phpkv)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
    _db_map.clear();
    phpkv_globals.map.clear();
	return SUCCESS;
}
/*  */

/* Remove if there's nothing to do at request start */
/*  PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(phpkv)
{
	return SUCCESS;
}
/*  */

/* Remove if there's nothing to do at request end */
/*  PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(phpkv)
{
	return SUCCESS;
}
/*  */

/*  PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(phpkv)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "phpkv support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/*  */


static bool kv_build_mysql(const boost::filesystem::path &dir,
                           boost::property_tree::ptree &pt,
                           const std::string &db,
                           const std::string &mode)
{

    char buf[128];
    time_t now = time(NULL);
    strftime(buf, sizeof(buf) -1, "%Y%m%d_%H%M%S", localtime(&now));
    boost::filesystem::path tmp_file(dir/(std::string(buf)+ ".tmp"));
    MySQLLoader loader;
    CTCBuilder builder;
    boost::system::error_code e;
    if (!builder.open(tmp_file.string().c_str())) {
        std::cerr<< "build failed:" << db << std::endl;
        boost::filesystem::remove(tmp_file,e);
        return false;
    }
    if (!loader.load(pt,&builder,mode)) {
        std::cerr<< "load failed:"  << db << std::endl;
        boost::filesystem::remove(tmp_file,e);
        return false;
    }
    if (!builder.flush()) {
        std::cerr<< "flush failed:"  << db<< std::endl;
        boost::filesystem::remove(tmp_file,e);
        return false;
    }
    builder.close();

    boost::filesystem::path db_file(dir/(std::string(buf) + ".tc"));
    boost::filesystem::rename(tmp_file,db_file,e);
    if (e)
    {
        std::cerr<< "remove failed:" << db<< std::endl;
        boost::filesystem::remove(tmp_file,e);
        return false;
    }
    pt.put(mode +".path" , std::string(buf) + ".tc" );
    return true;
}
/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/*  proto string confirm_phpkv_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_phpkv_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = spprintf(&strg, 0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "phpkv", arg);
	RETURN_STRINGL(strg, len, 0);
}
PHP_FUNCTION(get_vocab)
{
	char *arg = NULL;
	int arg_len, len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}
    std::string word = arg;
    int res = get_vocab(phpkv_globals.map,word);

	RETURN_LONG(res);
}
PHP_FUNCTION(add_vocab)
{
	char *arg = NULL;
	int arg_len, len;
    long value;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &arg, &arg_len,&value) == FAILURE) {
		return;
	}
    std::string word = arg;
    int res = add_vocab(phpkv_globals.map,word,value);

	RETURN_LONG(res);
}
PHP_FUNCTION(del_vocab)
{
	char *arg = NULL;
	int arg_len, len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}
    std::string word = arg;
    int res = del_vocab(phpkv_globals.map,word);

	RETURN_LONG(res);
}
PHP_FUNCTION(kv_build)//bool kv_build("dbname",update|yes,full|yes)
{
    const char *db = NULL;
    int db_len = 0;
    int argc = ZEND_NUM_ARGS();
    if (zend_parse_parameters(argc TSRMLS_CC, "s",
                              &db, &db_len) == FAILURE)
    {
        RETURN_FALSE;
    }
    if (!db|| db_len <=0 ) {
        RETURN_NULL();
    }
    std::string dbname(std::string(db,db_len));
    CDBMap::iterator it = _db_map.find(dbname);
    if (it == _db_map.end()) {
        RETURN_FALSE;
    }
    boost::filesystem::path conf(it->second._dir/"db.conf" );
    if (!boost::filesystem::exists(conf)) {
        RETURN_FALSE;
    }
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(conf.string(), pt);
    if (!kv_build_mysql(it->second._dir,pt,dbname,"full"))
    {
        RETURN_FALSE;
    }
    //重建全量库
    boost::property_tree::ini_parser::write_ini(conf.string(),pt);
    RETURN_TRUE;
}
#if 0
PHP_FUNCTION(kv_get)
{
    const char *db = NULL,*key = NULL;
    int argc = ZEND_NUM_ARGS();
    int key_len = 0,db_len = 0;
    if (zend_parse_parameters(argc TSRMLS_CC, "ss",
                              &db, &db_len,
                              &key,&key_len) == FAILURE)
    {
        RETURN_NULL();
    }
    if (!db|| db_len <=0 ) {
        RETURN_NULL();
    }
    CDBMap::iterator it = _db_map.find(std::string(db,db_len));
    if (it == _db_map.end()) {
        RETURN_NULL();
    }

    //查询增量库
    CStringMapPtr update = boost::atomic_load(&it->second._update);
    if (update ) {
        CStringMap::iterator it = update->find(std::string(key,key_len));
        if ( it != update->end()) {
            RETURN_STRINGL((char *)it->second.data(),it->second.size(),1);
        }
    }

    //查询全量库
    int len;
    CFinderPtr full(boost::atomic_load(&it->second._full));
    char *buf  = full ? (char *)full->find(key,key_len,&len) : NULL;
    if (buf) {
        /*
        char buf1[2048];
        strcpy(buf1,buf);
        if(buf != NULL)
		{
        	free(buf);
			buf = NULL;
        }
        RETURN_STRINGL(buf1,len,1);
        */
		RETURN_STRINGL(buf,len,0);
    }
    RETURN_NULL();
}
#endif
//todo:sort后再查找
typedef struct  {
    char *key;
    char *val;
    int key_len;
    int val_len;
    int idx;
} BATCH_KV;
char *php_memory_dup(const void *src,int len)
{
    if (!src || len <= 0) return NULL;
    char *buf   = (char *)emalloc(len);
    memcpy(buf,src,len);
    return buf;
}
bool batch_key_cmp( const BATCH_KV * a, const BATCH_KV * b )
{
    int len = std::min<int>(a->key_len,b->key_len);
    int n = memcmp(a->key,b->key,len);
    return n ? n < 0 : a->key_len < b->key_len;
}
bool find_key(CFinder * full,CStringMap *update ,BATCH_KV *kv)
{
    if (!kv->key || kv->key_len <= 0)
    {
        kv->val_len = 0;
        kv->val   = NULL;
        return false;
    }
    if (update ) {
        //查询增量库
        CStringMap::iterator it  = update->find(std::string(kv->key,kv->key_len));
        if ( it != update->end()) {
            kv->val_len = it->second.size();
            kv->val = php_memory_dup(it->second.data(),kv->val_len);
            return true;
        }
    }
    if (full) {
        //查询全量库
		void *val = full->find(kv->key,kv->key_len,&kv->val_len);
        kv->val   = php_memory_dup(val,kv->val_len);
		if (val) tcfree(val);
        return kv->val != NULL;
    }
    kv->val_len = 0;
    kv->val   = NULL;
    return false;
}
//查询单个key,输入k,返回v
PHP_FUNCTION(kv_get)
{
    const char *db = NULL,*key = NULL;
    int argc = ZEND_NUM_ARGS();
    int key_len = 0,db_len = 0;
    if (zend_parse_parameters(argc TSRMLS_CC, "ss",
                              &db, &db_len,
                              &key,&key_len) == FAILURE)
    {
        RETURN_NULL();
    }
    if (!db|| db_len <=0 || !key || key_len <= 0) {
        RETURN_NULL();
    }
    CDBMap::iterator it = _db_map.find(std::string(db,db_len));
    if (it == _db_map.end()) {
        RETURN_NULL();
    }
    CStringMapPtr update = boost::atomic_load(&it->second._update);
    CFinderPtr full(boost::atomic_load(&it->second._full));
    BATCH_KV kv;
    kv.key = (char *)key;
    kv.key_len = key_len;
    if (find_key(full.get(),update.get() ,&kv))
    {
        RETURN_STRINGL(kv.val,kv.val_len,0);
    } else {
        RETURN_NULL();
    }
}

//批量查询多个key,输入[k1,k2...],返回[v1,v2...]
PHP_FUNCTION(kv_get_batch)
{
    const char *db = NULL;
    int argc = ZEND_NUM_ARGS();
    int db_len = 0;
    zval *val;
    if (zend_parse_parameters(argc TSRMLS_CC, "sa",
                              &db, &db_len,
                              &val) == FAILURE)
    {
        RETURN_NULL();
    }
    if (!db|| db_len <=0 ) {
        RETURN_NULL();
    }
    if (Z_TYPE_P(val) != IS_ARRAY) {
        RETURN_NULL();
    }
    CDBMap::iterator it = _db_map.find(std::string(db,db_len));
    if (it == _db_map.end()) {
        RETURN_NULL();
    }


    CStringMapPtr update = boost::atomic_load(&it->second._update);
    CFinderPtr full(boost::atomic_load(&it->second._full));
    CFinder * f = full.get();
    CStringMap *u  = update.get();

    HashPosition         pos;
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P( val ),&pos);
    zval               **z_val;
    BATCH_KV kv;
    zval *array;
    MAKE_STD_ZVAL(array);
    array_init(array);
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P( val ), (void **)&z_val, &pos ) == SUCCESS ) {
        convert_to_string_ex( z_val );
        kv.key_len = Z_STRLEN_PP(z_val);
        kv.key = Z_STRVAL_PP(z_val);
        if (find_key(f,u,&kv)) {
            add_next_index_stringl(array, kv.val, kv.val_len, 0);
        } else {
            add_next_index_null(array);
        }
        zend_hash_move_forward_ex(Z_ARRVAL_P( val ), &pos );
    }
    RETURN_ZVAL(array,false, false );
}

//先对key排序，然后做批量查询多个key,输入[k1,k2...],返回[v1,v2...]
//多个key顺序查询更快，但排序本身需要消耗cpu，需要根据具体应用取舍
PHP_FUNCTION(kv_get_batch_quick)
{
    const char *db = NULL;
    int argc = ZEND_NUM_ARGS();
    int db_len = 0;
    zval *val;
    if (zend_parse_parameters(argc TSRMLS_CC, "sa",
                              &db, &db_len,
                              &val) == FAILURE)
    {
        RETURN_NULL();
    }
    if (!db|| db_len <=0 ) {
        RETURN_NULL();
    }
    if (Z_TYPE_P(val) != IS_ARRAY) {
        RETURN_NULL();
    }
    CDBMap::iterator it = _db_map.find(std::string(db,db_len));
    if (it == _db_map.end()) {
        RETURN_NULL();
    }
    HashPosition         pos;
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P( val ),&pos);
    zval               **z_val;
    boost::object_pool<BATCH_KV> pool;
    std::vector<BATCH_KV*> kvs;
    int idx = 0;
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P( val ), (void **)&z_val, &pos ) == SUCCESS ) {
        convert_to_string_ex( z_val );
        BATCH_KV *kv = pool.malloc();
        kv->key_len = Z_STRLEN_PP(z_val);
        kv->key = php_memory_dup(Z_STRVAL_PP(z_val),kv->key_len );
        kv->idx = idx++;
        kvs.push_back(kv);
        zend_hash_move_forward_ex(Z_ARRVAL_P( val ), &pos );
    }
    //先对key进行排序,然后再查询
    CStringMapPtr update = boost::atomic_load(&it->second._update);
    CFinderPtr full(boost::atomic_load(&it->second._full));
    CFinder * f = full.get();
    CStringMap *u  = update.get();
    std::sort(kvs.begin(),kvs.end(),batch_key_cmp);
    for(std::vector<BATCH_KV*>::iterator it = kvs.begin(); it != kvs.end(); ++it) {
        find_key(f,u,*it);
    }
    //查询结果按原来的顺序返回
    zval *array;
    MAKE_STD_ZVAL(array);
    array_init(array);
    for(std::vector<BATCH_KV*>::const_iterator it = kvs.begin(); it != kvs.end(); ++it) {
        const BATCH_KV *kv = *it;
        if (kv->val) {
            add_index_stringl(array,kv->idx, kv->val, kv->val_len, 0);
        } else {
            add_index_null(array, kv->idx);
        }
        efree(kv->key);
    }
    RETURN_ZVAL(array,false, false );
}
/*  */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
