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
			L"保存的目标文件已存在。",
			L"Destination file existed."
		);
		casexx(
			RID_DestEmpty,
			L"保存的目标不能为空。",
			L"Destination file is empty"
		);

		casexx(
			RID_SuffixNotSupported,
			L"文件名的后缀格式不支持。",
			L"File name suffix is not supported"
		);
		casexx(
			RID_UnkSourceSize,
			L"配置排除分区前，未设置源磁盘总大小。",
			L"Unknown size of source file"
		);
		casexx(
			RID_SetExcludeRangeError,
			L"无法配置排除分区，可能区间有冲突。",
			L"set exclude range error"
		);
		casexx(
			RID_SetRangeError,
			L"无法配置指定分区区间。",
			L"set include range error"
		);
		casexx(
			RID_ConvertorNotFound,
			L"找不到虚拟磁盘格式转换器。",
			L"not found vboxmanage"
		);
		casexx(
			RID_InitBufferFailed,
			L"初始化缓冲区失败。",
			L"init buffer failed."
		);
		casexx(
			RID_CreateDestFailed,
			L"创建目标文件失败。",
			L"create destination file faied."
		);
		casexx(
			RID_CreateWorkerProcessError,
			L"创建工作进程失败。",
			L"create worker process failed."
		);
		casexx(
			RID_CreateWorkerThreadError,
			L"创建工作线程失败。",
			L"create work thread failed"
		);
		casexx(
			RID_GetNextRangeError,
			L"获取下一个区间时异常。",
			L"get next range error."
		);
		casexx(
			RID_SetSourceFilePointerFaied,
			L"源磁盘指针调整失败。",
			L"set source file pointer failed."
		);
		casexx(
			RID_ReadSourceFaied,
			L"读取源磁盘数据失败。",
			L"read source file data failed."
		);
		casexx(
			RID_UnexpectedEOF,
			L"读取源磁盘数据未完整。",
			L"unexpected EOF reached."
		);
		casexx(
			RID_WriteFileError,
			L"写入文件失败。",
			L"writefile error."
		);

		casexx(
			RID_error,
			L"出错",
			L"Error"
		);
		casexx(
			RID_nodisks,
			L"没有磁盘",
			L"no disks"
		);
		casexx(
			RID_Disk,
			L"磁盘",
			L"Disk"
		);
		casexx(
			RID_BusType,
			L"Bus类型",
			L"BusType"
		);
		casexx(
			RID_Size,
			L"大小",
			L"Size"
		);
		casexx(
			RID_ChooseDisk,
			L"选择要做镜像的磁盘序号: ",
			L"select a disk number: "
		);
		casexx(
			RID_InvalidInput,
			L"无效输入",
			L"Invalid Input"
		);
		casexx(
			RID_NoPartitions,
			L"没有找到分区",
			L"No Partitions"
		);
		casexx(
			RID_partition,
			L"分区",
			L"Partition"
		);
		casexx(
			RID_label,
			L"标签",
			L"Label"
		);
		casexx(
			RID_Usage,
			L"使用",
			L"Usage"
		);
		casexx(
			RID_1stsector,
			L"扇区偏移",
			L"1st-sector"
		);
		casexx(
			RID_InputPartNumbersToBeExcluded,
			L"输入要排除做镜像的分区序号(多个用逗号分隔，留空表示全盘镜像): ",
			L"partition numbers to be excluded: "
		);
		casexx(
			RID_OpenSourceError,
			L"打开源盘失败",
			L"open source disk failed"
		);
		casexx(
			RID_InvalidPartNum,
			L"无效分区序号",
			L"invalid partition number"
		);
		casexx(
			RID_ExcludedParts,
			L"已选择排除分区%d, 扇区偏移范围(%I64d-%I64d)\n",
			L"excluded partition %d, 1st-sector(%I64d-%I64d)\n"
		);
		casexx(
			RID_SetOutputPath,
			L"设置保存镜像的路径，文件名可选扩展名有[.VMDK .DD .VDI .VHD]: ",
			L"Set output file path，with suffix[.VMDK .DD .VDI .VHD]: "
		);
		casexx(
			RID_SetImageOptions,
			L"设置镜像格式参数(标准=1, 预分配大小=2, 按2G分割=4)[留空默认为1]: ",
			L"Set output image options(Standard=1, Fixed=2, Split2G=4)[default=1]: "
		);
		casexx(
			RID_OnStart,
			L"准备开始...\n",
			L"Starting...\n"
		);
		casexx(
			RID_ConvertorThreadError,
			L"镜像格式转换器工作线程结果异常=%d        \n",
			L"convertor thread exit code=%d        \n"
		);
		casexx(
			RID_ErrorInfo,
			L"出错，%s\n",
			L"Error，%s\n"
		);
		casexx(
			RID_ConvertorError,
			L"出错: 转换器结果异常=%d, %s\n",
			L"Error: convertor error=%d, %s\n"
		);
		casexx(
			RID_GOOD,
			L"完成.\n",
			L"GOOD.\n"
		);

		casexx(
			RID_ConnectingHostPort,
			L"正在连接%S:%d...",
			L"connecting to %S:%d..."
		);
		casexx(
			RID_Failed_retry_5_seconds_later,
			L"连接失败，休息5秒...",
			L"failed，retry 5s later..."
		);
		casexx(
			RID_drcmd_query_pc_info,
			L"正在查询远程主机信息...",
			L"Querying remote host information..."
		);
		casexx(
			RID_drcmd_query_disk_info,
			L"正在查询远程磁盘信息...",
			L"Querying remote disk information..."
		);
		casexx(
			RID_drcmd_open_disk,
			L"正在请求访问远程磁盘%s...",
			L"Requesting access to remote disk%s..."
		);
		casexx(
			RID_drcmd_seek_disk,
			L"正在请求调整远程磁盘指针...",
			L"Requesting to adjust remote disk pointer..."
		);
		casexx(
			RID_drcmd_read_disk,
			L"正在请求读取远程磁盘数据...",
			L"Requesting to read remote disk data..."
		);
		casexx(
			RID_drcmd_open_volume,
			L"正在请求访问远程分区...",
			L"Requesting access to a remote partition..."
		);
		casexx(
			RID_drcmd_read_volume,
			L"正在请求读取远程分区数据...",
			L"Requesting to read remote partition data..."
		);

		casexx(
			RID_Partdlg_Title,
			L"分区信息 - 勾选想要做镜像的分区",
			L"Partitions - Check what you want to clone"
		);
		casexx(
			RID_PartIndex,
			L"分区序号",
			L"Part-Num."
		);
		casexx(
			RID_Lable,
			L"标签",
			L"Label"
		);
		casexx(
			RID_PartUsed,
			L"使用",
			L"Used"
		);
		casexx(
			RID_SectorOffset,
			L"扇区偏移",
			L"Sector offset"
		);
		casexx(
			RID_Bitlocker,
			L"Bitlocker",
			L"Bitlocker"
		);
		casexx(
			RID_Option_All_Space,
			L"选项-全部空间",
			L"All Space"
		);
		casexx(
			RID_Option_VSS,
			L"选项-VSS",
			L"VSS"
		);

		casexx(
			RID_DiskNum,
			L"磁盘序号",
			L"Disk-Num."
		);
		casexx(
			RID_OtherInfo,
			L"其他信息",
			L"Other info"
		);

		casexx(
			RID_Disk_Remotely_Tip1,
			L"[远程模式]列表中的项可双击进行配置该磁盘镜像的参数",
			L"[Remote Mode] Double-click an item in the list to configure the parameters of the disk image."
		);
		casexx(
			RID_Disk_Remotely_NoDisks,
			L"[远程模式]没有找到磁盘",
			L"[Remote Mode] Disk not found"
		);
		casexx(
			RID_Disk_Remotely_Unexpect,
			L"[远程模式]获取磁盘信息异常",
			L"[Remote Mode] Failed to retrieve disk information"
		);
		casexx(
			RID_Disk_Remotely_Error,
			L"[远程模式]获取磁盘信息出错",
			L"[Remote Mode] Error getting disk information"
		);
		casexx(
			RID_Disk_Locally_Tip1,
			L"列表中的项可双击进行配置该磁盘镜像的参数",
			L"Double-click an item above to configure how to clone the disk."
		);
		casexx(
			RID_Disk_Locally_NoDisks,
			L"没有找到磁盘",
			L"Disk not found"
		);
		casexx(
			RID_Disk_GetList,
			L"正在获取本地磁盘列表信息...",
			L"Getting local disk list information..."
		);
		casexx(
			RID_Disk_GetListFailed,
			L"获取本地磁盘列表信息失败。",
			L"Failed to obtain local disk list information."
		);
		casexx(
			RID_Disk_RemotelyGetList,
			L"[远程模式]正在获取磁盘列表信息...",
			L"[Remote Mode] Getting disk list information..."
		);
		casexx(
			RID_Disk_RemotelyGetListFailed,
			L"[远程模式]获取磁盘列表信息失败。",
			L"[Remote Mode] Failed to obtain disk list information."
		);
		casexx(
			RID_Disk_ConnectFailed,
			L"连接远程失败。",
			L"Failed to connect to remote."
		);
		casexx(
			RID_Disk_GetRemoteHostInfoFailed,
			L"获取远程主机信息失败。%s",
			L"Failed to obtain remote host information. %s"
		);
		casexx(
			RID_Disk_GetRemoteDiskInfoFailed,
			L"获取远程磁盘信息失败。%s",
			L"Failed to obtain remote disk information. %s"
		);
		casexx(
			RID_MakingImageDD,
			L"正在创建DD镜像...",
			L"Creating raw image..."
		);
		casexx(
			RID_MakingImage,
			L"正在创建镜像...",
			L"Creating image..."
		);
		casexx(
			RID_Part_NotConfig,
			L"未设置镜像保存路径",
			L"Creating image..."
		);
		casexx(
			RID_Part_FileExist,
			L"镜像保存路径已有文件",
			L"The image save path already has a file"
		);
		casexx(
			RID_Part_expect_unlock,
			L"有未解锁的加密Bitlocker磁盘分区",
			L"There is an encrypted Bitlocker partition that is not unlocked"
		);
		casexx(
			RID_Part_bitlock_unexpected,
			L"有异常状态的Bitlocker磁盘分区， ",
			L"There is an abnormal status of the Bitlocker disk partition,"
		);
		casexx(
			RID_UnableToEstOutputSize,
			L"无法评估输出镜像文件的大小",
			L"Unable to estimate the size of the output image file"
		);
		casexx(
			RID_TIPS,
			L"提示",
			L"Notice"
		);
		casexx(
			RID_DiskConfigError,
			L"错误信息：磁盘%d，%s",
			L"Error：Disk%d，%s"
		);
		casexx(
			RID_ImageOutputWarning1,
			L"错误信息：无法评估磁盘%d镜像保存位置的剩余存储空间, %s\n是否继续？",
			L"Error message: Unable to estimate the remaining storage space of disk %d where the image is saved, %s\nDo you want to continue?"
		);
		casexx(
			RID_ImageOutputWarning2,
			L"磁盘%d的镜像的保存位置有可能随着镜像文件增大，最后存储空间不足。\n是否继续？",
			L"The storage location of the image on disk %d may grow as the image file grows, and eventually the storage space will be insufficient. \nDo you want to continue?"
		);
		casexx(
			RID_ImageOutputWarning3,
			L"镜像的保存位置有可能随着镜像文件增大，最后存储空间不足。\n备份的数据量%S，目标存储剩余空间%S。\n是否继续？",
			L"The image storage location may grow as the image file grows, and eventually the storage space will be insufficient. \nThe amount of data backed up is %S, and the remaining space in the target storage is %S. \nDo you want to continue?"
		);
		casexx(
			RID_NoDisksChecked,
			L"没有勾选任何一项磁盘",
			L"No disk is checked"
		);
		casexx(
			RID_ImageTaskStarted,
			L"镜像磁盘%d任务开始 ..",
			L"Clone disk %d task started.."
		);
		casexx(
			RID_SureToStop,
			L"确定要停止吗？",
			L"Are you sure you want to stop?"
		);
		casexx(
			RID_PreparingCloneNext,
			L"准备镜像磁盘%d...",
			L"Preparing to clone disk %d..."
		);
		casexx(
			RID_WaitForCompletion,
			L"等待结束...",
			L"Waiting for completion..."
		);
		casexx(
			RID_ErrorString,
			L"错误信息：%s",
			L"Error Info: %s"
		);
		casexx(
			RID_OpenVolErrorComma,
			L"打开分区出错，",
			L"Error opening volume, "
		);
		casexx(
			RID_ShouldRunX64Version,
			L"当前系统是64位，请使用本程序的64位版本",
			L"The current system is 64-bit, please use the 64-bit version of this program"
		);
		casexx(
			RID_OpenVSSVolFailed,
			L"打开VSS分区失败,",
			L"Failed to open VSS partition,"
		);
		casexx(
			RID_GetVSSVolPathFailed,
			L"获取VSS分区路径失败,",
			L"Failed to obtain the VSS partition path."
		);
		casexx(
			RID_ImagePathNotSet,
			L"未配置镜像保存路径。",
			L"The image save path is not configured."
		);
		casexx(
			RID_GettingStarted,
			L"准备开始...",
			L"Getting Started..."
		);
		casexx(
			RID_Speed_Remaining,
			L"速度 %S/s， 预计剩余时间 %S",
			L"Speed %S/s, estimated remaining time %S"
		);
		casexx(
			RID_ImageMaker_End_OK,
			L"完成, %S",
			L"Done, %S"
		);
		casexx(
			RID_ImageMaker_End_Err,
			L"出错，错误码%d, %s",
			L"Error，errcode%d, %s"
		);
		casexx(
			RID_VSS_init,
			L"正在初始化VSS...",
			L"Initializing VSS..."
		);
		casexx(
			RID_VSS_init_err,
			L"出错, 初始化VSS失败",
			L"Error, failed to initialize VSS"
		);
		casexx(
			RID_VSS_BeforeStartSnapshot,
			L"准备开始创建还原点...",
			L"Getting started creating restore points..."
		);
		casexx(
			RID_VSS_StartSnapshotErr,
			L"出错, 准备创建还原点失败",
			L"Error, failed to create restore point"
		);
		casexx(
			RID_VSS_AddVol,
			L"设置VSS分区",
			L"Adding VSS Volume"
		);
		casexx(
			RID_VSS_AddVolErr,
			L"失败",
			L"Failed"
		);
		casexx(
			RID_VSS_BeforeMakeSnapshot,
			L"创建还原点...",
			L"Create a restore point..."
		);
		casexx(
			RID_VSS_MakeSnapshotErr,
			L"出错，创建还原点失败0x",
			L"Error, failed to create restore point 0x"
		);
		casexx(
			RID_VSS_SnapshotDone,
			L"创建还原点成功",
			L"Restore point created successfully"
		);
		casexx(
			RID_SaveFileDlgTitle,
			L"保存文件到...",
			L"Save file to..."
		);
		casexx(
			RID_FileExistAndDelFailed,
			L"输出路径已经有文件，且无法删除。",
			L"The output path already contains a file and cannot be deleted."
		);
		casexx(
			RID_InvalidFileExt,
			L"输出路径的后缀名无效",
			L"The output path suffix is invalid"
		);
		casexx(
			RID_NotAllowedChar,
			L"当前设置要保存的镜像的文件名\n    \"%s\"\n包含了不能接受的中文或其他非英文字符。",
			L"The current setting for saving the image file name \n \"%s\"\n contains unacceptable non-English characters."
		);
		casexx(
			RID_insufficient_diskspace_for_rawimg,
			L"DD镜像的保存位置存储空间不足。",
			L"The storage space for saving the DD image is insufficient."
		);
		casexx(
			RID_uncheck_part_warning1,
			L"这个分区如果不勾选，制作的硬盘镜像中的操作系统可能无法在虚拟机中引导启动。\n这是个隐藏分区，不是用户可以存取数据的分区，且数据位于系统盘前面，其作用应该和引导启动系统盘%s中的操作系统有关，确定要去掉勾选吗？（如果你不清楚这个分区的数据，请按取消按钮）",
			L"If this partition is not checked, the operating system in the created hard disk image may not be able to boot in the virtual machine. \nThis is a hidden partition, not a partition where users can access data, and the data is located in front of the system disk. Its function should be related to booting the operating system in the system disk %s. Are you sure you want to uncheck it? (If you are not sure about the data of this partition, please press the Cancel button)"
		);
		casexx(
			RID_uncheck_part_warning2,
			L"这个分区（%s）是安装了操作系统的分区，确定要去掉勾选吗？",
			L"This partition (%s) is where the operating system is installed. Are you sure you want to uncheck it?"
		);
		casexx(
			RID_disk_config_exmode_excluded_parts,
			L"排除了分区[%s]",
			L"Excluded partitions[%s]"
		);
		casexx(
			RID_disk_config_exmode_onlyused,
			L"非全盘镜像",
			L"only used space"
		);
		casexx(
			RID_disk_config_exmode_allspace,
			L"全盘镜像",
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
