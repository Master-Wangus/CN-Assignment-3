#include "Utils.h"

#include <Windows.h>		// windows API
#include <shlobj.h>			// folder dialog
#include <iostream>

namespace Utils
{
	std::string htonlToString(u_long input)
	{
		std::string output(sizeof(unsigned long), '\0'); // Initialize string with the size of u_long, filled with '\0'
		std::memcpy(&output[0], &input, sizeof(unsigned long)); // Copy the binary data of val into the string
		return output;
	}
	/*!***********************************************************************
	\brief
	To convert the string taken as input which maybe network order and convert it to a unsigned long
	for ntohl to process.
	\param[in, out] string
	the input string representing the length of the message in network order
	\return
	the long representing the system order
	*************************************************************************/
	u_long StringTo_ntohl(std::string const& input) {
		u_long ret = 0;
		std::memcpy(&ret, input.data(), sizeof(u_long)); // copy binary data into the string
		return ntohl(ret);
	}

	u_short StringTo_ntohs(std::string const& input) {
		uint16_t ret = 0;
		std::memcpy(&ret, input.data(), sizeof(u_short)); // copy binary data into the string
		return ntohs(ret);
	}
	std::filesystem::path OpenFolder()
	{
		std::filesystem::path value;
		IFileDialog *dialog = NULL;
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
		{
			DWORD dwOptions;
			if (SUCCEEDED(dialog->GetOptions(&dwOptions)))
			{
				dialog->SetOptions(dwOptions | FOS_PICKFOLDERS);
			}
			if (SUCCEEDED(dialog->Show(NULL)))
			{
				IShellItem* psi = NULL;
				LPWSTR g_path;
				if (SUCCEEDED(dialog->GetResult(&psi)))
				{
					if (SUCCEEDED(psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &g_path)))
					{
						
						int length = WideCharToMultiByte(CP_UTF8, 0, g_path, -1, nullptr, 0, nullptr, nullptr);
						std::string temp(length, 0);
						WideCharToMultiByte(CP_UTF8, 0, g_path, -1, &temp[0], length, nullptr, nullptr);
						value = std::filesystem::path(temp);

						CoTaskMemFree(g_path);
					}
				}
				psi->Release();
			}
		}
		dialog->Release();
		return value;
	}
}

