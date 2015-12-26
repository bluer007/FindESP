
#include <Windows.h>  
#include <Winbase.h>  //the head file of GetFirmwareEnvironmentVariableA and GetFirmwareType 
#include <iostream>  
#include <TCHAR.H>
#include <setupapi.h>	//the head file of Setup API 
#include <vector>
#include <map>
#pragma comment( lib, "setupapi.lib" )
using namespace std;

#ifdef UNICODE
#define tcout std::wcout
#define tstring std::wstring
#else
#define tcout std::cout
#define tstring std::string
#endif


//return code
typedef enum
{
	MYERROR_SUCCESS = 0,
	MYERROR_FAIL,
	MYERROR_INVALID_PARAMETER,
	MYERROR_NONE_PARAMETER,
	MYERROR_NOT_HARD_DISK,
	MYERROR_CANNOT_GET_DISK_NUMBER,
	MYERROR_CANNOT_GET_ESP_INFO,
	MYERROR_NOT_EXIST_DRIVE,
	MYERROR_NOT_EXIST_DISK,
	MYERROR_CANNOT_FIND_ESP
}ERROR_CODE;

enum 
{
	MAX_DISK_NUM = 32,
	MAX_FILE_SYSTEM_NAME_SIZE = 10
};


//the struct of all disks information
typedef struct
{
	//total number of disk, such as 2, 3..
	int diskTotal;
	/*
	a array indicates each disk number, 
	for example if aryDisk[0] == 0 indicates disk0 is valid;
	if aryDisk[1] == 2 indicates disk2 is invalid
	*/
	int aryDisk[MAX_DISK_NUM];
}DiskInfo;

DiskInfo diskInfo = {0};

#define DEVICE_NUMBER(diskNum, partitionNum)	(((DWORD)diskNum << 16) | (DWORD)partitionNum)
#define DISK_NUMBER(deviceNum)	(((DWORD)deviceNum >> 16) & 0x0000FFFF)
#define PARTITION_NUMBER(deviceNum)	((DWORD)deviceNum & 0x0000FFFF)
typedef DWORD DEVNUM;	//define new type. use to record disk number and partition number


typedef struct 
{
	union
	{
		DEVNUM deviceNum;
		struct
		{
			DWORD diskNum;
			DWORD partitionNum;
		};
	};
	TCHAR fileSystem[MAX_FILE_SYSTEM_NAME_SIZE];
	TCHAR mountDrive[3];
}PartitionInfo;

//record partition information, such as file system name, disk and partition number
map <DEVNUM, PartitionInfo> partitionInfo;

//record information of all ESP partitions
map <DEVNUM, PartitionInfo> allESP;

LPCTSTR ErrorStrArray[] =
{
	//MYERROR_SUCCESS
	TEXT(""),
	//MYERROR_FAIL
	TEXT("error: can't mount ESP partitions.\n"),
	//MYERROR_INVALID_PARAMETER
	TEXT("error: invalid parameters.\n"),
	//MYERROR_NONE_PARAMETER
	TEXT("error: none parameters.\n"),
	//MYERROR_NOT_HARD_DISK
	TEXT("error: drive entered is not a hard disk drive.\n"),
	//MYERROR_CANNOT_GET_DISK_NUMBER
	TEXT("error: can't get disk number, maybe you can run as admin.\n"),
	//MYERROR_CANNOT_GET_ESP_INFO
	TEXT("error: can't get information of ESP partitions.\n"),
	//MYERROR_NOT_EXIST_DRIVE,
	TEXT("error: input a drive that does not exist.\n"),
	//MYERROR_NOT_EXIST_DISK
	TEXT("error: input a disk that does not exist.\n"),
	//MYERROR_CANNOT_FIND_ESP
	TEXT("error: can't find ESP partitions.\n")
};

typedef int(*ParameterFun)(LPCTSTR arg1);

typedef struct 
{
	LPCTSTR cmd;
	ParameterFun fun;
	LPCTSTR arg1;
}ParameterEntry;

