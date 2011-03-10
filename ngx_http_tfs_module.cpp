/*
 *
 *
 * help on:
 * http://www.jiajun.org/2010/10/06/nginx_module_development_part_2.html
 * */
extern "C" {
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
}

#include <tfs_client_api.h>

using namespace std;
using namespace tfs::client;
using namespace tfs::common;

#define DEFAULT_TFS_READ_WRITE_SIZE 2

static void* ngx_http_tfs_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_tfs_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static char* ngx_http_tfs_put(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char* ngx_http_tfs_get(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

typedef struct {
    ngx_str_t tfs_nsip; 		/* 字符串不要在_create_loc_conf中初始化，在_merge_loc_conf给默认值相当初始化 */
    size_t tfs_rb_buffer_size;
} ngx_http_tfs_ns_loc_conf_t;

static ngx_command_t  ngx_http_tfs_commands[] = {
    { ngx_string("tfs_put"),
      NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS, /* 不带参数 */
      ngx_http_tfs_put,
      0,
      0,
      NULL },

    { ngx_string("tfs_get"),
      NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS, /* 不带参数 */
      ngx_http_tfs_get,
      0,
      0,
      NULL },

    { ngx_string("tfs_nsip"),
      NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
      ngx_conf_set_str_slot	,            	/* 直接调用内置的字符串解释函数解释参数*/
      NGX_HTTP_LOC_CONF_OFFSET,         	/* 只在location中配置 */
      offsetof(ngx_http_tfs_ns_loc_conf_t, tfs_nsip),
      NULL },

    { ngx_string("tfs_rb_buffer_size"),		/* 每次读写tfs文件buffer大小  */
      NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_tfs_ns_loc_conf_t, tfs_rb_buffer_size),
      NULL },

      ngx_null_command
};

static ngx_http_module_t  ngx_http_tfs_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    ngx_http_tfs_create_loc_conf,  /* create location configuration  创建location配置时调用 */
    ngx_http_tfs_merge_loc_conf    /* merge location configuration  与location配置合并时调用 */
};

