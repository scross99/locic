#ifndef LOCIC_AST_FUNCTION_HPP
#define LOCIC_AST_FUNCTION_HPP

#include <string>
#include <vector>

#include <locic/Support/Name.hpp>

#include <locic/AST/ConstSpecifier.hpp>
#include <locic/AST/ModuleScope.hpp>
#include <locic/AST/Node.hpp>
#include <locic/AST/RequireSpecifier.hpp>
#include <locic/AST/Scope.hpp>
#include <locic/AST/Symbol.hpp>
#include <locic/AST/TemplateVar.hpp>
#include <locic/AST/TypeDecl.hpp>
#include <locic/AST/Var.hpp>

#include <locic/Debug/FunctionInfo.hpp>

#include <locic/SEM/FunctionType.hpp>
#include <locic/SEM/GlobalStructure.hpp>
#include <locic/SEM/TemplatedObject.hpp>

#include <locic/Support/FastMap.hpp>

namespace locic {
	
	namespace AST {
		
		class TemplateVar;
		
	}
	
	namespace SEM {
		
		class Namespace;
		class Scope;
		
	}
	
	namespace AST {
		
		/**
		* \brief Function
		* 
		* This class encapsulates all of the properties of
		* a function, including:
		* 
		* - A name
		* - Whether it's a static method, definition etc.
		* - Template variables
		* - Parameter variables
		* - Const predicate
		* - Require predicate
		* - A scope containing statements
		*/
		class Function: public SEM::TemplatedObject {
		public:
			Function();
			~Function();
			
			SEM::GlobalStructure& parent();
			const SEM::GlobalStructure& parent() const;
			void setParent(SEM::GlobalStructure parent);
			
			SEM::Namespace& nameSpace();
			const SEM::Namespace& nameSpace() const;
			
			const ModuleScope& moduleScope() const;
			void setModuleScope(ModuleScope moduleScope);
			
			/**
			 * \brief Get/set whether this function is auto-generated.
			 * 
			 * Default functions are generated for various types
			 * and provide a generic implementation (e.g. copy an
			 * object by copying each member value) that doesn't
			 * have to be specified manually.
			 */
			bool isAutoGenerated() const;
			void setAutoGenerated(bool value);
			
			/**
			 * \brief Get/set whether this function is a static method.
			 */
			bool isStatic() const;
			void setIsStatic(bool value);
			
			/**
			 * \brief Get/set whether this function is vararg.
			 */
			bool isVarArg() const;
			void setIsVarArg(bool value);
			
			/**
			 * \brief Get/set whether this function is marked 'import'.
			 */
			bool isImported() const;
			void setIsImported(bool value);
			
			/**
			 * \brief Get/set whether this function is marked 'export'.
			 */
			bool isExported() const;
			void setIsExported(bool value);
			
			/**
			 * \brief Get/set whether this function is primitive.
			 * 
			 * A primitive function is an 'axiom' of the language,
			 * such as the methods of type 'int'.
			 */
			bool isPrimitive() const;
			void setIsPrimitive(bool value);
			
			/**
			 * \brief Get/set whether this function is a method.
			 * 
			 * NOTE: This returns true for static methods.
			 */
			void setMethod(bool pIsMethod);
			bool isMethod() const;
			
			bool isStaticMethod() const;
			
			const Node<Name>& nameDecl() const;
			void setNameDecl(Node<Name> nameDecl);
			
			const Name& fullName() const;
			void setFullName(Name fullName);
			
			String canonicalName() const;
			
			void setType(SEM::FunctionType type);
			const SEM::FunctionType& type() const;
			
			Node<TypeDecl>& returnType();
			const Node<TypeDecl>& returnType() const;
			void setReturnType(Node<TypeDecl> returnType);
			
			const Node<VarList>& parameterDecls() const;
			void setParameterDecls(Node<VarList> parameterDecls);
			
			const std::vector<Var*>& parameters() const;
			void setParameters(std::vector<Var*> parameters);
			
			bool hasScopeDecl() const;
			const Node<Scope>& scopeDecl() const;
			void setScopeDecl(Node<Scope> scopeDecl);
			
			bool hasGeneratedScope() const;
			const SEM::Scope& scope() const;
			void setScope(std::unique_ptr<SEM::Scope> scope);
			
			const Node<ConstSpecifier>& constSpecifier() const;
			void setConstSpecifier(Node<ConstSpecifier> constSpecifier);
			
			const SEM::Predicate& constPredicate() const;
			void setConstPredicate(SEM::Predicate predicate);
			
			const Node<RequireSpecifier>& noexceptSpecifier() const;
			void setNoexceptSpecifier(Node<RequireSpecifier> noexceptSpecifier);
			
			const SEM::Predicate& noexceptPredicate() const;
			
			const Node<RequireSpecifier>& requireSpecifier() const;
			void setRequireSpecifier(Node<RequireSpecifier> requireSpecifier);
			
			const SEM::Predicate& requiresPredicate() const;
			void setRequiresPredicate(SEM::Predicate predicate);
			
			const Node<TemplateVarList>& templateVariableDecls() const;
			void setTemplateVariableDecls(Node<TemplateVarList> templateVariables);
			
			TemplateVarArray& templateVariables();
			const TemplateVarArray& templateVariables() const;
			
			FastMap<String, TemplateVar*>& namedTemplateVariables();
			const FastMap<String, TemplateVar*>& namedTemplateVariables() const;
			
			FastMap<String, Var*>& namedVariables();
			const FastMap<String, Var*>& namedVariables() const;
			
			void setDebugInfo(Debug::FunctionInfo debugInfo);
			const Optional<Debug::FunctionInfo>& debugInfo() const;
			
			std::string toString() const;
			
		private:
			Optional<SEM::GlobalStructure> parent_;
			
			bool isAutoGenerated_;
			bool isMethod_, isStatic_;
			bool isImported_, isExported_;
			bool isPrimitive_, isVarArg_;
			
			Node<Name> nameDecl_;
			Name fullName_;
			
			Node<TemplateVarList> templateVariableDecls_;
			TemplateVarArray templateVariables_;
			FastMap<String, TemplateVar*> namedTemplateVariables_;
			
			SEM::FunctionType type_;
			Node<TypeDecl> returnType_;
			
			Node<VarList> parameterDecls_;
			std::vector<Var*> parameters_;
			FastMap<String, Var*> namedVariables_;
			
			Node<Scope> scopeDecl_;
			std::unique_ptr<SEM::Scope> scope_;
			
			Node<ConstSpecifier> constSpecifier_;
			SEM::Predicate constPredicate_;
			
			Node<RequireSpecifier> noexceptSpecifier_;
			
			Node<RequireSpecifier> requireSpecifier_;
			SEM::Predicate requiresPredicate_;
			
			Optional<Debug::FunctionInfo> debugInfo_;
			
			ModuleScope moduleScope_;
			
		};
		
		typedef std::vector<Node<Function>> FunctionList;
		
	}
	
}

#endif