bool GetAllDiskNum();		//enum the disk to get all disk information
HANDLE GetDiskHandleByNum(int num);	//get disk handle by disk number
bool GetDeviceNumber(HANDLE handle, STORAGE_DEVICE_NUMBER* deviceNumber);
bool GetCurDriveInfo();
bool MountESP();
bool GetAllESP();
bool GetVolumeInfo(
	TCHAR* volume,
	TCHAR* fileSystem = nullptr,
	int fileSystemSize = 0,
	TCHAR* volumeName = nullptr,
	int volumeNameSize = 0);
const TCHAR MountPartition(
	DEVNUM devnum,
	TCHAR* dosName = nullptr,
	bool isUnmount = false);

int cmdDrive(LPCTSTR drive)
{
	if (!('a' <= *drive && *drive <= 'z') ||
		*(drive + 1) != '\0')
	{
		tcout << ErrorStrArray[MYERROR_INVALID_PARAMETER];
		return -1;
	}

	//judge whether the drive entered is a hard disk drive
	if (DRIVE_FIXED != GetDriveType(drive))
	{
		tcout << ErrorStrArray[MYERROR_NOT_HARD_DISK];
		return -1;
	}

	TCHAR str[10] = {0};
	_stprintf_s(str, TEXT("\\\\.\\%c:"), *drive);
	HANDLE handle = CreateFile(str, 
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (handle == INVALID_HANDLE_VALUE)
	{
		//input a drive that does not exist
		tcout << ErrorStrArray[MYERROR_NOT_EXIST_DRIVE];
		return -1;
	}

	STORAGE_DEVICE_NUMBER deviceNum;
	if (!GetDeviceNumber(handle, &deviceNum))
	{
		tcout << ErrorStrArray[MYERROR_CANNOT_GET_DISK_NUMBER];
		return -1;
	}

	//mount all ESP partitions
	if (!MountESP())
	{
		tcout << ErrorStrArray[MYERROR_FAIL];
		return -1;
	}

	bool isFind = false;
	map<DEVNUM, PartitionInfo>::iterator espIter;
	for (espIter = allESP.begin(); espIter != allESP.end(); espIter++)
	{
		if (espIter->second.diskNum == deviceNum.DeviceNumber)
		{
			tcout << espIter->second.mountDrive << TEXT("  ");
			isFind = true;
		}
	}

	if (!isFind)
	{
		tcout << ErrorStrArray[MYERROR_CANNOT_FIND_ESP];
		return -1;
	}

	return 0;
}

int cmdDisk(LPCTSTR disk)
{
	LPCTSTR diskNum = disk;
	while (*diskNum != '\0')
	{
		if (!('0' <= *diskNum && *diskNum <= '9'))
		{
			tcout << ErrorStrArray[MYERROR_INVALID_PARAMETER];
			return -1;
		}
		diskNum++;
	}
	
	DWORD num = _ttoi(disk);

	//mount all ESP partitions
	if (!MountESP())
	{
		tcout << ErrorStrArray[MYERROR_FAIL];
		return -1;
	}

	bool isFind = false;
	map<DEVNUM, PartitionInfo>::iterator espIter;
	for (espIter = allESP.begin(); espIter != allESP.end(); espIter++)
	{
		if (espIter->second.diskNum == num)
		{
			tcout << espIter->second.mountDrive << TEXT("  ");
			isFind = true;
		}
	}

	if (!isFind)
	{
		tcout << ErrorStrArray[MYERROR_CANNOT_FIND_ESP];
		return -1;
	}
	return 0;
}

ParameterEntry ParameterTable[] =
{
	{ TEXT("drive"), cmdDrive, TEXT("") },
	{ TEXT("disk"), cmdDisk, TEXT("") }
};


void PreProcessArg(TCHAR* str)
{
	//set all the characters to lower case
	while (*str != '\0')
	{
		if ('A' <= *str && *str <= 'Z')
			*str += 32;
		str++;
	}
}

bool PreProcessMount()
{
	//preprocess for mounting ESP partitions
	//get current all disks
	if (!GetAllDiskNum())
	{
		tcout << ErrorStrArray[MYERROR_CANNOT_GET_DISK_NUMBER];
		return false;
	}
	//get current drives information,no matter success or fail
	GetCurDriveInfo();
	//only get information of all esp partitions, not mounting
	if (!GetAllESP())
	{
		tcout << ErrorStrArray[MYERROR_CANNOT_GET_ESP_INFO];
		return false;
	}
	if (allESP.empty())
	{
		tcout << ErrorStrArray[MYERROR_CANNOT_FIND_ESP];
		return false;
	}
	return true;
}

int _tmain(int argc, TCHAR* arg[])
{
	//make wcout/cout can output chinese
	tcout.imbue(std::locale("chinese"));

	int firstArg = 1;
	int sizeParameterTable = sizeof(ParameterTable) / sizeof(ParameterEntry);
	
	//According parameters to the called function
	while (argc > firstArg)
	{
		PreProcessArg(arg[firstArg]);
		for (int index = 0; index < sizeParameterTable; index++)
		{
			if((_tcsstr(arg[firstArg], ParameterTable[index].cmd) - arg[firstArg] == 1) &&
				(arg[firstArg][0] == '-' || arg[firstArg][0] == '/') && 
				arg[firstArg][_tcslen(ParameterTable[index].cmd) + 1] == ':')
			{
				if (_tcslen(arg[firstArg]) > _tcslen(ParameterTable[index].cmd) + 2)
				{
					int res = ParameterTable[index].fun(&(arg[firstArg][_tcslen(ParameterTable[index].cmd) + 2]));
					return res ? MYERROR_FAIL : MYERROR_SUCCESS;
				}
				else
				{
					tcout << ErrorStrArray[MYERROR_INVALID_PARAMETER];
					return MYERROR_FAIL;
				}
			}
		}

		firstArg++;
	}
	if (argc == 1)
	{
		//tcout << ErrorStrArray[MYERROR_NONE_PARAMETER];
		//return MYERROR_NONE_PARAMETER;

		//call program without parameters will mount all ESP partitions
		if (MountESP())
			return MYERROR_SUCCESS;
		else
			return MYERROR_FAIL;
			
	}
	else
	{
		tcout << ErrorStrArray[MYERROR_INVALID_PARAMETER];
		return MYERROR_FAIL;
	}
	


}



/**************
*	function: DiskInfo* GetAllDiskNum();
*	work: enum the disk to get all disk information
*	return: 
*	if success, return true
*	if fail, return false
**************/
bool GetAllDiskNum()
{
	HDEVINFO diskClassDevices;
	GUID diskClassDeviceInterfaceGuid = GUID_DEVINTERFACE_DISK;
	SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = nullptr;
	DWORD requiredSize;
	DWORD deviceIndex;

	HANDLE disk = INVALID_HANDLE_VALUE;
	STORAGE_DEVICE_NUMBER diskNumber;

	diskClassDevices = SetupDiGetClassDevs(&diskClassDeviceInterfaceGuid,
		NULL,
		NULL,
		DIGCF_PRESENT |
		DIGCF_DEVICEINTERFACE);
	if (INVALID_HANDLE_VALUE == diskClassDevices)
		return NULL;

	ZeroMemory(&deviceInterfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
	deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	deviceIndex = 0;

	while (SetupDiEnumDeviceInterfaces(diskClassDevices,
		NULL,
		&diskClassDeviceInterfaceGuid,
		deviceIndex,
		&deviceInterfaceData)) 
	{
		SetupDiGetDeviceInterfaceDetail(diskClassDevices,
			&deviceInterfaceData,
			NULL,
			0,
			&requiredSize,
			NULL);

		deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
		if (!deviceInterfaceDetailData)
			goto EXIT;

		ZeroMemory(deviceInterfaceDetailData, requiredSize);
		deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if(!SetupDiGetDeviceInterfaceDetail(diskClassDevices,
			&deviceInterfaceData,
			deviceInterfaceDetailData,
			requiredSize,
			NULL,
			NULL))
			goto EXIT;

		disk = CreateFile(deviceInterfaceDetailData->DevicePath,
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		if (INVALID_HANDLE_VALUE == disk)
			goto EXIT;			

		if (!GetDeviceNumber(disk, &diskNumber))
			goto EXIT;

		CloseHandle(disk);
		disk = INVALID_HANDLE_VALUE;

		diskInfo.aryDisk[deviceIndex++] = diskNumber.DeviceNumber;
		//cout << deviceInterfaceDetailData->DevicePath << endl;
		//cout << "\\\\?\\PhysicalDrive" << diskNumber.DeviceNumber << endl;
		//cout << endl;
	}

	diskInfo.diskTotal = deviceIndex;
	return true;

EXIT:
	if (deviceInterfaceDetailData) 
		delete deviceInterfaceDetailData;
	if (disk != INVALID_HANDLE_VALUE)
	{
		CloseHandle(disk);
		disk = INVALID_HANDLE_VALUE;
	}

	return false;
}



/**************
*	function: HANDLE GetDiskHandleByNum(int num);
*	work: get disk handle by disk number
*	return:
*	return disk handle if success, otherwise INVALID_HANDLE_VALUE
**************/
HANDLE GetDiskHandleByNum(int num)
{
	TCHAR diskStr[20];
	_stprintf_s(diskStr, TEXT("\\\\.\\PhysicalDrive%d"), num);
	HANDLE handle;
	return handle = CreateFile(diskStr,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
}



/**************
*	function: bool GetPartitionInfo(..);
*	work: get all partitions information in one disk
*	return:
*	return true if success, otherwise false
**************/
bool GetPartitionInfo(HANDLE diskHandle, DRIVE_LAYOUT_INFORMATION_EX* info, int infoSize)
{
	BOOL res = 0;

	DWORD bytesReturned = 0;
	res = DeviceIoControl(diskHandle,
		IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
		nullptr,
		0,
		info,
		infoSize,
		&bytesReturned,
		nullptr);
	return res ? true : false;
}



/**************
*	function: bool GetCurDriveInfo();
*	work: get current all drives information, 
*		such as: disk and partition number, file system name
*	return:
*	return true if success, otherwise false
**************/
bool GetCurDriveInfo()
{
	TCHAR Drive[MAX_PATH] = { 0 };
	DWORD i = 0;
	PartitionInfo info = {0};
	TCHAR driveName[20];
	STORAGE_DEVICE_NUMBER deviceNumber = { 0 };
	HANDLE partition = INVALID_HANDLE_VALUE;
	
	DWORD len = ::GetLogicalDriveStrings(sizeof(Drive) / sizeof(TCHAR), Drive);
	if (len <= 0)
		goto ERROR_EXIT;
	
	partitionInfo.clear();

	while (Drive[i])
	{
		//cout << Drive[i] << endl;		//optput such as C:\\

		//get the file system type name
		if (!GetVolumeInfo(
			&Drive[i],
			info.fileSystem,
			MAX_FILE_SYSTEM_NAME_SIZE))
			goto ERROR_EXIT;

		_stprintf_s(driveName, TEXT("\\\\.\\%c:"), Drive[i]);
		partition = CreateFile(driveName,
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		
		if (INVALID_HANDLE_VALUE == partition)
			goto ERROR_EXIT;

		//get the partition number and disk number
		if (!GetDeviceNumber(partition, &deviceNumber))
			goto ERROR_EXIT;

		CloseHandle(partition);

		//record partition information
		_stprintf_s(info.mountDrive, TEXT("%c:"), Drive[i]);
		info.diskNum = deviceNumber.DeviceNumber;
		info.partitionNum = deviceNumber.PartitionNumber;
		
		//insert data to map
		partitionInfo.insert(map<DEVNUM, PartitionInfo>::value_type(
			DEVICE_NUMBER(deviceNumber.DeviceNumber, deviceNumber.PartitionNumber),
			info));

		//switch to next partition		
		i += (_tcslen(&Drive[i]) + 1);	
	}

	return true;

ERROR_EXIT:
	partitionInfo.clear();
	return false;
}



/**************
*	function: GetDeviceNumber();
*	work: get disk number or partition number
*	return:
*	return true if success, otherwise false
**************/
bool GetDeviceNumber(HANDLE handle, STORAGE_DEVICE_NUMBER* deviceNumber)
{
	DWORD bytesReturned = 0;
	return DeviceIoControl(handle,
		IOCTL_STORAGE_GET_DEVICE_NUMBER,
		NULL,
		0,
		deviceNumber,
		sizeof(STORAGE_DEVICE_NUMBER),
		&bytesReturned,
		NULL)? true : false;
}



/**************
*	function: GetVolumeInfo();
*	work: get specified volume information, contain file system name, volume name
*	return:
*	return true if success, otherwise false
**************/
bool GetVolumeInfo(
	TCHAR* volume, 
	TCHAR* fileSystem /*= nullptr*/, 
	int fileSystemSize /*= 0*/,
	TCHAR* volumeName /*= nullptr*/, 
	int volumeNameSize /*= 0*/)
{
	DWORD dwSysFlags;
	return GetVolumeInformation(volume,
		volumeName,
		volumeNameSize,
		nullptr,
		nullptr,
		&dwSysFlags,
		fileSystem,
		fileSystemSize
		)? true : false;
}



bool GetAllESP()
{
	//esp partition GUID {C12A7328-F81F-11D2- BA 4B - 00 A0 C9 3E C9 3B}
	//	位	字节	描述		字节序
	//	32	4	数据1	原生
	//	16	2	数据2	原生
	//	16	2	数据3	原生
	//	64	8	数据4	大端序
	GUID GuidESP = { 
		0xC12A7328, 
		0xF81F, 
		0x11D2,
		{ 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } 
	};

	TCHAR tmpTargetName[] = TEXT("1:");
	HANDLE handle = INVALID_HANDLE_VALUE;
	PartitionInfo info;
	DRIVE_LAYOUT_INFORMATION_EX partitionInfo[20] = {0};
	int disktotal = diskInfo.diskTotal;
	allESP.clear();
	while (disktotal--)
	{
		handle = GetDiskHandleByNum(diskInfo.aryDisk[disktotal]);
		if (INVALID_HANDLE_VALUE == handle)
			goto ERROR_EXIT;
		if (!GetPartitionInfo(handle, partitionInfo, sizeof(partitionInfo)))
			goto ERROR_EXIT;
		if (PARTITION_STYLE_GPT == partitionInfo->PartitionStyle)
		{
			for (DWORD i = 0; i < partitionInfo->PartitionCount; i++)
			{
				//judge ESP partition by esp guid
				if (GuidESP == partitionInfo->PartitionEntry[i].Gpt.PartitionType)
				{
					//mount partition for getting file system on it
					if (MountPartition(DEVICE_NUMBER(diskInfo.aryDisk[disktotal], 
							partitionInfo->PartitionEntry[i].PartitionNumber),
						tmpTargetName) == tmpTargetName[0])
					{
						TCHAR fileSystem[10];
						GetVolumeInfo(tmpTargetName, fileSystem, 10);
						//ESP partition filesystem is fat or fat32
						if (_tcsicmp(fileSystem, TEXT("fat")) == 0 ||
							 _tcsicmp(fileSystem, TEXT("fat32")) == 0)
						{
							memset(&info, 0, sizeof(info));

							//set information of ESP partition except for mountDrive
							info.diskNum = diskInfo.aryDisk[disktotal];
							info.partitionNum = partitionInfo->PartitionEntry[i].PartitionNumber;
							_tcscpy_s(info.fileSystem, fileSystem);
							
							//add ESP partition data to allESP
							allESP.insert(map<DEVNUM, PartitionInfo>::value_type(
								DEVICE_NUMBER(info.diskNum, info.partitionNum),
								info));
						}
						//unmount the partition after getting file system 
						MountPartition(DEVICE_NUMBER(diskInfo.aryDisk[disktotal], 
							partitionInfo->PartitionEntry[i].PartitionNumber),
							tmpTargetName, true);
					}
				}		
			}
		}
		CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
	}
	return true;

ERROR_EXIT:
	allESP.clear();
	if (INVALID_HANDLE_VALUE != handle)
		CloseHandle(handle);

	return false;
}




const TCHAR MountPartition(
	DEVNUM devnum, 
	TCHAR* dosName /*= nullptr*/, 
	bool isUnmount /*= false*/)
{
	DWORD driveBitmap = GetLogicalDrives();
	bool isFind = false;
	int index = 1;		//'B'-'A'==1
	TCHAR drive[3] = {0};
	TCHAR devName[50] = {0};
	TCHAR targetName[MAX_PATH + 1];
	DWORD diskNum = DISK_NUMBER(devnum); 
	DWORD partitionNum = PARTITION_NUMBER(devnum);
	BOOL res = false;
	//for unmount partition operation
	if (dosName && isUnmount)
	{
		_stprintf_s(
			drive,
			TEXT("%c:"),
			dosName[0]);
		res = DefineDosDevice(
			DDD_REMOVE_DEFINITION ,
			drive,
			nullptr);
		if (res)
			return drive[0];
		else
			return TEXT('\0');
	}

	//for mount partition operation
	if (!dosName)
	{
		while (!isFind && index < 25)
		{
			if (!((driveBitmap >> (++index)) & 0x1))	//FOR drive letter between C: to Z:
			{
				_stprintf_s(
					drive,
					TEXT("%c:"),
					index + TEXT('A'));
				//QueryDosDevice() == 0 indicates the dos device name does not define
				if (QueryDosDevice(drive, targetName, MAX_PATH + 1) == 0)
					isFind = true;
				else
					continue;
			}
		}
		if (!isFind && !(driveBitmap & 0x1))	//for the drive letter A:
		{
			_stprintf_s(drive, TEXT("A:"));
			if (QueryDosDevice(drive, targetName, MAX_PATH + 1) == 0)
				isFind = true;
		}
		else if (!isFind && !((driveBitmap >> 1) & 0x1))		//for the drive letter B:
		{
			_stprintf_s(drive, TEXT("B:"));
			if (QueryDosDevice(drive, targetName, MAX_PATH + 1) == 0)
				isFind = true;
		}
	}
	else	//user custom dos name
	{
		_stprintf_s(
			drive,
			TEXT("%c:"),
			dosName[0]);
		if (QueryDosDevice(drive, targetName, MAX_PATH + 1) == 0)
			isFind = true;
	}

	if (isFind)
	{
		_stprintf_s(
			devName,
			TEXT("\\Device\\Harddisk%d\\Partition%d"),
			diskNum, partitionNum);
		res = DefineDosDevice(DDD_RAW_TARGET_PATH, drive, devName);
	}
	if (res)
		return drive[0];
	else
		return TEXT('\0');
}



bool MountESP()
{
	if (!PreProcessMount())
		return false;

	bool res = false;
	TCHAR mountDrive = '\0';
	map<DEVNUM, PartitionInfo>::iterator espIter, partitionIter;
	
	for (espIter = allESP.begin(); espIter != allESP.end(); espIter++)
	{
		if ((partitionIter = partitionInfo.find(espIter->first))
			!= partitionInfo.end())
		{
			//indicates this ESP partition has been mounted
			//set mountDrive item of ESP partition
			_tcscpy_s(espIter->second.mountDrive,
				partitionIter->second.mountDrive);
			//do not need to mount this esp partition
			//output drive letter direct, such as D:disk0  
			tcout << espIter->second.mountDrive[0] << TEXT(":disk")
				<< espIter->second.diskNum << TEXT("  ");
			res = true;
		}
		else
		{
			//indicates this ESP partition hasn't been mounted
			//To mount the esp partition
			if (mountDrive = MountPartition(espIter->first))
			{
				//set mountDrive item of ESP partition
				_stprintf_s(espIter->second.mountDrive, TEXT("%c:"), mountDrive);
				//output format, such as E:disk2
				tcout << mountDrive << TEXT(":disk")
					<< espIter->second.diskNum << TEXT("  ");
				res = true;
			}
		}
	}

	return res;
}