ngx_module_t  ngx_http_tfs_module = {
    NGX_MODULE_V1,
    &ngx_http_tfs_module_ctx, /* module context */
    ngx_http_tfs_commands,   /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_tfs_get_args_tfsname(ngx_http_request_t *r, u_char *ret)
{
	static const ngx_str_t TFSNAME_KEY = ngx_string("tfsname=");
	int i = 0;
	if(!r->args.len) {
		printf("ngx_tfs_mods: --- > args not match! 1 %s, %d\n", r->args.data, r->args.len);
		return NGX_ERROR;
	}

	if(ngx_strncasecmp(r->args.data, TFSNAME_KEY.data, TFSNAME_KEY.len) == 0) {

		ngx_cpystrn(ret, (r->args.data + TFSNAME_KEY.len), TFS_FILE_LEN);
		ret[TFS_FILE_LEN] = '\0';
		printf("ngx_tfs_mods: --- > tfs_name: %s\n", ret);
		return NGX_OK;
	}
	printf("ngx_tfs_mods: --- > args not match! 2 %s\n", ret + TFSNAME_KEY.len );
	return NGX_ERROR;
}

static ngx_int_t
ngx_http_tfs_get_handler(ngx_http_request_t *r)
{
    printf("called:ngx_http_tfs_put_handler\n");
    ngx_int_t     rc;
    ngx_buf_t    *b;
    ngx_chain_t   out;
    u_char tfsname[TFS_FILE_LEN + 1];
    ngx_http_tfs_ns_loc_conf_t  *cglcf;

    cglcf = (ngx_http_tfs_ns_loc_conf_t*)ngx_http_get_module_loc_conf(r, ngx_http_tfs_module);

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }
    if (r->headers_in.if_modified_since) {
        return NGX_HTTP_NOT_MODIFIED;
    }

    if( NGX_OK != ngx_http_tfs_get_args_tfsname(r, tfsname)) {
    	return NGX_DECLINED;
    }

    int ret = 0;
	TfsClient tfsclient;
	tfsclient.initialize((const char*)cglcf->tfs_nsip.data);

	// 打开待读写的文件
	ret = tfsclient.tfs_open((const char*)tfsname, NULL, READ_MODE);
	if (ret != TFS_SUCCESS)	{
		printf("open remote file %s error\n", b->pos);
		return NGX_DECLINED;
	}
	// 获得文件属性
	FileInfo finfo;
	ret = tfsclient.tfs_stat(&finfo);
	if (ret != TFS_SUCCESS || finfo.size_ <= 0)	{
		printf("get remote file info error\n");
		return NGX_DECLINED;
	}

	b = (ngx_buf_t *)ngx_create_temp_buf(r->pool, finfo.size_);

    if (b == NULL) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate response buffer.");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;
    b->last = b->pos + finfo.size_ ;
    b->memory = 1;
    b->last_buf = 1;

	int32_t read = 0;
 	int32_t read_size;
	uint32_t crc = 0;
	size_t left = finfo.size_;
	// 读取文件
	while (read < finfo.size_) {
		read_size = left > cglcf->tfs_rb_buffer_size ? cglcf->tfs_rb_buffer_size : left;
		ret = tfsclient.tfs_read((char*)b->pos + read, read_size);
		if (ret < 0) {
			break;
		}
		else {
			crc = TfsClient::crc(crc, (const char*)(b->pos + read), ret); // 对读取的文件计算crc值
			read += ret;
			left -= ret;
		}
	}

	if (ret < 0 || crc != finfo.crc_) {
		printf("read remote file error!\n");
		return NGX_DECLINED;
	}

	ret = tfsclient.tfs_close();
	if (ret < 0) {
		printf("close remote file error!");
		return NGX_DECLINED;
	}

    r->headers_out.content_type.len = sizeof("application/octet-stream") - 1;
    r->headers_out.content_type.data = (u_char *) "application/octet-stream";
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = finfo.size_;

    if (r->method == NGX_HTTP_HEAD) {
        rc = ngx_http_send_header(r);
        if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
            return rc;
        }
    }

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}

void ngx_http_tfs_cb_handler (ngx_http_request_t *r)
{
	printf("ngx_tfs_mods: --- > reading quest body..!\n");
}

