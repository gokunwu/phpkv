#1.zlib
if [ ! -e zlib-1.2.7.tar.gz ]; then
    wget http://jaist.dl.sourceforge.net/project/libpng/zlib/1.2.7/zlib-1.2.7.tar.gz
fi
tar vzxf zlib-1.2.7.tar.gz \
&& cd zlib-1.2.7 \
&& ./configure && make && make install && cd ..

#2.bzip2
if [ ! -e bzip2-1.0.6.tar.gz ]; then
    wget http://bzip.org/1.0.6/bzip2-1.0.6.tar.gz
fi
tar vzxf bzip2-1.0.6.tar.gz \
&& cd bzip2-1.0.6 \
&& make && make install && cd ..


#3.tokyocabinet
if [ ! -e tokyocabinet-1.4.48.tar.gz ]; then
    wget http://fallabs.com/tokyocabinet/tokyocabinet-1.4.48.tar.gz
fi
tar vzxf tokyocabinet-1.4.48.tar.gz \
&& cd tokyocabinet-1.4.48 \
&& ./configure  --disable-shared --disable-bzip  --disable-zlib --disable-exlzo --disable-exlzma \
&& make && make install && cd ..


#4.mysql-connector-c
if [ ! -e mysql-connector-c-6.0.2.tar.gz]; then
    wget http://cdn.mysql.com/Downloads/Connector-C/mysql-connector-c-6.0.2.tar.gz
fi
rm -rf boost_1_53_0
tar vzxf mysql-connector-c-6.0.2.tar.gz \
&& cd mysql-connector-c-6.0.2 \
&& cmake -DCMAKE_CXX_FLAGS="-fPIC" -DCMAKE_C_FLAGS="-fPIC"  . \
&& make && make install  && cd ..


#5.boost
if [ ! -e boost_1_53_0.tar.bz2]; then
    wget http://superb-dca2.dl.sourceforge.net/project/boost/boost/1.53.0/boost_1_53_0.tar.bz2
fi
tar vjxf boost_1_53_0.tar.bz2 \
&& cd boost_1_53_0 \
&& sh bootstrap.sh \
&& ./b2 stage
./b2 cxxflags=-fPIC --without-python link=static runtime-link=static stage install \
&& cd ..

#6.phpkv
#yum install php-devel
#���ʹ������������װ�����滻phpize��php-config��Ӧ��·��
cd phpkv \
&& phpize \
&& ./configure --with-php-config=/usr/bin/php-config \
&& make && make install && cd ..


    
    
    

1.ϵͳ����:/etc/phpkv.conf
[db]
pigai_spss=/data2/phpkv
[vocab]
vocab=/data2/phpkv


2.���ݿ�����(ִ��php phpkv.php���Զ�������)
/data2/phpkv/db.conf
[full]
path=20130329_232526.tc
host=127.0.0.1
user=cikuu
pwd=cikuutest!
query=select kp,mf from pigai_spss.kp_yes
[update]
path=update_20120909_122323.dat
host=127.0.0.1
user=cikuu
pwd=cikuutest!
query=select kp,mf from pigai_spss.kp_update
[vocab]
host=127.0.0.1
user=cikuu
pwd=cikuutest!
db=pigai_spss
table=vocab


3.ʹ��
kv_build("dbname");            //�ؽ�ȫ��������
kv_reload("dbname");           //�ؼ���ȫ��������������
kv_get("dbname","k1");         //��ѯ
<?php
kv_build("pigai_spss");
echo kv_get("pigai_spss","! whcl/h")."\n";
echo kv_get("pigai_spss","fuck")."\n";
$query = array("fuck","! whcl/h");
print_r(kv_get_batch("pigai_spss",$query))."\n";
print_r(kv_get_batch_quick("pigai_spss",$query))."\n";
echo kv_get("pigai_spss","fuck")."\n";
echo add_vocab("11111",11111)."\n";
echo del_vocab("11111")."\n";
echo get_vocab("accept")."\n";
?>
��ע��
   1).php��������ʱ��������ֻ����һ��ȫ����������mysql��������std::map��
   2).��/etc/phpkv.php����Ŀ�б�֧�ֶ����Ŀ��
   3).��/data2/phpkv/db.conf��ά��������Ŀ��mysql��Ϣ��

ע�����
Q:
  PHP Warning:  dl(): Dynamically loaded extensions aren't enabled in /.../phpkv.php on line 4
A:
  php.ini-->enable_dl = on

�ر�php�ڴ��Ż����������ڴ�й¶
export USE_ZEND_ALLOC=0

configure֮���Makefile�ļ����޸�LDFLAGS = ѡ��Ϊ
LDFLAGS = -lmysqlclient -L/usr/local/mysql/lib/mysql -lstdc++ -lpthread
-ltokyocabinet -lboost_iostreams -lboost_thread -lboost_system
-lboost_filesystem



