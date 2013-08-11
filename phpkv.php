
<?php

if(!extension_loaded('phpkv')) {
    dl('phpkv.' . PHP_SHLIB_SUFFIX);
}

//重建索引:全量
print 'build:' . kv_build('dbname') . "\n";

//重新加载:全量+增量库
print 'reload:' . kv_reload('dbname') . "\n";

//查询
print 'get:' .kv_get('dbname','1') . "\n";
?>
