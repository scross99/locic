#include <assert.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include <locic/Support/MakeString.hpp>
#include <locic/Support/String.hpp>
#include <locic/Support/Version.hpp>

namespace locic {

	Version Version::FromString(const std::string& versionString) {
		const auto components = splitString(versionString, ".");
		assert(components.size() == 3);
		return Version(atol(components.at(0).c_str()), atol(components.at(1).c_str()), atol(components.at(2).c_str()));
	}
	
	Version::Version(size_t pMajor, size_t pMinor, size_t pBuild)
		: major_(pMajor), minor_(pMinor), build_(pBuild) { }
	
	size_t Version::majorVersion() const {
		return major_;
	}
	
	size_t Version::minorVersion() const {
		return minor_;
	}
	
	size_t Version::buildVersion() const {
		return build_;
	}
	
	bool Version::operator<(const Version& other) const {
		if (majorVersion() != other.majorVersion()) {
			return majorVersion() < other.majorVersion();
		}
		
		if (minorVersion() != other.minorVersion()) {
			return minorVersion() < other.minorVersion();
		}
		
		return buildVersion() < other.buildVersion();
	}
	
	bool Version::operator==(const Version& other) const {
		return !(*this < other) && !(other < *this);
	}
	
	bool Version::operator!=(const Version& other) const {
		return !(*this == other);
	}
	
	std::string Version::toString() const {
		return makeString("%llu.%llu.%llu",
			(unsigned long long) majorVersion(),
			(unsigned long long) minorVersion(),
			(unsigned long long) buildVersion());
	}
	
}
	
