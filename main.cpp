


#include <Windows.h>  
#include <Winbase.h>  //the head file of GetFirmwareEnvironmentVariableA and GetFirmwareType 
#include <iostream>  
#include <TCHAR.H>
using namespace std;

typedef int(*ParameterFun)(LPCTSTR arg1);

typedef struct 
{
	LPCTSTR cmd;
	ParameterFun fun;
	LPCTSTR arg1;
}ParameterEntry;


int cmdDrive(LPCTSTR drive)
{
	wcout << drive << endl;
	return 0;
}

int cmdDisk(LPCTSTR disk)
{
	wcout << disk << endl;
	return 0;
}

ParameterEntry ParameterTable[] =
{
	{ TEXT("drive"), cmdDrive, TEXT("") },
	{ TEXT("disk"), cmdDisk, TEXT("") }
};


typedef struct
{
	int error_code;
	LPCTSTR error_str;
}ERROR_STR;

typedef enum 
{
	MYERROR_SUCCESS = 0,
	MYERROR_INVALID_PARAMETER = 1,
	MYERROR_NONE_PARAMETER = 2

}ERROR_CODE;

ERROR_STR ErrorArray[] =
{
	{MYERROR_SUCCESS, TEXT("")},
	{MYERROR_INVALID_PARAMETER, TEXT("error: invalid parameters.\n")},
	{MYERROR_NONE_PARAMETER, TEXT("error: none parameters.\n")}
};

void preProcess(TCHAR* str)
{
	//set all the characters to lower case
	while (*str != '\0')
	{
		if ('A' < *str && *str < 'Z')
			*str += 32;
		str++;
	}
}


int wmain(int argc, wchar_t** arg)
{
	//make wcout() can output chinese
	std::wcout.imbue(std::locale("chinese"));

	int firstArg = 1;
	int sizeParameterTable = sizeof(ParameterTable) / sizeof(ParameterEntry);
	
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
					wcout << ErrorArray[MYERROR_INVALID_PARAMETER].error_str;
					return MYERROR_INVALID_PARAMETER;
				}
			}
		}

		firstArg++;
	}
	if (argc == 1)
	{
		wcout << ErrorArray[MYERROR_NONE_PARAMETER].error_str;
		return MYERROR_NONE_PARAMETER;
	}
	else
	{
		wcout << ErrorArray[MYERROR_INVALID_PARAMETER].error_str;
		return MYERROR_INVALID_PARAMETER;
	}
	

	/*
	//Windows 7/Server 2008R2 and above valid
	GetFirmwareEnvironmentVariableA((""), "{00000000-0000-0000-0000-000000000000}", NULL, 0);
	if (GetLastError() == ERROR_INVALID_FUNCTION)
		//API not supported; this is a legacy BIOS  
		cout << "Bios引导" << endl;
	else
		//API error (expected) but call is supported.This is UEFI.  
		cout << "Uefi引导" << endl;

*/

}