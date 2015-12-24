#ifndef LOCIC_LEX_CHARACTER_HPP
#define LOCIC_LEX_CHARACTER_HPP

#include <cassert>
#include <cstdint>

namespace locic {
	
	namespace Lex {
		
		class Character {
		public:
			Character(uint32_t value)
			: value_(value) { }
			
			bool isNull() const {
				return value_ == 0;
			}
			
			bool isASCII() const {
				return value_ < 128;
			}
			
			bool isDigit() const {
				if (!isASCII()) {
					return false;
				}
				
				return '0' <= value_ && value_ <= '9';
			}
			
			bool isHexLowerCaseCharacter() const {
				if (!isASCII()) {
					return false;
				}
				
				return 'a' <= value_ && value_ <= 'f';
			}
			
			bool isHexUpperCaseCharacter() const {
				if (!isASCII()) {
					return false;
				}
				
				return 'A' <= value_ && value_ <= 'F';
			}
			
			bool isHexCharacter() const {
				return isHexLowerCaseCharacter() ||
				       isHexUpperCaseCharacter();
			}
			
			bool isHexDigit() const {
				return isDigit() || isHexCharacter();
			}
			
			char asciiValue() const {
				assert(isASCII());
				return (char) value_;
			}
			
			uint32_t value() const {
				return value_;
			}
			
			bool operator==(const Character& other) const {
				return value_ == other.value_;
			}
			
			bool operator!=(const Character& other) const {
				return value_ != other.value_;
			}
			
			bool operator==(const uint32_t other) const {
				return value_ == other;
			}
			
			bool operator!=(const uint32_t other) const {
				return value_ != other;
			}
			
		private:
			uint32_t value_;
			
		};
		
	}
	
}

#endif