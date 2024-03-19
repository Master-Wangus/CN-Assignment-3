#include "Utils.h"

#include <Windows.h>		// windows API

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
		OPENFILENAMEA folderDialog;
		CHAR szfile[260] = { 0 };
		ZeroMemory(&folderDialog, sizeof(OPENFILENAME));
		folderDialog.lStructSize = sizeof(OPENFILENAME);

		return std::filesystem::path();
	}
}

