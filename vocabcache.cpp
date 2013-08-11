
#include "vocabcache.h"
#include "/usr/local/mysql/include/mysql.h"

int loadVocabFromMysql(std::map<std::string,int> &vocab,const char *host,const char *user,const char *pwd,const char *db,const char *table)
{
  std::string mysqlQuery = "select w,rank from " + std::string(db) + "." + std::string(table);
  MYSQL _mysql;
  mysql_init(&_mysql);
  if(!mysql_real_connect(&_mysql,host,user,pwd,NULL,0,NULL,0))
  {
    std::cerr<< "Failed to connect to database: Error: " <<
    	mysql_error(&_mysql)<<std::endl;
    mysql_close(&_mysql);
    return false;
  }
  if(mysql_query(&_mysql, mysqlQuery.c_str()))
  {
    std::cerr<< "Query failed: " <<mysql_error(&_mysql)<<std::endl;
    mysql_close(&_mysql);
    return false;
  }
  MYSQL_RES * result = mysql_use_result(&_mysql);
  MYSQL_ROW row;
  unsigned int num_fields = mysql_num_fields(result);
  if(num_fields < 2)
  {
    std::cerr<<" num fields is not consitent with query : "<<num_fields<<std::endl; 
    mysql_free_result(result);
    mysql_close(&_mysql);
    return false;
  }
  while(row = mysql_fetch_row(result))
  {
    unsigned long *rowLengths = mysql_fetch_lengths(result);
    std::string key = "";
    int value = 0;
    if(rowLengths[0] && row[0])
    {
      key = std::string(row[0]);
    }
    else
    {
      continue;
    }
    if(rowLengths[1] && row[1])
    {
      value = atoi(row[1]);
    }
    else
    {
      continue;
    }
    vocab.insert(make_pair(key,value)); 
  }
  mysql_free_result(result);
  mysql_close(&_mysql);
  return true;
}

int add_vocab(std::map<std::string,int> &vocab, const std::string &key,const int &value)
{
  std::pair< std::map<std::string,int>::iterator,bool > ret;
  ret=vocab.insert(std::pair<std::string,int>(key,value));
  if(ret.second==false)
  {
    std::cerr<<"The insert key -> value ("<<key<<"->"<<value<<")has extists in map!"<<std::endl;
    return false;
  }
  else
  {
    std::cerr<<"Insert key -> value ("<<key<<"->"<<value<<") SUCCEED!"<<std::endl;
    return true;
  }
}

int del_vocab(std::map<std::string,int> &vocab,const std::string &key)
{
  std::map<std::string,int>::iterator it = vocab.find(key);
  if(it == vocab.end())
  {
    std::cerr<<"The delete query("<<key<<") is not in memory! please check in!"<<std::endl;
    return false;
  }
  else
  {
    vocab.erase(it);
    std::cerr<<"Delete key("<<key<<") SUCCEED!"<<std::endl;
    return true;
  }
}

int get_vocab(std::map<std::string,int> &vocab,const std::string &name)
{
  std::map<std::string,int>::iterator it = vocab.find(name);
  if(it != vocab.end())
    return it->second;
  else
    return 0;
}
/*
int main()
{
  std::map<std::string,int> map;
  std::string host = "127.0.0.1";
  std::string user = "cikuu";
  std::string pwd = "cikuutest!";
  std::string db = "pigai_spss";
  std::string table = "vocab";
  loadVocabFromMysql(map,host.c_str(),user.c_str(),pwd.c_str(),db.c_str(),table.c_str());
  std::string a="accept";
  int res = get_vocab(map,a);
  int flag = add_vocab(map,"11111",11111);
  int flag1 = del_vocab(map,"11111");
  std::cout<<res<<std::endl;
  std::cout<<flag<<std::endl;
  std::cout<<flag1<<std::endl;
}
*/
