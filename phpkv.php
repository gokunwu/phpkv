
<?php

if(!extension_loaded('phpkv')) {
    dl('phpkv.' . PHP_SHLIB_SUFFIX);
}

//�ؽ�����:ȫ��
print 'build:' . kv_build('dbname') . "\n";

//���¼���:ȫ��+������
print 'reload:' . kv_reload('dbname') . "\n";

//��ѯ
print 'get:' .kv_get('dbname','1') . "\n";
?>
