<?php
/**
 * 系统配置文件
 * @copyright    Copyright(c) 2015
 * @author       Wumpus
 * @version      1.0
 */

$CONFIG['system']['db'] = array(
    'db_host'           =>      'localhost',
    'db_user'           =>      'root',
    'db_password'       =>      'msi',
    'db_database'       =>      'oj',
    'db_table_prefix'   =>      'oj_',
    'db_charset'        =>      'utf8'
);

$CONFIG['system']['lib'] = array(
    'prefix'            =>      'ob' // app lib prefix54
);

$CONFIG['system']['route'] = array(
    'default_controller'    =>  'home',
    'default_action'        =>  'index',
    'url_type'              =>  1   //URL形式  1普通模式 2 PATHINFO
);

$CONFIG['system']['cache'] = array(
    'cache_dir'         =>      'cache',
    'cache_prefix'      =>      'cache_',
    'cache_time'        =>      1800,
    'cache_mode'        =>      2   //1 serialize  2保存为可执行文件
);

$CONFIG['system']['cookie'] = array(
    'time'              =>      60*60*24,
);

$CONFIG['system']['mq'] = array(
    'mq_host'       =>      'localhost',
    'mq_port'       =>      '5672',
    'mq_user'       =>      'guest',
    'mq_password'   =>      'guest',
    'mq_queue'      =>      'duang',
);

?>