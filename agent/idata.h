#pragma once
#include <stdint.h>

/*
1. 接受查询磁盘信息请求，返回json
2. 接受vss相关功能的指令，可创建或释放磁盘还原点，返回结果的json信息；
3. 接受读取数据请求。需指定1）读取对象 2）偏移 3）长度。 返回数据包含（状态<错误代号>、数据类型<错误信息、磁盘数据、压缩标志>、数据长度）
4. 如果传输过程网络异常，远程agent直接关闭磁盘对象和tcp连接。由客户端根据已经接收的数据长度，重新发起读取数据请求。
*/

#pragma pack(push, 1)

#define IAMREADY "\xc5\xd5\x6f\xd9\x1b\x45\xe1\x0b\x38\xc4\x83\x3a\xdb\x71\xac\xc0"
#define SIZEOF_IAMREADY 16

struct drcmd_hdr {
	uint8_t cmd;
	uint32_t datalen;
};

struct drcmd_result
{
	uint8_t cmd;
	uint8_t err;
	uint8_t datatype;
	uint64_t datalen;
};

struct drcmd_open_disk_request {
	uint64_t pos;
	char name[0x100];
};
struct drcmd_seek_request {
	uint64_t pos;
};
struct drcmd_read_request {
	uint64_t size;
};
#define read_chunk_maxsize (256 * 10240)	//2.5MB 设置每次读取磁盘每块大小
#define recv_chunk_maxsize (1024*1024*3)	//3MB 压缩模式可能压缩后长度反而增大，接收方需要允许这种情况

struct drcmd_open_volume_request {
	uint32_t dwOptions;
	uint64_t pos;
	uint64_t size;
	char name[0x100];
};
struct drcmd_read_volume_request {
	uint64_t size;
};

#pragma pack(pop)

#define drDT_json           1
#define drDT_gzip          (1 << 1)
#define drDT_zeroblock     (1 << 2)
#define drDT_diskdata      (1 << 3)
#define drDT_zstd          (1 << 4)

enum d2v_rcmd{
	drcmd_reserved = 0,
	drcmd_query_pc_info,
	drcmd_query_disk_info,
	drcmd_open_disk,
	drcmd_seek_disk,
	drcmd_read_disk,
	drcmd_close_disk,
	drcmd_open_volume,
	drcmd_read_volume,
	drcmd_close_volume,
	drcmd_quit
};

enum drcmd_error{
	drerr_ok = 0,
	drerr_general,
};