
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
	MYERROR_INVALID_PARAMETER = 1,
	MYERROR_NONE_PARAMETER = 2,
	MYERROR_NOT_HARD_DISK =3,
	MYERROR_CANNOT_GET_DISK_NUMBER = 4
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
typedef DWORD DEVNUM;	//new type. use to record disk number and partition number

//record partition information, such as file system name, disk and partition number
map<DEVNUM, TCHAR[MAX_FILE_SYSTEM_NAME_SIZE]> partitionInfo;

vector<DEVNUM> allESP;

LPCTSTR ErrorStrArray[] =
{
	//MYERROR_SUCCESS = 0,
	TEXT(""),
	//MYERROR_INVALID_PARAMETER = 1,
	TEXT("error: invalid parameters.\n"),
	//MYERROR_NONE_PARAMETER = 2
	TEXT("error: none parameters.\n"),
	//MYERROR_NOT_HARD_DISK =3
	TEXT("error: drive entered is not a hard disk drive.\n"),
	//MYERROR_CANNOT_GET_DISK_NUMBER = 4
	TEXT("error: can't get disk number, maybe you can run as admin.\n")
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
bool GetVolumeInfo(
	TCHAR* volume,
	TCHAR* fileSystem = nullptr,
	int fileSystemSize = 0,
	TCHAR* volumeName = nullptr,
	int volumeNameSize = 0);

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

	if (!GetAllDiskNum())
	{
		tcout << ErrorStrArray[MYERROR_CANNOT_GET_DISK_NUMBER];
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
	//GetDiskHandleByNum();
	
	tcout << drive << endl;
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
	
	tcout << disk << endl;
	return 0;
}

ParameterEntry ParameterTable[] =
{
	{ TEXT("drive"), cmdDrive, TEXT("") },
	{ TEXT("disk"), cmdDisk, TEXT("") }
};


void preProcess(TCHAR* str)
{
	//set all the characters to lower case
	while (*str != '\0')
	{
		if ('A' <= *str && *str <= 'Z')
			*str += 32;
		str++;
	}
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
		preProcess(arg[firstArg]);
		for (int index = 0; index < sizeParameterTable; index++)
		{
			if((_tcsstr(arg[firstArg], ParameterTable[index].cmd) - arg[firstArg] == 1) &&
				(arg[firstArg][0] == '-' || arg[firstArg][0] == '/') && 
				arg[firstArg][_tcslen(ParameterTable[index].cmd) + 1] == ':')
			{
				if (_tcslen(arg[firstArg]) > _tcslen(ParameterTable[index].cmd) + 2)
				{
					int res = ParameterTable[index].fun(&(arg[firstArg][_tcslen(ParameterTable[index].cmd) + 2]));
					return res ? MYERROR_INVALID_PARAMETER : MYERROR_SUCCESS;
				}
				else
				{
					tcout << ErrorStrArray[MYERROR_INVALID_PARAMETER];
					return MYERROR_INVALID_PARAMETER;
				}
			}
		}

		firstArg++;
	}
	if (argc == 1)
	{
		tcout << ErrorStrArray[MYERROR_NONE_PARAMETER];
		return MYERROR_NONE_PARAMETER;
	}
	else
	{
		tcout << ErrorStrArray[MYERROR_INVALID_PARAMETER];
		return MYERROR_INVALID_PARAMETER;
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
		if (!SetupDiGetDeviceInterfaceDetail(diskClassDevices,
			&deviceInterfaceData,
			NULL,
			0,
			&requiredSize,
			NULL))
			goto EXIT;

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
	bool res = 0;

	DWORD bytesReturned = 0;
	res = (bool)DeviceIoControl(diskHandle,
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
	TCHAR FileSysNameBuf[MAX_FILE_SYSTEM_NAME_SIZE] = { 0 };
	TCHAR driveName[20];
	STORAGE_DEVICE_NUMBER deviceNumber = { 0 };
	HANDLE partition = INVALID_HANDLE_VALUE;

	DWORD len = ::GetLogicalDriveStrings(sizeof(Drive) / sizeof(TCHAR), Drive);
	if (len <= 0)
		goto ERROR_EXIT;

	partitionInfo.erase(partitionInfo.begin(), partitionInfo.end());
	while (Drive[i])
	{
		//cout << Drive[i] << endl;

		//get the file system type name
		if (!GetVolumeInfo(
			&Drive[i],
			FileSysNameBuf,
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
		if (GetDeviceNumber(partition, &deviceNumber))
			goto ERROR_EXIT;

		CloseHandle(partition);

		//insert data to map, 
		//key indicates disk and partition number, 
		//second value indicates file system name
		partitionInfo.insert(map<DWORD, TCHAR[MAX_FILE_SYSTEM_NAME_SIZE]>::value_type((deviceNumber.DeviceNumber << 16) | (deviceNumber.PartitionNumber), 
			FileSysNameBuf));

		//switch to next partition		
		i += (_tcslen(&Drive[i]) + 1);	
	}

	return true;

ERROR_EXIT:
	partitionInfo.erase(partitionInfo.begin(), partitionInfo.end());
	return false;
}



/**************
*	function: GetDeviceNumber();
*	work: get disk number or disk number
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
		&deviceNumber,
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
		fileSystem,
		fileSystemSize,
		nullptr,
		nullptr,
		&dwSysFlags,
		volumeName,
		volumeNameSize
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

	HANDLE handle = INVALID_HANDLE_VALUE;
	DEVNUM diskNum, partitionNum;
	DRIVE_LAYOUT_INFORMATION_EX partitionInfo[20] = {0};
	int disktotal = diskInfo.diskTotal;
	allESP.erase(allESP.begin(), allESP.end());
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
				if (GuidESP == partitionInfo->PartitionEntry[i].Gpt.PartitionType)
				{
					diskNum = diskInfo.aryDisk[disktotal];
					partitionNum = partitionInfo->PartitionEntry[i].PartitionNumber;
					//add ESP partition data to allESP
					allESP.push_back(DEVICE_NUMBER(diskNum, partitionNum));
				}		
			}
		}
		CloseHandle(handle);
		handle = INVALID_HANDLE_VALUE;
	}
	return true;

ERROR_EXIT:
	allESP.erase(allESP.begin(), allESP.end());
	if (INVALID_HANDLE_VALUE != handle)
		CloseHandle(handle);

	return false;
}




bool MountESP(DEVNUM devnum)
{
	DWORD driveBitmap = GetLogicalDrives();
	bool isFind = false;
	int index = 26;
	TCHAR drive[3] = {0};
	TCHAR devName[20] = {0};
	DWORD diskNum, partitionNum;
	bool res = false;
	while (!isFind && index > 0)
	{
		if ((driveBitmap >> (--index)) & 0x1)
		{
			isFind = true;
			diskNum = DISK_NUMBER(devnum);
			partitionNum = PARTITION_NUMBER(devnum);
			_stprintf_s(
				drive, 
				TEXT("%c:"), 
				index + TEXT('A'));
			_stprintf_s(
				devName, 
				TEXT("\\Device\\Harddisk%d\\Partition%d"), 
				diskNum, partitionNum);
			res = (bool)DefineDosDevice(DDD_RAW_TARGET_PATH, drive, devName);
		}
	}
	
	return res;
}


