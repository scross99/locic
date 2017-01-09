#include <locic/AST.hpp>
#include <locic/AST/Type.hpp>
#include <locic/SemanticAnalysis/Context.hpp>
#include <locic/SemanticAnalysis/ConvertVar.hpp>
#include <locic/SemanticAnalysis/Exception.hpp>
#include <locic/SemanticAnalysis/ScopeStack.hpp>
#include <locic/SemanticAnalysis/TypeResolver.hpp>

namespace locic {
	
	namespace SemanticAnalysis {
		
		class ExceptionCircularInheritanceDiag: public Error {
		public:
			ExceptionCircularInheritanceDiag(std::string typeName)
			: typeName_(std::move(typeName)) { }
			
			std::string toString() const {
				return makeString("exception type '%s' inherits itself via a circular dependency",
				                  typeName_.c_str());
			}
			
		private:
			std::string typeName_;
			
		};
		
		bool hasInheritanceCycle(const AST::TypeInstance& typeInstance) {
			auto parentType = typeInstance.parentType();
			while (parentType != nullptr && parentType->isException()) {
				if (parentType->getObjectType() == &typeInstance) {
					return true;
				}
				
				parentType = parentType->getObjectType()->parentType();
			}
			
			return false;
		}
		
		void checkForInheritanceCycle(Context& context, const AST::TypeInstance& rootTypeInstance) {
			auto typeInstance = &rootTypeInstance;
			while (true) {
				if (hasInheritanceCycle(*typeInstance)) {
					context.issueDiag(ExceptionCircularInheritanceDiag(typeInstance->fullName().toString()),
					                  typeInstance->debugInfo()->location);
				}
					
				const auto parentType = typeInstance->parentType();
				if (parentType == nullptr || !parentType->isException()) {
					break;
				}
				
				typeInstance = parentType->getObjectType();
				if (typeInstance == &rootTypeInstance) {
					break;
				}
			}
		}
		
		class ExceptionCannotInheritNonExceptionTypeDiag: public Error {
		public:
			ExceptionCannotInheritNonExceptionTypeDiag(const AST::TypeInstance& exceptionType,
			                                           const AST::Type* inheritType)
			: exceptionType_(exceptionType), inheritType_(inheritType) { }
			
			std::string toString() const {
				return makeString("'%s' cannot inherit from non-exception type '%s'",
				                  exceptionType_.fullName().toString(/*addPrefix=*/false).c_str(),
				                  inheritType_->toString().c_str());
			}
			
		private:
			const AST::TypeInstance& exceptionType_;
			const AST::Type* inheritType_;
			
		};
		
		class PatternMemberVarsNotSupportedDiag: public Error {
		public:
			PatternMemberVarsNotSupportedDiag() { }
			
			std::string toString() const {
				return "pattern variables not supported for member variables";
			}
			
		};
		
		// Fill in type instance structures with member variable information.
		void AddTypeInstanceMemberVariables(Context& context, const AST::Node<AST::TypeInstance>& typeInstanceNode) {
			assert(typeInstanceNode->variables().empty());
			assert(typeInstanceNode->constructTypes().empty());
			
			if (typeInstanceNode->isException()) {
				// Add exception type parent using initializer.
				const auto& astInitializerNode = typeInstanceNode->initializer;
				if (astInitializerNode->kind == AST::ExceptionInitializer::INITIALIZE) {
					const auto astType = TypeResolver(context).resolveObjectType(astInitializerNode->symbol);
					
					if (!astType->isException()) {
						context.issueDiag(ExceptionCannotInheritNonExceptionTypeDiag(*typeInstanceNode, astType),
						                  astInitializerNode->symbol.location());
					}
					
					typeInstanceNode->setParentType(astType);
					
					checkForInheritanceCycle(context, *typeInstanceNode);
					
					// Also add parent as first member variable.
					// FIXME: We shouldn't be creating an AST::Var here; the solution
					//        is to remove this variable and have code that accesses
					//        it generated by CodeGen.
					auto parentVar = AST::Var::NamedVar(AST::Node<AST::TypeDecl>(),
					                                    String());
					parentVar->setType(astType);
					typeInstanceNode->attachVariable(*parentVar);
				}
			}
			
			for (auto& astVarNode: *(typeInstanceNode->variableDecls)) {
				if (!astVarNode->isNamed()) {
					context.issueDiag(PatternMemberVarsNotSupportedDiag(),
					                  astVarNode.location());
				}
				
				auto var = ConvertVar(context, Debug::VarInfo::VAR_MEMBER, astVarNode);
				typeInstanceNode->attachVariable(*var);
			}
		}
		
		void AddNamespaceDataTypeMemberVariables(Context& context, const AST::Node<AST::NamespaceData>& astNamespaceDataNode) {
			for (const auto& astChildNamespaceNode: astNamespaceDataNode->namespaces) {
				auto& astChildNamespace = astChildNamespaceNode->nameSpace();
				
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::Namespace(astChildNamespace));
				AddNamespaceDataTypeMemberVariables(context, astChildNamespaceNode->data());
			}
			
			for (const auto& astModuleScopeNode: astNamespaceDataNode->moduleScopes) {
				AddNamespaceDataTypeMemberVariables(context, astModuleScopeNode->data());
			}
			
			for (const auto& typeInstanceNode: astNamespaceDataNode->typeInstances) {
				{
					PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TypeInstance(*typeInstanceNode));
					AddTypeInstanceMemberVariables(context, typeInstanceNode);
				}
				
				if (typeInstanceNode->isUnionDatatype()) {
					for (const auto& variantNode: *(typeInstanceNode->variantDecls)) {
						PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TypeInstance(*variantNode));
						AddTypeInstanceMemberVariables(context, variantNode);
					}
				}
			}
		}
		
		void AddTypeMemberVariablesPass(Context& context, const AST::NamespaceList& rootASTNamespaces) {
			for (const auto& astNamespaceNode: rootASTNamespaces) {
				AddNamespaceDataTypeMemberVariables(context, astNamespaceNode->data());
			}
		}
		
	}
	
}
