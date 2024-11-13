#pragma once
#include <stdint.h>

/*
1. ���ܲ�ѯ������Ϣ���󣬷���json
2. ����vss��ع��ܵ�ָ��ɴ������ͷŴ��̻�ԭ�㣬���ؽ����json��Ϣ��
3. ���ܶ�ȡ����������ָ��1����ȡ���� 2��ƫ�� 3�����ȡ� �������ݰ�����״̬<�������>����������<������Ϣ���������ݡ�ѹ����־>�����ݳ��ȣ�
4. ���������������쳣��Զ��agentֱ�ӹرմ��̶����tcp���ӡ��ɿͻ��˸����Ѿ����յ����ݳ��ȣ����·����ȡ��������
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
#define read_chunk_maxsize (256 * 10240)	//2.5MB ����ÿ�ζ�ȡ����ÿ���С
#define recv_chunk_maxsize (1024*1024*3)	//3MB ѹ��ģʽ����ѹ���󳤶ȷ������󣬽��շ���Ҫ�����������

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