/*
 * (C) 2011-2012 chuantong.huang@gmail.com Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id: pytfs.cpp 95 2011-03-03 09:06:24Z chuantong.huang@gmail.com $
 *
 * Authors:
 *   chuantong <chuantong.huang@gmail.com>
 *      - initial release
 *
 */

// tfs2.0 版本接口变化比较大，文件操作的open，write类接口直接把tfs_前辍去除了
static const char* __SUPPORT__ = "support tfs-stable-2.0";
#include <Python.h>

#include "tfs_client_api.h"
#include "func.h"
#include "tblog.h"

using namespace tfs::client;
using namespace tfs::common;
using namespace std;

//每次发送数据大小
static Py_ssize_t WROTE_PRE_ONE = 1 * 1024 * 1024;

typedef struct {
    PyObject_HEAD
    PyObject *dict;                 /* Python attributes dictionary */
    TfsClient *tfs_handle;
    int fd;                         /* tfs file fd,when call open to return */
} TfsClientObject;

static PyObject *ErrorObject = NULL;
static PyTypeObject *p_TfsClient_Type = NULL;

static const char module_doc [] =
"This module implements an interface to the tfs client library.\n"
"version() -> tuple.  Return version information.\n"
"注意：非线程安全, 且没实现delete等接口\n not safe in multi threading\n"
">>> import pytfs\n"
">>> tfs = pytfs.TfsClient()\n"
">>> tfs.init('127.0.0.1:8018')\n"
">>> tfs.open(None, pytfs.WRITE_MODE, None) # return fd\n"
">>> tfs.write('abcd')\n"
">>> tfs.close() #end write and return T1XXXXXXX\n"
">>> tfs.open('T1xxxxxx', pytfs.READ_MODE, None)\n"
">>> tfs.read(fd, count) #return count stream from file\n"
">>> tfs.close()#end read\n"
"#or you can use the easy function:\n"
">>> tfs.put(stream) # put a new file to tfs.\n"
">>> tfs.get('T1xxxxxxx') # get a file from tfs.\n"
;
static const char *tfsclient_doc = module_doc;

const char * _check_str_obj(PyObject *obj)
{
    char *str = NULL;
	if (Py_None == obj)
		return str;

	if (PyString_Check(obj))
	{
		if (0 == PyString_Size(obj) || 0 != PyString_AsStringAndSize(obj, &str, NULL))
			return NULL;
		return str;
	}
	return NULL;
}

static char pytfs_version_doc [] = "pytfs.version() -> version info";
static PyObject *
do_pytfs_version(PyObject *self, PyObject *args)
{
	PyObject* pTuple = PyTuple_New(3);
	PyTuple_SetItem(pTuple, 0, Py_BuildValue("s", "0.3"));
	PyTuple_SetItem(pTuple, 1, Py_BuildValue("s", __SUPPORT__));
	PyTuple_SetItem(pTuple, 2, Py_BuildValue("s", "author: chuantong.huang@gmail.com"));
	Py_INCREF(pTuple);
	return pTuple;
}

static char pytfs_setloglevel_doc [] = "setloglevel(DEBUG|WARN|INFO|ERROR) default = 'ERROR';\n"
		"-> return None\n";
static PyObject *
do_pytfs_setloglevel(PyObject *self, PyObject *args)
{
	const char *level = NULL;
	Py_INCREF(Py_None);

	if (!PyArg_ParseTuple(args, "s", &level))
		return Py_None;

	TBSYS_LOGGER.setLogLevel(level);
	return Py_None;
}

// 构造函数
static char tfsclient_new_doc [] = "Create new instances of Class TfsClient. \n"
		"you must call init() to initialize it.\n";
TfsClientObject*
tfsclient_new(PyObject *dummy)
{
	UNUSED(dummy);
	TfsClientObject *self = NULL;

	self = (TfsClientObject *) PyObject_GC_New(TfsClientObject, p_TfsClient_Type);
	if (self == NULL)
		goto error;

	self->tfs_handle = TfsClient::Instance();
	if (self->tfs_handle == NULL)
		goto error;

	self->dict = PyDict_New();
	if(self->dict == NULL)
		goto error;

	PyObject_GC_Track(self);

	return self;
error:
	PyErr_SetString(ErrorObject, "PyObject_GC_New(TfsClientObject) error!");
	Py_DECREF(self);
	return NULL;
};

