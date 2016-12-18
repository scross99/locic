#ifndef LOCIC_SEMANTICANALYSIS_SCOPEELEMENT_HPP
#define LOCIC_SEMANTICANALYSIS_SCOPEELEMENT_HPP

#include <locic/Support/String.hpp>

namespace locic {
	
	namespace AST {
		
		class Alias;
		class Function;
		
	}
	
	namespace SEM {
		
		class CatchClause;
		class Namespace;
		class Scope;
		class SwitchCase;
		class TypeInstance;
		
	}
	
	namespace SemanticAnalysis {
		
		class ScopeElement {
			public:
				enum Kind {
					NAMESPACE,
					ALIAS,
					TYPEINSTANCE,
					FUNCTION,
					SCOPE,
					SWITCHCASE,
					CATCHCLAUSE,
					LOOP,
					SCOPEACTION,
					TRYSCOPE,
					ASSERTNOEXCEPT
				};
				
				static ScopeElement Namespace(SEM::Namespace& nameSpace);
				
				static ScopeElement Alias(AST::Alias& alias);
				
				static ScopeElement TypeInstance(SEM::TypeInstance& typeInstance);
				
				static ScopeElement Function(AST::Function& function);
				
				static ScopeElement Scope(SEM::Scope& scope);
				
				static ScopeElement SwitchCase(SEM::SwitchCase& switchCase);
				
				static ScopeElement CatchClause(SEM::CatchClause& catchClause);
				
				static ScopeElement Loop();
				
				static ScopeElement ScopeAction(const String& state);
				
				static ScopeElement TryScope();
				
				static ScopeElement AssertNoExcept();
				
				Kind kind() const;
				
				bool isNamespace() const;
				bool isAlias() const;
				bool isTypeInstance() const;
				bool isFunction() const;
				bool isScope() const;
				bool isSwitchCase() const;
				bool isCatchClause() const;
				bool isLoop() const;
				bool isScopeAction() const;
				bool isTryScope() const;
				bool isAssertNoExcept() const;
				
				SEM::Namespace& nameSpace() const;
				AST::Alias& alias() const;
				SEM::TypeInstance& typeInstance() const;
				AST::Function& function() const;
				SEM::Scope& scope() const;
				SEM::SwitchCase& switchCase() const;
				SEM::CatchClause& catchClause() const;
				const String& scopeActionState() const;
				
				bool hasName() const;
				const String& name() const;
				
			private:
				ScopeElement(Kind pKind);
				
				Kind kind_;
				
				union {
					void* ptr;
					SEM::Namespace* nameSpace;
					AST::Alias* alias;
					SEM::TypeInstance* typeInstance;
					AST::Function* function;
					SEM::Scope* scope;
					SEM::SwitchCase* switchCase;
					SEM::CatchClause* catchClause;
					String scopeActionState;
				} data_;
				
		};
		
	}
	
}

#endif
