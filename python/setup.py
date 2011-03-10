#! /usr/bin/env python
#coding:utf8
#
# author: chuantong.huang@gmail.com
# date: 2011-3-4
#
'''
Setup script for the PyTFS, pytfs is for tfs client libs.
'''
PACKAGE = "pytfs"
VERSION = "0.1"
import os
import distutils
from distutils.core import setup
from distutils.extension import Extension
try:
 tblib = os.environ['TBLIB_ROOT']
except KeyError, ke:
 print 'TBLIB_ROOT not in you OS environ path.'
 print 'has you install the TBLIB_ROOT: tysys, tbnet?'
 raise ke

include_dirs = [
    '/root/workspace/tb-tfs/src',
    tblib + '/include/tbsys',
    tblib + '/include/tbnet',
]

library_dirs = [
    tblib,
    tblib + '/lib'
    #'/usr/local/tbsys/lib',
    #'/usr/local/tbnet/lib',
]

libraries = ['tbsys', 'tbnet']

sources = [
    os.path.join("src/client", "pytfs.cpp"),
    os.path.join("src/client", "tfs_session_pool.cpp"),
    os.path.join("src/client", "tfs_session.cpp"),
    os.path.join("src/client", "tfs_file.cpp"),
    os.path.join("src/client", "fsname.cpp"),
    os.path.join("src/message", "client.cpp"),
    os.path.join("src/message", "client_pool.cpp"),
    os.path.join("src/message", "async_client.cpp"),
    os.path.join("src/message", "admin_cmd_message.cpp"),
    os.path.join("src/message", "status_message.cpp"),
    os.path.join("src/message", "dataserver_message.cpp"),
    os.path.join("src/message", "block_info_message.cpp"),
    os.path.join("src/message", "close_file_message.cpp"),
    os.path.join("src/message", "client_cmd_message.cpp"),
    os.path.join("src/message", "compact_block_message.cpp"),
    os.path.join("src/message", "crc_error_message.cpp"),
    os.path.join("src/message", "create_filename_message.cpp"),
    os.path.join("src/message", "heart_message.cpp"),
    os.path.join("src/message", "reload_message.cpp"),
    os.path.join("src/message", "oplog_sync_message.cpp"),
    os.path.join("src/message", "file_info_message.cpp"),
    os.path.join("src/message", "message_factory.cpp"),
    os.path.join("src/message", "tfs_packet_streamer.cpp"),
    os.path.join("src/message", "read_data_message.cpp"),
    os.path.join("src/message", "rename_file_message.cpp"),
    os.path.join("src/message", "write_data_message.cpp"),
    os.path.join("src/message", "unlink_file_message.cpp"),
    os.path.join("src/message", "rollback_message.cpp"),
    os.path.join("src/message", "replicate_block_message.cpp"),
    os.path.join("src/message", "server_meta_info_message.cpp"),
    os.path.join("src/message", "server_status_message.cpp"),
    os.path.join("src/common", "parameter.cpp"),
    os.path.join("src/common", "func.cpp"),
    os.path.join("src/common", "config.cpp"),
]


module1 = Extension(name = PACKAGE,
                    define_macros = [],
                    include_dirs = include_dirs,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    sources = sources)

setup(name = PACKAGE,
      version = VERSION,
      description = 'tfs client libs for Python',
      ext_modules = [module1],
#      py_modules = ['pytfs.cpp'],
)