// 析构函数，后先调用clear
static void
_tfsclient_dealloc(TfsClientObject *self)
{
    PyObject_GC_UnTrack(self);
    Py_TRASHCAN_SAFE_BEGIN(self)
    self->tfs_handle->destroy();
    self->tfs_handle = NULL;
    Py_XDECREF(self->dict); 
    PyObject_GC_Del(self);
    Py_TRASHCAN_SAFE_END(self)
}

static int
_tfsclient_clear(TfsClientObject *self)
{
	self->tfs_handle->destroy();
    self->tfs_handle = NULL;
    Py_XDECREF(self->dict); 
    return 0;
}

static int
_tfsclient_traverse(TfsClientObject *self, visitproc visit, void *arg)
{
    return 0;
}

static char tfsclient_initialize_doc [] =
	"tfsclient.init('127.0.0.1:8108', cacheTimeBySeconds = 300, cacheItems = 500) -> True or False";
static PyObject *
tfsclient_initialize(TfsClientObject *self, PyObject *args)
{
	const char *ns_ip_port = NULL;
	int cache_time = 300;
	int cache_items = 500;
	int ret = 0;

	if (!PyArg_ParseTuple(args, "s|ii", &ns_ip_port, &cache_time, &cache_items))
		goto error;

	if (NULL == ns_ip_port){
		PyErr_SetString(PyExc_TypeError, "invalid arguments to initialize.");
		goto error;
	}


	ret = self->tfs_handle->initialize(ns_ip_port);
	if (TFS_SUCCESS != ret) {
		TBSYS_LOG(ERROR, "connect to name_server[%s] failed.", ns_ip_port);
		goto error;
	}

	Py_INCREF(Py_True);
	return Py_True;

error:
    Py_INCREF(Py_False);
    return Py_False;
}

static char tfsclient_open_doc [] =
	"open(file_name, suffix,  pytfs.WRITE_MODE|pytfs.READ_MODE,[appKey])"
	"file_name TFS文件名, 新建时传空None \n"
	"suffix    文件后缀，如果没有传空None \n"
	"mode      打开文件的模式: pytfs.READ_MODE, pytfs.WRITE_MODE\n"
	"appKey    2.0中新增的，暂不支持，不必传\n"
	"\tREAD_MODE 读\n"
	"\tWRITE_MODE 写\n"
		" --> fd ";
static PyObject *
tfsclient_open(TfsClientObject *self, PyObject *args)
{
    const char *file_name = NULL;
    const char  *suffix = NULL;
    PyObject *ofname = NULL;
    PyObject *osuffix = NULL;
    int fd = 0;
    int mode ;

    if (!PyArg_ParseTuple(args, "OOi:open", &ofname, &osuffix, &mode)){
        PyErr_SetString(PyExc_TypeError, "invalid arguments to open");
        goto error;
    }

    file_name = _check_str_obj(ofname);
    suffix = _check_str_obj(osuffix);

    if (self->tfs_handle == NULL){
    	TBSYS_LOG(ERROR, "tfs_handle is NULL");
    	goto error;
    }

    // TODO: 没处理appKey参数, 大文件要appKey
    fd = self->tfs_handle->open(file_name, suffix,(const char*)NULL, mode);
    if (fd > 0)
    {
        self->fd = fd;
        return Py_BuildValue("i", fd);
    }
    TBSYS_LOG(ERROR, "error to open. ret = %d", fd);
error:
	return Py_BuildValue("i", 0);
}

Py_ssize_t _write_buffer(TfsClient* tfsclient, int fd, const char* buff, Py_ssize_t len)
{
	Py_ssize_t left = len;
	int ret = 0;
	int wrote_size = 0;
	Py_ssize_t wrote = 0;

	while (left > 0) {
		wrote_size = left > WROTE_PRE_ONE ? WROTE_PRE_ONE : left;
		// 将buffer中的数据写入tfs
		ret = tfsclient->write(fd, (char*) ((buff + wrote)),
				wrote_size);
		if (ret >= left) {
			break;
		} else if (ret < 0) {
			TBSYS_LOG(ERROR, "write file error. ret = %d", ret);
			break;
		} else {
			// 若ret>0，则ret为实际写入的数据量
			wrote += ret;
			left -= ret;
		}
	}
	return wrote;
}

