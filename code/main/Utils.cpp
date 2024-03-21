#include "Utils.h"

#include <Windows.h>		// windows API
#include <shlobj.h>			// folder dialog
#include <iostream>
#include <bitset>

namespace Utils
{
	/*!***********************************************************************
	\brief
	Changes the message length in network order to its string representation in hex form through binary manipulation.
	\param[in, out] long
	the long that is the message length to be converted to string
	\return
	The string representing the message length in network order
	*************************************************************************/
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

	u_long StringTo_htonl(std::string const& input)
	{
		u_long ret = 0;
		std::memcpy(&ret, input.data(), sizeof(u_long)); // copy binary data into the string
		return htonl(ret);
	}

	u_short StringTo_htons(std::string const& input)
	{
		u_short ret = 0;
		std::memcpy(&ret, input.data(), sizeof(u_short)); // copy binary data into the string
		return htons(ret);
	}

	/*!***********************************************************************
	\brief
	Converts a hex string into human readable string
	\param[in, out] inputstring
	the hex string to be converted
	\return
	the human readable string
	*************************************************************************/
	std::string HexToString(const std::string& inputstring) {
		std::string output{};
		for (size_t i = 0; i < inputstring.length(); i += 2) {
			std::string byteString = inputstring.substr(i, 2);
			//convert to unsigned long in hex format then cast to char
			char byte = static_cast<char>(std::stoul(byteString, nullptr, 16));
			output.push_back(byte); //append to message
		}
		return output;
	}

	USHORT ToChecksum(const std::string segment)
	{
		// divide each chunk to 16-bits
		std::vector<std::string> data_segments;
		for (int i = 0; i < segment.size(); i += 2) {
			std::string chunk = segment.substr(i, 2);
			data_segments.push_back(chunk);
		}
		// sum all 16-bits
		std::bitset<16> checksum(0);
		for (std::string s : data_segments) {
			std::bitset<8> first(s[0]);
			std::bitset<8> second(s[1]);
			std::bitset<16> str_bits(first.to_ulong() * 0x100 + second.to_ulong());
			checksum = std::bitset<16>(checksum.to_ulong() + str_bits.to_ulong());
		}

		// obtain 1's compliment of each sum
		for (int i = 0; i < checksum.size(); i++) {
			checksum[i] = checksum[i] ^ 1;
		}

		return static_cast<USHORT>(checksum.to_ulong());
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

