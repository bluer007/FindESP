


#include <Windows.h>  
#include <Winbase.h>  //the head file of GetFirmwareEnvironmentVariableA and GetFirmwareType 
#include <iostream>  
using namespace std;
int main(int argc, char **arg)
{
	//Windows 7/Server 2008R2 and above valid
	GetFirmwareEnvironmentVariableA((""), "{00000000-0000-0000-0000-000000000000}", NULL, 0);
	if (GetLastError() == ERROR_INVALID_FUNCTION)
		//API not supported; this is a legacy BIOS  
		cout << "Bios引导" << endl;
	else
		//API error (expected) but call is supported.This is UEFI.  
		cout << "Uefi引导" << endl;

	system("pause");

	return 0;
}