static int _tfs_write(TfsClientObject *self, int fd, PyObject* stream)
{
    char *buff = NULL;
    Py_ssize_t ret = 0;
    Py_ssize_t len = 0;

    if (0 != PyString_AsStringAndSize(stream, &buff, &len)){
        TBSYS_LOG(ERROR, "string is empty to write.");
        return -1;
    }

    if (self->fd <= 0 && fd <= 0){
    	TBSYS_LOG(ERROR, "didn't call function open.");
    	return -2;
    }
    ret = _write_buffer(self->tfs_handle, fd, buff, len);
    return ret > 0 ? 0:ret;
}

static char tfsclient_write_doc [] =
    "tfs.write(fd, str)\n"
    "must call open first. len(str) <= (2 * 1024 * 1024) well be better.\n"
    "return True if write success else return False";
static PyObject *
tfsclient_write(TfsClientObject *self, PyObject *args)
{
    PyObject *stream = NULL;
    int fd = 0;

    if (!PyArg_ParseTuple(args, "iO", &fd, &stream) && !PyString_Check(stream) && fd <= 0){
        PyErr_SetString(PyExc_TypeError, "invalid arguments to write.");
        goto error;
    }

    if (_tfs_write(self, fd, stream) > 0){
        Py_INCREF(Py_True);
        return Py_True;
    }

error:
    Py_INCREF(Py_False);
    return Py_False;
}

static char tfsclient_read_doc [] =
    "read(fd, count)\n"
    "must call open() first. \n"
	"tfs 提供接口参数来看，不可随机读，只能从头读count个字节，不能从任意位置读。\n"
	"故不建议使用此接口";
static PyObject *
tfsclient_read(TfsClientObject *self, PyObject *args)
{
    int fd = 0;
    int64_t count = 0;
	int read_size = 0;
	int64_t read = 0;
	int64_t left = 0;
    int ret = 0;
    int64_t file_length = 0;
    char* buffer = NULL;
    PyObject *pString = NULL;

    if (!PyArg_ParseTuple(args, "ii", &fd, &count) && fd >= 0 && count >= 0){
        PyErr_SetString(PyExc_TypeError, "invalid arguments to write.");
        goto error;
    }

    file_length = self->tfs_handle->get_file_length(fd);
    if (file_length <=0 ){
    	TBSYS_LOG(ERROR, "get file length fail, length = %d", file_length);
    	goto error;
    }

    count = file_length > count ? file_length: count;
    buffer = new char[count];
    if (NULL == buffer){
    	goto error;
    }

    left = count;

	while (read < count) {
		read_size = left > WROTE_PRE_ONE ? WROTE_PRE_ONE : left;
		ret = self->tfs_handle->read(fd, buffer + read, read_size);
		if (ret < 0) {
			break;
		} else {
			read += ret;
			left -= ret;
		}
	}

	if (ret < 0) {
		TBSYS_LOG(ERROR, "read file error! ret = %d", ret);
		delete[] buffer;
		goto error;
	}

	pString = (PyObject*) (PyString_FromStringAndSize(buffer, read));
    delete []buffer;
    buffer = NULL;
    return pString;
error:
    Py_INCREF(Py_False);
    return Py_False;
}


static char tfsclient_close_doc [] =
    "tfs.close(fd)\n Return (if succse tfs_name/None)";
static PyObject *
tfsclient_close(TfsClientObject *self, PyObject *args)
{
	int ret = TFS_SUCCESS;
	char ret_tfs_name[TFS_FILE_LEN];
	ret_tfs_name[0] = '\0';
	int fd = 0;

	if (!PyArg_ParseTuple(args, "i", &fd) && fd <= 0){
    	PyErr_SetString(PyExc_TypeError, "invalid arguments to close");
        TBSYS_LOG(ERROR, "invalid arguments to close");
        return Py_BuildValue(""); // return None
    }

	ret = self->tfs_handle->close(fd, ret_tfs_name, TFS_FILE_LEN);
	if (TFS_SUCCESS != ret) {
		TBSYS_LOG(ERROR, "close fail. ret = %d", ret);
		return Py_BuildValue(""); // return None
	}
	self->fd = 0;
    return (PyObject*)Py_BuildValue("s", ret_tfs_name);
}

