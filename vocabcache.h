#include <iostream>
#include <map>
#include <string>

int get_vocab(std::map<std::string,int> &vocab,const std::string &name);
int del_vocab(std::map<std::string,int> &vocab,const std::string &key);
int add_vocab(std::map<std::string,int> &vocab, const std::string &key,const int &value);
int loadVocabFromMysql(std::map<std::string,int> &vocab,const char *host,const char *user,const char *pwd,const char *db,const char *table);
