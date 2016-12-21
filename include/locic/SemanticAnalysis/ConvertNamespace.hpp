#ifndef LOCIC_SEMANTICANALYSIS_CONVERTNAMESPACE_HPP
#define LOCIC_SEMANTICANALYSIS_CONVERTNAMESPACE_HPP

#include <locic/AST.hpp>

#include <locic/SemanticAnalysis/Context.hpp>

namespace locic {
	
	namespace SemanticAnalysis {
		
		void ConvertNamespace(Context& context, const AST::NamespaceList& rootASTNamespaces);
		
	}
	
}

#endif
