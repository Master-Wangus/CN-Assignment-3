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

    template<typename V>
    ULONG GenerateUniqueULongKey(const std::unordered_map<ULONG, V>& map) 
    {
        std::random_device rd;
        std::mt19937_64 engine(rd()); // Use a Mersenne Twister engine for 64-bit values
        std::uniform_int_distribution<ULONG> dist(
            std::numeric_limits<ULONG>::min(),
            std::numeric_limits<ULONG>::max());

        K newKey;
        do {
            newKey = dist(engine);
        } while (map.find(newKey) != map.end()); / Continue until a unique key is found

        return newKey;
    }
}