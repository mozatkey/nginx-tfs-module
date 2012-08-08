#! /usr/bin/env python
#coding:utf8
#
# author: chuantong.huang@gmail.com
# date: 2011-08-08
#
'''
Setup script for the PyTFS, pytfs is for tfs client libs.
'''
PACKAGE = "pytfs"
VERSION = "0.3"
import os
import distutils
from distutils.core import setup
from distutils.extension import Extension
try:
 tblib = os.environ['TBLIB_ROOT']
except KeyError, ke:
 print 'TBLIB_ROOT not in you OS environ path.'
 print '必需先安装tbnet、tbsys，并导出环境变量:TBLIB_ROOT'
 raise ke

def _help():
 print "\r\n\t\t找不到头文件的错：(本模块只在tfs-stable-2.0版本编译过，其它版本未尝试)"
 print "\t使用counfigure --enable-shared --prefix=/root/tfs_bin 命令编译tfs"
 print "\t使用以下命令编译此pytfs模块 python setup.py build "
 print "\tpython setup.py build_ext -I/root/tfs_bin/include -L/root/tfs_bin/lib"
 print "\tpython install"

include_dirs = [
    './src',
    tblib + '/include/tbsys',
    tblib + '/include/tbnet',
#    '/root/tfs_bin/include',
]

library_dirs = [
    tblib,
    tblib + '/lib',
    #'/usr/local/tbsys/lib',
    #'/usr/local/tbnet/lib',
#    "/root/tfs_bin/lib"
]

libraries = ['tbsys', 'tbnet', 'uuid', 'z', 'tfsclient']

sources = [
    "pytfs.cpp",
]

module1 = Extension(name = PACKAGE,
                    define_macros = [],
                    include_dirs = include_dirs,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    sources = sources)
try:
 setup(name = PACKAGE,
      version = VERSION,
      description = 'tfs client libs for Python',
      ext_modules = [module1],
#      py_modules = ['pytfs.cpp'],
)
except:
 _help()