static ngx_int_t
ngx_http_tfs_put_handler(ngx_http_request_t *r)
{ // 读取post数据，上传到tfs
    ngx_int_t     rc;
    ngx_buf_t    *b;
	ngx_chain_t   out;
	ngx_http_request_body_t		*rb;
	size_t						rb_size;
	int ret = 0;

	ngx_http_tfs_ns_loc_conf_t  *cglcf;
	static TfsClient tfsclient;

	if (!(r->method & NGX_HTTP_POST)) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
			"ngx_tfs_mods: request method must be POST.");
		return NGX_DECLINED;
	}
	//  必需先调用到个回调，读body数据
	rc = ngx_http_read_client_request_body(r, ngx_http_tfs_cb_handler);

	if(NULL == r->request_body || NULL == r->request_body->buf) {
		printf("ngx_tfs_mods: --- > request body is empty!\n");
		return NGX_HTTP_BAD_REQUEST;
	}

    cglcf = (ngx_http_tfs_ns_loc_conf_t*)ngx_http_get_module_loc_conf(r, ngx_http_tfs_module);

    //？ 应该不必每次调用 init
	tfsclient.initialize((const char*)cglcf->tfs_nsip.data);
	// 创建tfs客户端，并打开一个新的文件准备写入
	ret = tfsclient.tfs_open((char*)NULL, NULL, WRITE_MODE);
	if (ret != TFS_SUCCESS)	{
		printf("ngx_tfs_mods: create remote file error! nsip:%s, ret:%d \n", cglcf->tfs_nsip.data, ret);
		return NGX_HTTP_BAD_GATEWAY;
	}

	rb = r->request_body;
	rb_size = rb->buf->last - rb->buf->pos;

	if(rb_size < 0) {
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
					"ngx_tfs_mods: invalid content data");
		return NGX_HTTP_BAD_REQUEST;
	}

	size_t wrote = 0;
	size_t left = rb_size;
	int wrote_size;

	while (wrote < rb_size) {
		wrote_size = left > cglcf->tfs_rb_buffer_size ? cglcf->tfs_rb_buffer_size : left;
		// 将buffer中的数据写入tfs
		ret = tfsclient.tfs_write((char*)(rb->buf->pos + wrote), wrote_size);
		if (ret < 0 || ret >= left) {
			// 读写失败或完成
			break;
		}
		else {
			// 若ret>0，则ret为实际写入的数据量
			wrote += ret;
			left -= ret;
		}
	}

	// 读写失败
	if (ret < 0) {
		printf("write data error!\n");
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	// 提交写入
	ret = tfsclient.tfs_close();

	if (ret != TFS_SUCCESS)	{
		// 提交失败
		printf("ngx_tfs_mods: upload file error! ret = \n", ret);
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}
	else {
		b = ngx_create_temp_buf(r->pool, TFS_FILE_LEN);

		if (b == NULL) {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
				"ngx_tfs_mods: alloc memory fail (body_handler)");
			return NGX_HTTP_INTERNAL_SERVER_ERROR;
		}

		ngx_cpystrn(b->pos, (u_char*)tfsclient.get_file_name(), TFS_FILE_LEN);
		//b->temporary = 1;
		b->memory = 1;
		b->last_buf = 1;
		b->last = b->pos + TFS_FILE_LEN;

		out.buf = b;
		out.next = NULL;

		printf("write remote file:%s\n", (u_char*)b->pos);
	}

    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char *) "text/html";
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = FILE_NAME_LEN;

    rc = ngx_http_send_header(r);

	if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
		return rc;
	}

	ngx_http_output_filter(r, &out);
	ngx_http_finalize_request(r, rc);

	return NGX_OK;
}

static char *
ngx_http_tfs_put(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    printf("called:ngx_http_tfs_put.\n");
    ngx_http_core_loc_conf_t  *clcf;

    clcf = reinterpret_cast<ngx_http_core_loc_conf_t*>(
    			ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module));
    clcf->handler = ngx_http_tfs_put_handler;

    return NGX_CONF_OK;
}

static char *
ngx_http_tfs_get(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    printf("called:ngx_http_tfs_get\n");
    ngx_http_core_loc_conf_t  *clcf;

    clcf = reinterpret_cast<ngx_http_core_loc_conf_t*>(
    			ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module));
    clcf->handler = ngx_http_tfs_get_handler;
    return NGX_CONF_OK;
}

static void *
ngx_http_tfs_create_loc_conf(ngx_conf_t *cf)
{
    printf("called:ngx_http_tfs_create_loc_conf\n");
    ngx_http_tfs_ns_loc_conf_t  *conf;

    conf = (ngx_http_tfs_ns_loc_conf_t *)ngx_pcalloc(cf->pool, sizeof(ngx_http_tfs_ns_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->tfs_rb_buffer_size = (size_t)DEFAULT_TFS_READ_WRITE_SIZE;

    return conf;
}

static char *
ngx_http_tfs_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    printf("called:ngx_http_tfs_merge_loc_conf\n");
    ngx_http_tfs_ns_loc_conf_t *prev = (ngx_http_tfs_ns_loc_conf_t *)parent;
    ngx_http_tfs_ns_loc_conf_t *conf = (ngx_http_tfs_ns_loc_conf_t *)child;

    ngx_conf_merge_str_value(conf->tfs_nsip, prev->tfs_nsip, "127.0.0.1:10000");
    ngx_conf_merge_size_value(conf->tfs_rb_buffer_size, prev->tfs_rb_buffer_size, (size_t)DEFAULT_TFS_READ_WRITE_SIZE);

    return NGX_CONF_OK;
}

