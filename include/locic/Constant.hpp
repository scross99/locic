#ifndef LOCIC_CONSTANT_HPP
#define LOCIC_CONSTANT_HPP

#include <stdint.h>
#include <cassert>

#include <locic/MakeString.hpp>
#include <locic/Support/String.hpp>

namespace locic{
	
	class Constant {
		public:
			enum Kind {
				NULLVAL,
				BOOLEAN,
				INTEGER,
				FLOATINGPOINT,
				CHARACTER,
				STRING
			};
			
			typedef unsigned long long IntegerVal;
			typedef long double FloatVal;
			typedef uint32_t CharVal;
			
			static Constant Null(){
				return Constant(NULLVAL);
			}
			
			static Constant True(){
				Constant constant(BOOLEAN);
				constant.bool_ = true;
				return constant;
			}
			
			static Constant False(){
				Constant constant(BOOLEAN);
				constant.bool_ = false;
				return constant;
			}
			
			static Constant Integer(IntegerVal value){
				Constant constant(INTEGER);
				constant.integer_ = value;
				return constant;
			}
			
			static Constant Float(FloatVal value){
				Constant constant(FLOATINGPOINT);
				constant.float_ = value;
				return constant;
			}
			
			static Constant Character(CharVal value){
				Constant constant(CHARACTER);
				constant.character_ = value;
				return constant;
			}
			
			static Constant StringVal(const String value){
				Constant constant(STRING);
				constant.string_ = value;
				return constant;
			}
			
			// Default (non-initialising!) constructor allows
			// placing this type in unions.
			Constant() = default;
			
			Kind kind() const{
				return kind_;
			}
			
			bool boolValue() const{
				assert(kind_ == BOOLEAN);
				return bool_;
			}
			
			IntegerVal integerValue() const{
				assert(kind_ == INTEGER);
				return integer_;
			}
			
			FloatVal floatValue() const{
				assert(kind_ == FLOATINGPOINT);
				return float_;
			}
			
			CharVal characterValue() const{
				assert(kind_ == CHARACTER);
				return character_;
			}
			
			const String& stringValue() const{
				assert(kind_ == STRING);
				return string_;
			}
			
			std::string toString() const {
				switch (kind_) {
					case NULLVAL:
						return "NullConstant";
					case BOOLEAN:
						return makeString("BoolConstant(%s)", bool_ ? "true" : "false");
					case INTEGER:
						return makeString("IntegerConstant(%llu)", integerValue());
					case FLOATINGPOINT:
						return makeString("FloatConstant(%Lf)", floatValue());
					case CHARACTER:
						return makeString("CharacterConstant(%llu)", (unsigned long long) characterValue());
					case STRING:
						return makeString("StringConstant(\"%s\")", escapeString(stringValue().asStdString()).c_str());
					default:
						return "[UNKNOWN CONSTANT]";
				}
			}
			
		private:
			Constant(Kind pKind)
			: kind_(pKind) { }
			
			Kind kind_;
			
			union{
				bool bool_;
				IntegerVal integer_;
				FloatVal float_;
				CharVal character_;
				String string_;
			};
		
	};

}

#endif