static char tfsclient_unlink_doc [] =
    "unlink('Txxxxx', 'suffix', pytfs.DELETE) \n"
    "pytfs.DELETE, / UNDELETE / CONCEAL / REVEAL \n"
	"return file_size, which was delete.\n";

static PyObject *
tfsclient_unlink(TfsClientObject *self, PyObject *args)
{
    const char *file_name = NULL;
    const char  *suffix = NULL;
    PyObject *ofname = NULL;
    PyObject *osuffix = NULL;
    int64_t file_size = 0;
    int ret = 0;;
    TfsUnlinkType action = DELETE;

    if (!PyArg_ParseTuple(args, "OOi:open", &ofname, &action, &osuffix)){
        PyErr_SetString(PyExc_TypeError, "invalid arguments to unlink");
        return Py_BuildValue("i", 0);
    }

    file_name = _check_str_obj(ofname);
    suffix = _check_str_obj(osuffix);

    ret = self->tfs_handle->unlink(file_size, file_name, suffix, action);
    if (ret > 0)
    {
    	return Py_BuildValue("i", 0);
    }

    TBSYS_LOG(ERROR, "error to unlink. ret = %d", ret);
	return Py_BuildValue("i", file_size);
}

static char tfsclient_put_doc [] =
    "put(file_bin_stream_as_str)\n create a new file and save to tfs.\n "
    "Return success -> tfsname; error->False";
static PyObject *
tfsclient_put(TfsClientObject *self, PyObject *args)
{
    char *buff = NULL;
    long int nBufLen = 0;
    PyObject *obj = NULL;
;
    int fd = 0;
    char ret_tfs_name[TFS_FILE_LEN];
    ret_tfs_name[0] = '\0';
    int ret;

    if (!PyArg_ParseTuple(args, "O:put", &obj) && !PyString_Check(obj)){
    	PyErr_SetString(PyExc_TypeError, "invalid arguments to put");
        TBSYS_LOG(ERROR, "invalid arguments to put");
        goto error;
    }

    if (Py_None == obj)
    	return Py_BuildValue("");

	if (PyString_Check(obj))
	{
		if (0 == PyString_Size(obj) || 0 != PyString_AsStringAndSize(obj, &buff, &nBufLen))
			return Py_BuildValue("");
	}

	// TODO: 没处理Key参数, 大文件要appKey
	self->fd = fd = self->tfs_handle->open((char*)NULL, NULL, NULL, T_WRITE);
	if (fd <= 0){
		TBSYS_LOG(ERROR, "error to open tfs file ret = %d", fd);
		self->fd = 0;
		goto error;
	}

	ret  =_write_buffer(self->tfs_handle, fd, buff, nBufLen);

	// 读写失败
	if (ret < 0)
	{
	  TBSYS_LOG(ERROR, "write data error, ret = %d", ret);
	  goto error;
	}

	// 提交写入
	ret = self->tfs_handle->close(fd, ret_tfs_name, TFS_FILE_LEN);

	if (ret != TFS_SUCCESS) // 提交失败
	{
		TBSYS_LOG(ERROR, "write remote file failed, ret = %d", ret);
		goto error;
	}

	return Py_BuildValue("s", ret_tfs_name);

error:
    Py_INCREF(Py_False);
    return Py_False;
}

char* _read_buffer(TfsClient* tfsclent, int fd, int64_t& ret_length) {
	int ret = 0;
	int read_size = 0;
	int64_t left = 0;
	uint32_t crc = 0;
	TfsFileStat fstat;
	char* buffer = NULL;
	ret_length = 0;

	// 获得文件属性
	ret = tfsclent->fstat(fd, &fstat);
	if (ret != TFS_SUCCESS || fstat.size_ <= 0) {
		TBSYS_LOG(ERROR, "ret = %d, tfs.fstat failed", ret);
		return NULL;
	}

	buffer = new char[fstat.size_];
	left = fstat.size_;
	while (ret_length < fstat.size_) {
		read_size = left > WROTE_PRE_ONE ? WROTE_PRE_ONE : left;
		ret = tfsclent->read(fd, buffer + ret_length, read_size);
		if (ret < 0) {
			break;
		} else {
			crc = Func::crc(crc, (buffer + ret_length), ret); // 对读取的文件计算crc值
			ret_length += ret;
			left -= ret;
		}
	}

	if (ret < 0 || crc != fstat.crc_) {
		TBSYS_LOG(ERROR, "ret = %d, crc not math, crc = %d, fstat.crc_ = %d",
				ret, crc, fstat.crc_);
		delete[] buffer;
		buffer = NULL;
	}
	return buffer;
}

