#ifndef LOCIC_FRONTEND_DIAGNOSTICS_HPP
#define LOCIC_FRONTEND_DIAGNOSTICS_HPP

#include <string>

#include <locic/Support/ErrorHandling.hpp>

namespace locic {
	
	namespace Lex {
		enum class DiagID;
	}
	
	enum class DiagLevel {
		Error,
		Warning,
		Notice
	};
	
	class Diag {
	public:
		virtual ~Diag() { }
		
		virtual DiagLevel level() const = 0;
		
		virtual Lex::DiagID lexId() const {
			locic_unreachable("Not lex diagnostic.");
		}
		
		virtual std::string toString() const = 0;
		
	};
	
	class Error: public Diag {
		DiagLevel level() const {
			return DiagLevel::Error;
		}
	};
	
	class Warning: public Diag {
		DiagLevel level() const {
			return DiagLevel::Warning;
		}
	};
	
	class Notice: public Diag {
		DiagLevel level() const {
			return DiagLevel::Notice;
		}
	};
	
}

#endif