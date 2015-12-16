
#include <Windows.h>  
#include <Winbase.h>  //the head file of GetFirmwareEnvironmentVariableA and GetFirmwareType 
#include <iostream>  
#include <TCHAR.H>
#include <setupapi.h>	//the head file of Setup API 
#include <vector>
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
	MYERROR_NONE_PARAMETER = 2
}ERROR_CODE;

enum 
{
	MAX_DISK_NUM = 32
};

//the struct of a disk information
typedef struct
{
	int diskNum;			//The disk of the subscript, such as 0, 1..
	int partitionOfDisk;	//total number of disk partitions, such as 5, 6, 7..
}OneDiskInfo;

//the struct of all disks information
typedef struct
{
	//total number of disk, such as 2, 3..
	int diskTotal;
	/*
	a array indicates each disk number, 
	for example if aryDisk[0] == 1 indicates disk0 is valid;
	if aryDisk[1] == 0 indicates disk1 is invalid
	*/
	byte aryDisk[MAX_DISK_NUM];
}DiskInfo;

DiskInfo* GetAllDiskNum();		//enum the disk to get all disk information
HANDLE GetDiskHandleByNum(int num);	//get disk handle by disk number

LPCTSTR ErrorStrArray[] =
{
	//MYERROR_SUCCESS = 0,
	TEXT(""),
	//MYERROR_INVALID_PARAMETER = 1,
	TEXT("error: invalid parameters.\n"),
	//MYERROR_NONE_PARAMETER = 2
	TEXT("error: none parameters.\n")
};

typedef int(*ParameterFun)(LPCTSTR arg1);

typedef struct 
{
	LPCTSTR cmd;
	ParameterFun fun;
	LPCTSTR arg1;
}ParameterEntry;


int cmdDrive(LPCTSTR drive)
{
	if (!('a' <= *drive && *drive <= 'z') ||
		*(drive + 1) != '\0')
	{
		tcout << ErrorStrArray[MYERROR_INVALID_PARAMETER];
		return -1;
	}

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
*	if success, return the address array of DiskInfo struct
*	if fail, return null
**************/
DiskInfo* GetAllDiskNum()
{
	DiskInfo* m_diskInfo = new DiskInfo();

	HDEVINFO diskClassDevices;
	GUID diskClassDeviceInterfaceGuid = GUID_DEVINTERFACE_DISK;
	SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = nullptr;
	DWORD requiredSize;
	DWORD deviceIndex;

	HANDLE disk = INVALID_HANDLE_VALUE;
	STORAGE_DEVICE_NUMBER diskNumber;
	DWORD bytesReturned;

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
		++deviceIndex;

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

		if(!DeviceIoControl(disk,
			IOCTL_STORAGE_GET_DEVICE_NUMBER,
			NULL,
			0,
			&diskNumber,
			sizeof(STORAGE_DEVICE_NUMBER),
			&bytesReturned,
			NULL))
			goto EXIT;

		CloseHandle(disk);
		disk = INVALID_HANDLE_VALUE;

		m_diskInfo->aryDisk[diskNumber.DeviceNumber] = 1;
		//cout << deviceInterfaceDetailData->DevicePath << endl;
		//cout << "\\\\?\\PhysicalDrive" << diskNumber.DeviceNumber << endl;
		//cout << endl;
	}

	m_diskInfo->diskTotal = deviceIndex;
	return m_diskInfo;

EXIT:
	if (deviceInterfaceDetailData) 
		delete deviceInterfaceDetailData;
	if (disk != INVALID_HANDLE_VALUE)
	{
		CloseHandle(disk);
		disk = INVALID_HANDLE_VALUE;
	}
	if (m_diskInfo)
		delete m_diskInfo;

	return nullptr;
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
	res = DeviceIoControl(diskHandle,
		IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
		NULL,
		0,
		info,
		infoSize,
		&bytesReturned,
		NULL);
	return res ? true : false;
}



/**************
*	function: TCHAR* GetFileSystemByDrive(TCHAR* drive);
*	work: get file system by drive letter, such as: e:\
*	return:
*	return string indicates file system if success, otherwise null
**************/
TCHAR* GetFileSystemByDrive(TCHAR* drive)
{
	static TCHAR fileSystem[MAX_PATH + 1] = {0};
	DWORD dwSysFlags;
	if (GetVolumeInformation(drive, 
		NULL, 0, 
		NULL, NULL, 
		&dwSysFlags, 
		fileSystem, 
		MAX_PATH + 1))
		return fileSystem;
	else
		return nullptr;
}










