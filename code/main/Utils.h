/* Start Header
*****************************************************************/
/*!
\file Utils.h
\authors Koh Wei Ren, weiren.koh, 2202110,
		 Pang Zhi Kai, p.zhikai, 2201573
\par weiren.koh@digipen.edu
	 p.zhikai@digipen.edu
\date 18/03/2024
\brief A file containing utlity functions
*/
/* End Header
*******************************************************************/
#pragma once

#include <string>
#include <filesystem>
#include <Bits.h>
#include <unordered_map>
#include <random>
#include <limits>

#undef max
#undef min

namespace Utils
{
	std::string htonlToString(u_long input);
	u_long StringTo_ntohl(std::string const& input);
	u_short StringTo_ntohs(std::string const& input);
	u_long StringTo_htonl(std::string const& input);
	u_short StringTo_htons(std::string const& input);
	std::string HexToString(const std::string& inputstring);

	USHORT ToChecksum(const std::string segment);
	std::filesystem::path OpenFolder();

	ULONG GenerateUniqueULongKey(const std::vector<ULONG> keyvec);
}