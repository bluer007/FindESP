
#include <Windows.h>  
#include <Winbase.h>  //the head file of GetFirmwareEnvironmentVariableA and GetFirmwareType 
#include <iostream>  
#include <TCHAR.H>
using namespace std;

#ifdef UNICODE
#define tcout std::wcout
#else
#define tcout std::cout
#endif


typedef enum
{
	MYERROR_SUCCESS = 0,
	MYERROR_INVALID_PARAMETER = 1,
	MYERROR_NONE_PARAMETER = 2
}ERROR_CODE;

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