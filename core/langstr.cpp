#include "pch.h"
#include <assert.h>
#include "langstr.h"

const int default_lang = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

#define strxx(dl, en) {\
    if (lid == default_lang) { return dl; }\
    else { return en; }\
}

#define casexx(i, dl, en) \
case i:\
strxx(\
dl, \
en)

LPCWSTR LSTRW_X(int i, int lid)
{
	switch (i) {
		casexx(
			RID_DestFileExist,
			L"�����Ŀ���ļ��Ѵ��ڡ�",
			L"Destination file existed."
		);
		casexx(
			RID_DestEmpty,
			L"�����Ŀ�겻��Ϊ�ա�",
			L"Destination file is empty"
		);

		casexx(
			RID_SuffixNotSupported,
			L"�ļ����ĺ�׺��ʽ��֧�֡�",
			L"File name suffix is not supported"
		);
		casexx(
			RID_UnkSourceSize,
			L"�����ų�����ǰ��δ����Դ�����ܴ�С��",
			L"Unknown size of source file"
		);
		casexx(
			RID_SetExcludeRangeError,
			L"�޷������ų����������������г�ͻ��",
			L"set exclude range error"
		);
		casexx(
			RID_SetRangeError,
			L"�޷�����ָ���������䡣",
			L"set include range error"
		);
		casexx(
			RID_ConvertorNotFound,
			L"�Ҳ���������̸�ʽת������",
			L"not found vboxmanage"
		);
		casexx(
			RID_InitBufferFailed,
			L"��ʼ��������ʧ�ܡ�",
			L"init buffer failed."
		);
		casexx(
			RID_CreateDestFailed,
			L"����Ŀ���ļ�ʧ�ܡ�",
			L"create destination file faied."
		);
		casexx(
			RID_CreateWorkerProcessError,
			L"������������ʧ�ܡ�",
			L"create worker process failed."
		);
		casexx(
			RID_CreateWorkerThreadError,
			L"���������߳�ʧ�ܡ�",
			L"create work thread failed"
		);
		casexx(
			RID_GetNextRangeError,
			L"��ȡ��һ������ʱ�쳣��",
			L"get next range error."
		);
		casexx(
			RID_SetSourceFilePointerFaied,
			L"Դ����ָ�����ʧ�ܡ�",
			L"set source file pointer failed."
		);
		casexx(
			RID_ReadSourceFaied,
			L"��ȡԴ��������ʧ�ܡ�",
			L"read source file data failed."
		);
		casexx(
			RID_UnexpectedEOF,
			L"��ȡԴ��������δ������",
			L"unexpected EOF reached."
		);
		casexx(
			RID_WriteFileError,
			L"д���ļ�ʧ�ܡ�",
			L"writefile error."
		);

		casexx(
			RID_error,
			L"����",
			L"Error"
		);
		casexx(
			RID_nodisks,
			L"û�д���",
			L"no disks"
		);
		casexx(
			RID_Disk,
			L"����",
			L"Disk"
		);
		casexx(
			RID_BusType,
			L"Bus����",
			L"BusType"
		);
		casexx(
			RID_Size,
			L"��С",
			L"Size"
		);
		casexx(
			RID_ChooseDisk,
			L"ѡ��Ҫ������Ĵ������: ",
			L"select a disk number: "
		);
		casexx(
			RID_InvalidInput,
			L"��Ч����",
			L"Invalid Input"
		);
		casexx(
			RID_NoPartitions,
			L"û���ҵ�����",
			L"No Partitions"
		);
		casexx(
			RID_partition,
			L"����",
			L"Partition"
		);
		casexx(
			RID_label,
			L"��ǩ",
			L"Label"
		);
		casexx(
			RID_Usage,
			L"ʹ��",
			L"Usage"
		);
		casexx(
			RID_1stsector,
			L"����ƫ��",
			L"1st-sector"
		);
		casexx(
			RID_InputPartNumbersToBeExcluded,
			L"����Ҫ�ų�������ķ������(����ö��ŷָ������ձ�ʾȫ�̾���): ",
			L"partition numbers to be excluded: "
		);
		casexx(
			RID_OpenSourceError,
			L"��Դ��ʧ��",
			L"open source disk failed"
		);
		casexx(
			RID_InvalidPartNum,
			L"��Ч�������",
			L"invalid partition number"
		);
		casexx(
			RID_ExcludedParts,
			L"��ѡ���ų�����%d, ����ƫ�Ʒ�Χ(%I64d-%I64d)\n",
			L"excluded partition %d, 1st-sector(%I64d-%I64d)\n"
		);
		casexx(
			RID_SetOutputPath,
			L"���ñ��澵���·�����ļ�����ѡ��չ����[.VMDK .DD .VDI .VHD]: ",
			L"Set output file path��with suffix[.VMDK .DD .VDI .VHD]: "
		);
		casexx(
			RID_SetImageOptions,
			L"���þ����ʽ����(��׼=1, Ԥ�����С=2, ��2G�ָ�=4)[����Ĭ��Ϊ1]: ",
			L"Set output image options(Standard=1, Fixed=2, Split2G=4)[default=1]: "
		);
		casexx(
			RID_OnStart,
			L"׼����ʼ...\n",
			L"Starting...\n"
		);
		casexx(
			RID_ConvertorThreadError,
			L"�����ʽת���������߳̽���쳣=%d        \n",
			L"convertor thread exit code=%d        \n"
		);
		casexx(
			RID_ErrorInfo,
			L"����%s\n",
			L"Error��%s\n"
		);
		casexx(
			RID_ConvertorError,
			L"����: ת��������쳣=%d, %s\n",
			L"Error: convertor error=%d, %s\n"
		);
		casexx(
			RID_GOOD,
			L"���.\n",
			L"GOOD.\n"
		);

		casexx(
			RID_ConnectingHostPort,
			L"��������%S:%d...",
			L"connecting to %S:%d..."
		);
		casexx(
			RID_Failed_retry_5_seconds_later,
			L"����ʧ�ܣ���Ϣ5��...",
			L"failed��retry 5s later..."
		);
		casexx(
			RID_drcmd_query_pc_info,
			L"���ڲ�ѯԶ��������Ϣ...",
			L"Querying remote host information..."
		);
		casexx(
			RID_drcmd_query_disk_info,
			L"���ڲ�ѯԶ�̴�����Ϣ...",
			L"Querying remote disk information..."
		);
		casexx(
			RID_drcmd_open_disk,
			L"�����������Զ�̴���%s...",
			L"Requesting access to remote disk%s..."
		);
		casexx(
			RID_drcmd_seek_disk,
			L"�����������Զ�̴���ָ��...",
			L"Requesting to adjust remote disk pointer..."
		);
		casexx(
			RID_drcmd_read_disk,
			L"���������ȡԶ�̴�������...",
			L"Requesting to read remote disk data..."
		);
		casexx(
			RID_drcmd_open_volume,
			L"�����������Զ�̷���...",
			L"Requesting access to a remote partition..."
		);
		casexx(
			RID_drcmd_read_volume,
			L"���������ȡԶ�̷�������...",
			L"Requesting to read remote partition data..."
		);

		casexx(
			RID_Partdlg_Title,
			L"������Ϣ - ��ѡ��Ҫ������ķ���",
			L"Partitions - Check what you want to clone"
		);
		casexx(
			RID_PartIndex,
			L"�������",
			L"Part-Num."
		);
		casexx(
			RID_Lable,
			L"��ǩ",
			L"Label"
		);
		casexx(
			RID_PartUsed,
			L"ʹ��",
			L"Used"
		);
		casexx(
			RID_SectorOffset,
			L"����ƫ��",
			L"Sector offset"
		);
		casexx(
			RID_Bitlocker,
			L"Bitlocker",
			L"Bitlocker"
		);
		casexx(
			RID_Option_All_Space,
			L"ѡ��-ȫ���ռ�",
			L"All Space"
		);
		casexx(
			RID_Option_VSS,
			L"ѡ��-VSS",
			L"VSS"
		);

		casexx(
			RID_DiskNum,
			L"�������",
			L"Disk-Num."
		);
		casexx(
			RID_OtherInfo,
			L"������Ϣ",
			L"Other info"
		);

		casexx(
			RID_Disk_Remotely_Tip1,
			L"[Զ��ģʽ]�б��е����˫���������øô��̾���Ĳ���",
			L"[Remote Mode] Double-click an item in the list to configure the parameters of the disk image."
		);
		casexx(
			RID_Disk_Remotely_NoDisks,
			L"[Զ��ģʽ]û���ҵ�����",
			L"[Remote Mode] Disk not found"
		);
		casexx(
			RID_Disk_Remotely_Unexpect,
			L"[Զ��ģʽ]��ȡ������Ϣ�쳣",
			L"[Remote Mode] Failed to retrieve disk information"
		);
		casexx(
			RID_Disk_Remotely_Error,
			L"[Զ��ģʽ]��ȡ������Ϣ����",
			L"[Remote Mode] Error getting disk information"
		);
		casexx(
			RID_Disk_Locally_Tip1,
			L"�б��е����˫���������øô��̾���Ĳ���",
			L"Double-click an item above to configure how to clone the disk."
		);
		casexx(
			RID_Disk_Locally_NoDisks,
			L"û���ҵ�����",
			L"Disk not found"
		);
		casexx(
			RID_Disk_GetList,
			L"���ڻ�ȡ���ش����б���Ϣ...",
			L"Getting local disk list information..."
		);
		casexx(
			RID_Disk_GetListFailed,
			L"��ȡ���ش����б���Ϣʧ�ܡ�",
			L"Failed to obtain local disk list information."
		);
		casexx(
			RID_Disk_RemotelyGetList,
			L"[Զ��ģʽ]���ڻ�ȡ�����б���Ϣ...",
			L"[Remote Mode] Getting disk list information..."
		);
		casexx(
			RID_Disk_RemotelyGetListFailed,
			L"[Զ��ģʽ]��ȡ�����б���Ϣʧ�ܡ�",
			L"[Remote Mode] Failed to obtain disk list information."
		);
		casexx(
			RID_Disk_ConnectFailed,
			L"����Զ��ʧ�ܡ�",
			L"Failed to connect to remote."
		);
		casexx(
			RID_Disk_GetRemoteHostInfoFailed,
			L"��ȡԶ��������Ϣʧ�ܡ�%s",
			L"Failed to obtain remote host information. %s"
		);
		casexx(
			RID_Disk_GetRemoteDiskInfoFailed,
			L"��ȡԶ�̴�����Ϣʧ�ܡ�%s",
			L"Failed to obtain remote disk information. %s"
		);
		casexx(
			RID_MakingImageDD,
			L"���ڴ���DD����...",
			L"Creating raw image..."
		);
		casexx(
			RID_MakingImage,
			L"���ڴ�������...",
			L"Creating image..."
		);
		casexx(
			RID_Part_NotConfig,
			L"δ���þ��񱣴�·��",
			L"Creating image..."
		);
		casexx(
			RID_Part_FileExist,
			L"���񱣴�·�������ļ�",
			L"The image save path already has a file"
		);
		casexx(
			RID_Part_expect_unlock,
			L"��δ�����ļ���Bitlocker���̷���",
			L"There is an encrypted Bitlocker partition that is not unlocked"
		);
		casexx(
			RID_Part_bitlock_unexpected,
			L"���쳣״̬��Bitlocker���̷����� ",
			L"There is an abnormal status of the Bitlocker disk partition,"
		);
		casexx(
			RID_UnableToEstOutputSize,
			L"�޷�������������ļ��Ĵ�С",
			L"Unable to estimate the size of the output image file"
		);
		casexx(
			RID_TIPS,
			L"��ʾ",
			L"Notice"
		);
		casexx(
			RID_DiskConfigError,
			L"������Ϣ������%d��%s",
			L"Error��Disk%d��%s"
		);
		casexx(
			RID_ImageOutputWarning1,
			L"������Ϣ���޷���������%d���񱣴�λ�õ�ʣ��洢�ռ�, %s\n�Ƿ������",
			L"Error message: Unable to estimate the remaining storage space of disk %d where the image is saved, %s\nDo you want to continue?"
		);
		casexx(
			RID_ImageOutputWarning2,
			L"����%d�ľ���ı���λ���п������ž����ļ��������洢�ռ䲻�㡣\n�Ƿ������",
			L"The storage location of the image on disk %d may grow as the image file grows, and eventually the storage space will be insufficient. \nDo you want to continue?"
		);
		casexx(
			RID_ImageOutputWarning3,
			L"����ı���λ���п������ž����ļ��������洢�ռ䲻�㡣\n���ݵ�������%S��Ŀ��洢ʣ��ռ�%S��\n�Ƿ������",
			L"The image storage location may grow as the image file grows, and eventually the storage space will be insufficient. \nThe amount of data backed up is %S, and the remaining space in the target storage is %S. \nDo you want to continue?"
		);
		casexx(
			RID_NoDisksChecked,
			L"û�й�ѡ�κ�һ�����",
			L"No disk is checked"
		);
		casexx(
			RID_ImageTaskStarted,
			L"�������%d����ʼ ..",
			L"Clone disk %d task started.."
		);
		casexx(
			RID_SureToStop,
			L"ȷ��Ҫֹͣ��",
			L"Are you sure you want to stop?"
		);
		casexx(
			RID_PreparingCloneNext,
			L"׼���������%d...",
			L"Preparing to clone disk %d..."
		);
		casexx(
			RID_WaitForCompletion,
			L"�ȴ�����...",
			L"Waiting for completion..."
		);
		casexx(
			RID_ErrorString,
			L"������Ϣ��%s",
			L"Error Info: %s"
		);
		casexx(
			RID_OpenVolErrorComma,
			L"�򿪷�������",
			L"Error opening volume, "
		);
		casexx(
			RID_ShouldRunX64Version,
			L"��ǰϵͳ��64λ����ʹ�ñ������64λ�汾",
			L"The current system is 64-bit, please use the 64-bit version of this program"
		);
		casexx(
			RID_OpenVSSVolFailed,
			L"��VSS����ʧ��,",
			L"Failed to open VSS partition,"
		);
		casexx(
			RID_GetVSSVolPathFailed,
			L"��ȡVSS����·��ʧ��,",
			L"Failed to obtain the VSS partition path."
		);
		casexx(
			RID_ImagePathNotSet,
			L"δ���þ��񱣴�·����",
			L"The image save path is not configured."
		);
		casexx(
			RID_GettingStarted,
			L"׼����ʼ...",
			L"Getting Started..."
		);
		casexx(
			RID_Speed_Remaining,
			L"�ٶ� %S/s�� Ԥ��ʣ��ʱ�� %S",
			L"Speed %S/s, estimated remaining time %S"
		);
		casexx(
			RID_ImageMaker_End_OK,
			L"���, %S",
			L"Done, %S"
		);
		casexx(
			RID_ImageMaker_End_Err,
			L"����������%d, %s",
			L"Error��errcode%d, %s"
		);
		casexx(
			RID_VSS_init,
			L"���ڳ�ʼ��VSS...",
			L"Initializing VSS..."
		);
		casexx(
			RID_VSS_init_err,
			L"����, ��ʼ��VSSʧ��",
			L"Error, failed to initialize VSS"
		);
		casexx(
			RID_VSS_BeforeStartSnapshot,
			L"׼����ʼ������ԭ��...",
			L"Getting started creating restore points..."
		);
		casexx(
			RID_VSS_StartSnapshotErr,
			L"����, ׼��������ԭ��ʧ��",
			L"Error, failed to create restore point"
		);
		casexx(
			RID_VSS_AddVol,
			L"����VSS����",
			L"Adding VSS Volume"
		);
		casexx(
			RID_VSS_AddVolErr,
			L"ʧ��",
			L"Failed"
		);
		casexx(
			RID_VSS_BeforeMakeSnapshot,
			L"������ԭ��...",
			L"Create a restore point..."
		);
		casexx(
			RID_VSS_MakeSnapshotErr,
			L"����������ԭ��ʧ��0x",
			L"Error, failed to create restore point 0x"
		);
		casexx(
			RID_VSS_SnapshotDone,
			L"������ԭ��ɹ�",
			L"Restore point created successfully"
		);
		casexx(
			RID_SaveFileDlgTitle,
			L"�����ļ���...",
			L"Save file to..."
		);
		casexx(
			RID_FileExistAndDelFailed,
			L"���·���Ѿ����ļ������޷�ɾ����",
			L"The output path already contains a file and cannot be deleted."
		);
		casexx(
			RID_InvalidFileExt,
			L"���·���ĺ�׺����Ч",
			L"The output path suffix is invalid"
		);
		casexx(
			RID_NotAllowedChar,
			L"��ǰ����Ҫ����ľ�����ļ���\n    \"%s\"\n�����˲��ܽ��ܵ����Ļ�������Ӣ���ַ���",
			L"The current setting for saving the image file name \n \"%s\"\n contains unacceptable non-English characters."
		);
		casexx(
			RID_insufficient_diskspace_for_rawimg,
			L"DD����ı���λ�ô洢�ռ䲻�㡣",
			L"The storage space for saving the DD image is insufficient."
		);
		casexx(
			RID_uncheck_part_warning1,
			L"��������������ѡ��������Ӳ�̾����еĲ���ϵͳ�����޷��������������������\n���Ǹ����ط����������û����Դ�ȡ���ݵķ�����������λ��ϵͳ��ǰ�棬������Ӧ�ú���������ϵͳ��%s�еĲ���ϵͳ�йأ�ȷ��Ҫȥ����ѡ�𣿣�����㲻���������������ݣ��밴ȡ����ť��",
			L"If this partition is not checked, the operating system in the created hard disk image may not be able to boot in the virtual machine. \nThis is a hidden partition, not a partition where users can access data, and the data is located in front of the system disk. Its function should be related to booting the operating system in the system disk %s. Are you sure you want to uncheck it? (If you are not sure about the data of this partition, please press the Cancel button)"
		);
		casexx(
			RID_uncheck_part_warning2,
			L"���������%s���ǰ�װ�˲���ϵͳ�ķ�����ȷ��Ҫȥ����ѡ��",
			L"This partition (%s) is where the operating system is installed. Are you sure you want to uncheck it?"
		);
		casexx(
			RID_disk_config_exmode_excluded_parts,
			L"�ų��˷���[%s]",
			L"Excluded partitions[%s]"
		);
		casexx(
			RID_disk_config_exmode_onlyused,
			L"��ȫ�̾���",
			L"only used space"
		);
		casexx(
			RID_disk_config_exmode_allspace,
			L"ȫ�̾���",
			L"fully clone"
		);
	default:
		assert(false);
	}
	return L"";
}

LPCWSTR LSTRW(int i)
{
	LANGID lid = GetUserDefaultUILanguage();
	return LSTRW_X(i, lid);
}