static char tfsclient_get_doc [] =
    "get(tfsname)\n create a new file and save to tfs.\n Return str";
static PyObject *
tfsclient_get(TfsClientObject *self, PyObject *args)
{
	int fd = 0;
    char *tfsname = NULL;
	char* buffer = NULL;
	int ret = 0;
	int64_t ret_length = 0;
    PyObject *obj = NULL;
    PyObject *pString = NULL;
    Py_ssize_t len = 0;

    if (!PyArg_ParseTuple(args, "O:get", &obj) && !PyString_Check(obj)) {
    	TBSYS_LOG(ERROR, "invalid arguments to get");
        goto error;
    }

    if (0 != PyString_AsStringAndSize(obj, &tfsname, &len) && TFS_FILE_LEN != len) {
    	TBSYS_LOG(ERROR, "wrong file name length.");
        goto error;
    }

    fd = self->tfs_handle->open(tfsname, NULL, T_READ, NULL);
    if (fd <= 0){
    	TBSYS_LOG(ERROR, "tfs.open failed, ret = %d", fd);
    	goto error;
    }

    buffer = _read_buffer(self->tfs_handle, fd, ret_length);
    if (NULL == buffer | 0 == ret_length)
    {
    	goto error;
    }

	ret = self->tfs_handle->close(fd);
	if (ret < 0) {
		TBSYS_LOG(ERROR, "close remote file error! ret = %d", ret);
		delete[] buffer;
		goto error;
	}

	pString = (PyObject*) (PyString_FromStringAndSize(buffer, ret_length));
    delete []buffer;
    buffer = NULL;
    return pString;

error:
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef pytfsobject_methods[] = {
    {"init", (PyCFunction)tfsclient_initialize, METH_VARARGS, tfsclient_initialize_doc},
    {"open", (PyCFunction)tfsclient_open, METH_VARARGS, tfsclient_open_doc},
    {"write", (PyCFunction)tfsclient_write, METH_VARARGS, tfsclient_write_doc},
    {"close", (PyCFunction)tfsclient_close, METH_VARARGS, tfsclient_close_doc}, //METH_NOARGS
    {"read", (PyCFunction)tfsclient_read, METH_VARARGS, tfsclient_read_doc},
    {"put", (PyCFunction)tfsclient_put, METH_VARARGS, tfsclient_put_doc},
    {"get", (PyCFunction)tfsclient_get, METH_VARARGS, tfsclient_get_doc},
    {"unlink", (PyCFunction)tfsclient_unlink, METH_VARARGS, tfsclient_unlink_doc},
    {NULL, NULL, 0, NULL}
};

// 2）方法列表
static PyMethodDef pytfsMethods[] =
{
   {"TfsClient", (PyCFunction)tfsclient_new, METH_NOARGS, tfsclient_new_doc},
   {"version", (PyCFunction)do_pytfs_version, METH_VARARGS, pytfs_version_doc},
   {"setloglevel", (PyCFunction)do_pytfs_setloglevel, METH_VARARGS, pytfs_setloglevel_doc},
   {NULL, NULL}
};
static int
_tfsclient_setattr(TfsClientObject *self, char *name, PyObject *v)
{
    if (v == NULL) {
        int rv = -1;
        if (self->dict != NULL)
            rv = PyDict_DelItemString(self->dict, name);
        if (rv < 0)
            PyErr_SetString(PyExc_AttributeError, "delete non-existing attribute");
        return rv;
    }
    if (self->dict == NULL) {
        self->dict = PyDict_New();
        if (self->dict == NULL)
            return -1;
    }
    return PyDict_SetItemString(self->dict, name, v);
}

static PyObject *
_tfsclient_getattr(TfsClientObject *self, char *name)
{
    PyObject *v = NULL;
    if (v == NULL && self->dict != NULL)
        v = PyDict_GetItemString(self->dict, name);

    if (v != NULL) {
        Py_INCREF(v);
        return v;
    }
    return Py_FindMethod(pytfsobject_methods, (PyObject *)self, name);
}
// 类定义
static PyTypeObject Pytfs_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "pytfs.TfsClient",	            /* tp_name */
    sizeof(TfsClientObject),    	/* tp_basicsize */
    0,                          /* tp_itemsize */
    /* Methods */
    (destructor)_tfsclient_dealloc,   /* tp_dealloc */
    0,                          /* tp_print */
    (getattrfunc)_tfsclient_getattr,    /* tp_getattr */
    (setattrfunc)_tfsclient_setattr,    /* tp_setattr */
    0,                          /* tp_compare */
    0,                          /* tp_repr */
    0,                          /* tp_as_number */
    0,                          /* tp_as_sequence */
    0,                          /* tp_as_mapping */
    0,                          /* tp_hash */
    0,                          /* tp_call */
    0,                          /* tp_str */
    0,                          /* tp_getattro */
    0,                          /* tp_setattro */
    0,                          /* tp_as_buffer */
    Py_TPFLAGS_HAVE_GC,         /* tp_flags */
    tfsclient_doc,              /* tp_doc */
    (traverseproc)_tfsclient_traverse, /* tp_traverse */
    (inquiry)_tfsclient_clear      /* tp_clear */
    /* More fields follow here, depending on your Python version. You can
     * safely ignore any compiler warnings about missing initializers.
     */
};

// 3) 初始化函数, 为C的dll导出函数
extern "C"
{
    DL_EXPORT(void) initpytfs()
    {	// default log level is ERROR

    	TBSYS_LOGGER.setLogLevel("ERROR");

    	Pytfs_Type.ob_type = &PyType_Type;
    	p_TfsClient_Type = &Pytfs_Type;
    	Pytfs_Type.tp_methods = pytfsobject_methods;

    	PyObject *module, *mods_dict;

    	module = Py_InitModule3("pytfs", pytfsMethods, module_doc);
        assert(module != NULL && PyModule_Check(module));

		/* Add error object to the module */
        mods_dict = PyModule_GetDict(module);
	    assert(mods_dict != NULL);
	    ErrorObject = PyErr_NewException("pytfs.TfsError", NULL, NULL);
	    assert(ErrorObject != NULL);
	    PyDict_SetItemString(mods_dict, "TfsError", ErrorObject);

	    //for tfs_open
	    PyDict_SetItemString(mods_dict, "READ", PyInt_FromLong(T_READ));
	    PyDict_SetItemString(mods_dict, "WRITE", PyInt_FromLong(T_WRITE));
	    PyDict_SetItemString(mods_dict, "DELETE", PyInt_FromLong(DELETE));
	    PyDict_SetItemString(mods_dict, "UNDELETE", PyInt_FromLong(UNDELETE));
	    PyDict_SetItemString(mods_dict, "CONCEAL", PyInt_FromLong(CONCEAL));
	    PyDict_SetItemString(mods_dict, "REVEAL", PyInt_FromLong(REVEAL));

//	    PyDict_SetItemString(mods_dict, "APPEND_MODE", PyInt_FromLong(APPEND_MODE));
//	    PyDict_SetItemString(mods_dict, "UNLINK_MODE", PyInt_FromLong(UNLINK_MODE));
//	    PyDict_SetItemString(mods_dict, "NEWBLK_MODE", PyInt_FromLong(NEWBLK_MODE));
	    //for link unlink
//	    PyDict_SetItemString(mods_dict, "DELETE", PyInt_FromLong(DELETE));
//	    PyDict_SetItemString(mods_dict, "UNDELETE", PyInt_FromLong(UNDELETE));
//	    PyDict_SetItemString(mods_dict, "CONCEAL", PyInt_FromLong(CONCEAL));
//	    PyDict_SetItemString(mods_dict, "REVEAL", PyInt_FromLong(REVEAL));

//	    PyDict_SetItemString(mods_dict, "T_SEEK_SET", PyInt_FromLong(T_SEEK_SET));
//	    PyDict_SetItemString(mods_dict, "T_SEEK_CUR", PyInt_FromLong(T_SEEK_CUR));
//	    PyDict_SetItemString(mods_dict, "T_SEEK_END", PyInt_FromLong(T_SEEK_END));

    }
}


