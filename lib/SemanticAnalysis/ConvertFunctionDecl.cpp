#include <cassert>
#include <cstddef>
#include <cstdio>
#include <vector>
#include <locic/AST.hpp>
#include <locic/AST/Type.hpp>

#include <locic/SemanticAnalysis/Context.hpp>
#include <locic/SemanticAnalysis/ConvertPredicate.hpp>
#include <locic/SemanticAnalysis/ConvertVar.hpp>
#include <locic/SemanticAnalysis/Exception.hpp>
#include <locic/SemanticAnalysis/ScopeElement.hpp>
#include <locic/SemanticAnalysis/ScopeStack.hpp>
#include <locic/SemanticAnalysis/Template.hpp>
#include <locic/SemanticAnalysis/TypeResolver.hpp>
#include <locic/Support/MethodID.hpp>
#include <locic/Support/MethodIDMap.hpp>
#include <locic/Support/SharedMaps.hpp>

namespace locic {

	namespace SemanticAnalysis {
		
		AST::GlobalStructure getParent(const ScopeElement& topElement) {
			assert(topElement.isNamespace() || topElement.isTypeInstance());
			if (topElement.isNamespace()) {
				return AST::GlobalStructure::Namespace(topElement.nameSpace());
			} else {
				return AST::GlobalStructure::TypeInstance(topElement.typeInstance());
			}
		}
		
		Diag
		ShadowsTemplateParameterDiag(String name) {
			return Error("declaration of '%s' shadows template parameter",
			             name.c_str());
		}
		
		Diag
		InterfaceMethodCannotBeTemplatedDiag(String name) {
			return Error("interface method '%s' cannot be templated",
			             name.c_str());
		}
		
		Diag
		FunctionCannotHaveConstSpecifierDiag(const Name& name) {
			return Warning("non-method function '%s' cannot have const specifier",
			               name.toString(/*addPrefix=*/false).c_str());
		}
		
		Diag
		FunctionCannotBeStaticDiag(const Name& name) {
			return Warning("non-method function '%s' cannot be static",
			               name.toString(/*addPrefix=*/false).c_str());
		}
		
		Diag
		FunctionTemplateHasNonPrimitiveTypeDiag(const String& varName,
		                                        const AST::Type* const type,
		                                        const Name& functionName) {
			return Error("template variable '%s' has non-primitive type '%s' in function '%s'",
			             varName.c_str(), type->toDiagString().c_str(),
			             functionName.toString(/*addPrefix=*/false).c_str());
		}
		
		void
		ConvertFunctionDecl(Context& context, AST::Node<AST::Function>& function) {
			const auto thisTypeInstance = lookupParentType(context.scopeStack());
			
			const auto& name = function->nameDecl()->last();
			
			function->setParent(getParent(context.scopeStack().back()));
			
			const bool isMethod = thisTypeInstance != nullptr;
			
			if (!isMethod && !function->constSpecifier()->isNone()) {
				context.issueDiag(FunctionCannotHaveConstSpecifierDiag(function->fullName()),
				                  function->constSpecifier().location());
			}
			
			if (!isMethod && function->isStatic()) {
				context.issueDiag(FunctionCannotBeStaticDiag(function->fullName()),
				                  function.location());
			}
			
			// Methods of a primitive type will be primitive, unless
			// they are extension methods.
			if (isMethod && thisTypeInstance->isPrimitive() && function->nameDecl()->size() == 1) {
				function->setIsPrimitive(true);
			}
			
			function->setMethod(isMethod);
			
			if (!function->templateVariableDecls()->empty() && (thisTypeInstance != nullptr && thisTypeInstance->isInterface())) {
				context.issueDiag(InterfaceMethodCannotBeTemplatedDiag(name),
				                  function.location());
			}
			
			// Add template variables.
			size_t templateVarIndex = (thisTypeInstance != nullptr) ? thisTypeInstance->templateVariables().size() : 0;
			for (const auto& templateVarNode: *(function->templateVariableDecls())) {
				const auto templateVarName = templateVarNode->name();
				
				templateVarNode->setContext(context.astContext());
				templateVarNode->setFullName(function->fullName() + templateVarName);
				templateVarNode->setIndex(templateVarIndex++);
				
				const auto templateVarIterator = function->namedTemplateVariables().find(templateVarName);
				if (templateVarIterator == function->namedTemplateVariables().end()) {
					function->namedTemplateVariables().insert(std::make_pair(templateVarName, templateVarNode.get()));
				} else {
					context.issueDiag(ShadowsTemplateParameterDiag(templateVarName),
					                  templateVarNode.location());
				}
				
				// Also adding the function template variable type here.
				auto& astVarTypeDecl = templateVarNode->typeDecl();
				const auto astVarType = TypeResolver(context).resolveTemplateVarType(astVarTypeDecl);
				
				if (!astVarType->isPrimitive()) {
					context.issueDiag(FunctionTemplateHasNonPrimitiveTypeDiag(templateVarName,
					                                                          astVarType,
					                                                          function->fullName()),
					                  templateVarNode->typeDecl().location());
				}
				
				templateVarNode->setType(astVarType);
				function->templateVariables().push_back(templateVarNode.get());
			}
		}
		
		bool isValidLifetimeMethod(const MethodID methodID) {
			switch (methodID) {
				case METHOD_MOVE:
				case METHOD_DESTROY:
				case METHOD_ALIGNMASK:
				case METHOD_SIZEOF:
				case METHOD_ISLIVE:
				case METHOD_SETDEAD:
				case METHOD_DEAD:
				case METHOD_ISVALID:
				case METHOD_SETINVALID:
					return true;
				default:
					return false;
			}
		}
		
		bool isValidReturnType(const MethodID methodID, const AST::TypeInstance* thisTypeInstance,
		                       const AST::Type* const type) {
			switch (methodID) {
				case METHOD_MOVE: {
					if (thisTypeInstance == nullptr) {
						return false;
					}
					const auto selfType = thisTypeInstance->selfType();
					if (type == selfType) {
						return true;
					}
					if (thisTypeInstance->isInterface() || thisTypeInstance->isPrimitive()) {
						return true;
					}
					return false;
				}
				case METHOD_DESTROY:
					return type->isBuiltInVoid();
				case METHOD_ALIGNMASK:
				case METHOD_SIZEOF:
					return type->isPrimitive() &&
					       type->primitiveID() == PrimitiveSize;
				case METHOD_ISLIVE:
					return type->isBuiltInBool();
				case METHOD_SETDEAD:
					return type->isBuiltInVoid();
				case METHOD_DEAD:
					// TODO
					return true;
				case METHOD_ISVALID:
					return type->isBuiltInBool();
				case METHOD_SETINVALID:
					return type->isBuiltInVoid();
				default:
					locic_unreachable("Unknown lifetime method.");
			}
		}
		
		bool isValidArgumentCount(const MethodID methodID, const size_t argCount) {
			switch (methodID) {
				case METHOD_MOVE:
					return argCount == 0;
				case METHOD_DESTROY:
					return argCount == 0;
				case METHOD_ALIGNMASK:
					return argCount == 0;
				case METHOD_SIZEOF:
					return argCount == 0;
				case METHOD_ISLIVE:
					return argCount == 0;
				case METHOD_SETDEAD:
					return argCount == 0;
				case METHOD_DEAD:
					return argCount == 0;
				case METHOD_ISVALID:
					return argCount == 0;
				case METHOD_SETINVALID:
					return argCount == 0;
				default:
					locic_unreachable("Unknown lifetime method.");
			}
		}
		
		bool isValidArgumentTypes(const MethodID methodID, const AST::TypeArray& /*types*/) {
			switch (methodID) {
				case METHOD_MOVE:
				case METHOD_DESTROY:
				case METHOD_ALIGNMASK:
				case METHOD_SIZEOF:
				case METHOD_ISLIVE:
				case METHOD_SETDEAD:
				case METHOD_DEAD:
				case METHOD_ISVALID:
				case METHOD_SETINVALID:
					return true;
				default:
					locic_unreachable("Unknown lifetime method.");
			}
		}
		
		bool isValidConstness(const MethodID methodID, const AST::Predicate& constPredicate) {
			switch (methodID) {
				case METHOD_MOVE:
					return constPredicate.isFalse();
				case METHOD_DESTROY:
					return constPredicate.isFalse();
				case METHOD_ALIGNMASK:
					return true;
				case METHOD_SIZEOF:
					return true;
				case METHOD_ISLIVE:
					return constPredicate.isTrue();
				case METHOD_SETDEAD:
					return constPredicate.isFalse();
				case METHOD_DEAD:
					return true;
				case METHOD_ISVALID:
					return constPredicate.isTrue();
				case METHOD_SETINVALID:
					return constPredicate.isFalse();
				default:
					locic_unreachable("Unknown lifetime method.");
			}
		}
		
		bool isValidStaticness(const MethodID methodID, const bool isStatic) {
			switch (methodID) {
				case METHOD_MOVE:
					return !isStatic;
				case METHOD_DESTROY:
					return !isStatic;
				case METHOD_ALIGNMASK:
					return isStatic;
				case METHOD_SIZEOF:
					return isStatic;
				case METHOD_ISLIVE:
					return !isStatic;
				case METHOD_SETDEAD:
					return !isStatic;
				case METHOD_DEAD:
					return isStatic;
				case METHOD_ISVALID:
					return !isStatic;
				case METHOD_SETINVALID:
					return !isStatic;
				default:
					locic_unreachable("Unknown lifetime method.");
			}
		}
		
		class UnknownLifetimeMethodDiag: public ErrorDiag {
		public:
			UnknownLifetimeMethodDiag(String functionName)
			: functionName_(std::move(functionName)) { }
			
			std::string toString() const {
				return makeString("unknown lifetime method '%s'",
				                  functionName_.c_str());
			}
			
		private:
			String functionName_;
			
		};
		
		class LifetimeMethodInvalidReturnTypeDiag: public ErrorDiag {
		public:
			LifetimeMethodInvalidReturnTypeDiag(std::string functionName)
			: functionName_(std::move(functionName)) { }
			
			std::string toString() const {
				return makeString("lifetime method '%s' has incorrect return type",
				                  functionName_.c_str());
			}
			
		private:
			std::string functionName_;
			
		};
		
		class LifetimeMethodInvalidArgumentCountDiag: public ErrorDiag {
		public:
			LifetimeMethodInvalidArgumentCountDiag(std::string functionName)
			: functionName_(std::move(functionName)) { }
			
			std::string toString() const {
				return makeString("lifetime method '%s' has incorrect argument count",
				                  functionName_.c_str());
			}
			
		private:
			std::string functionName_;
			
		};
		
		class LifetimeMethodInvalidArgumentTypesDiag: public ErrorDiag {
		public:
			LifetimeMethodInvalidArgumentTypesDiag(std::string functionName)
			: functionName_(std::move(functionName)) { }
			
			std::string toString() const {
				return makeString("lifetime method '%s' has incorrect argument types",
				                  functionName_.c_str());
			}
			
		private:
			std::string functionName_;
			
		};
		
		class LifetimeMethodShouldBeStaticDiag: public ErrorDiag {
		public:
			LifetimeMethodShouldBeStaticDiag(std::string functionName)
			: functionName_(std::move(functionName)) { }
			
			std::string toString() const {
				return makeString("lifetime method '%s' should be static",
				                  functionName_.c_str());
			}
			
		private:
			std::string functionName_;
			
		};
		
		class LifetimeMethodShouldNotBeStaticDiag: public ErrorDiag {
		public:
			LifetimeMethodShouldNotBeStaticDiag(std::string functionName)
			: functionName_(std::move(functionName)) { }
			
			std::string toString() const {
				return makeString("lifetime method '%s' should not be static",
				                  functionName_.c_str());
			}
			
		private:
			std::string functionName_;
			
		};
		
		class LifetimeMethodInvalidConstPredicateDiag: public ErrorDiag {
		public:
			LifetimeMethodInvalidConstPredicateDiag(std::string functionName)
			: functionName_(std::move(functionName)) { }
			
			std::string toString() const {
				return makeString("lifetime method '%s' has incorrect const predicate",
				                  functionName_.c_str());
			}
			
		private:
			std::string functionName_;
			
		};
		
		void validateFunctionType(Context& context, const Name& functionFullName,
		                          const AST::TypeInstance* thisTypeInstance,
		                          const AST::FunctionType& functionType,
		                          const AST::Predicate& constPredicate,
		                          const Debug::SourceLocation& location) {
			const auto& name = functionFullName.last();
			if (!name.starts_with("__")) {
				// Not a lifetime method; any type can be valid.
				return;
			}
			
			const auto methodID = context.sharedMaps().methodIDMap().tryGetMethodID(name);
			if (!methodID || !isValidLifetimeMethod(*methodID)) {
				context.issueDiag(UnknownLifetimeMethodDiag(name), location);
				return;
			}
			
			if (!isValidReturnType(*methodID, thisTypeInstance, functionType.returnType())) {
				context.issueDiag(LifetimeMethodInvalidReturnTypeDiag(functionFullName.toString()),
				                  location);
			}
			
			if (!isValidArgumentCount(*methodID, functionType.parameterTypes().size())) {
				context.issueDiag(LifetimeMethodInvalidArgumentCountDiag(functionFullName.toString()),
				                  location);
			} else if (!isValidArgumentTypes(*methodID, functionType.parameterTypes())) {
				context.issueDiag(LifetimeMethodInvalidArgumentTypesDiag(functionFullName.toString()),
				                  location);
			}
			
			const bool isStatic = !functionType.attributes().isMethod();
			if (!isValidStaticness(*methodID, isStatic)) {
				if (isStatic) {
					context.issueDiag(LifetimeMethodShouldNotBeStaticDiag(functionFullName.toString()),
					                  location);
				} else {
					context.issueDiag(LifetimeMethodShouldBeStaticDiag(functionFullName.toString()),
					                  location);
				}
			} else if (!isValidConstness(*methodID, constPredicate)) {
				context.issueDiag(LifetimeMethodInvalidConstPredicateDiag(functionFullName.toString()),
				                  location);
			}
		}
		
		class LifetimeMethodNotNoExceptDiag: public ErrorDiag {
		public:
			LifetimeMethodNotNoExceptDiag(std::string functionName)
			: functionName_(std::move(functionName)) { }
			
			std::string toString() const {
				return makeString("lifetime method '%s' isn't marked 'noexcept'",
				                  functionName_.c_str());
			}
			
		private:
			std::string functionName_;
			
		};
		
		class PatternMatchingNotSupportedForParameterVariablesDiag: public ErrorDiag {
		public:
			PatternMatchingNotSupportedForParameterVariablesDiag() { }
			
			std::string toString() const {
				return "pattern matching not supported for parameter variables";
			}
			
		};
		
		void ConvertFunctionDeclType(Context& context, AST::Node<AST::Function>& function) {
			if (function->isAutoGenerated()) {
				// Auto-generated functions have already had their
				// type converted.
				return;
			}
			
			const auto thisTypeInstance = lookupParentType(context.scopeStack());
			
			// Enable lookups for function template variables.
			PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::Function(*function));
			
			// Convert const specifier.
			if (!function->constSpecifier().isNull()) {
				function->setConstPredicate(ConvertConstSpecifier(context, function->constSpecifier()));
			}
			
			auto& astReturnTypeNode = function->returnType();
			const AST::Type* astReturnType = NULL;
			
			if (astReturnTypeNode->typeEnum == AST::TypeDecl::AUTO) {
				// Undefined return type means this must be a class
				// constructor, with no return type specified (i.e.
				// the return type will be the parent class type).
				assert(thisTypeInstance != nullptr);
				//assert(function->hasScope());
				//assert(function->isStatic());
				
				astReturnType = thisTypeInstance->selfType();
			} else {
				astReturnType = TypeResolver(context).resolveType(astReturnTypeNode);
			}
			
			std::vector<AST::Var*> parameterVars;
			parameterVars.reserve(function->parameterDecls()->size());
			
			AST::TypeArray parameterTypes;
			parameterTypes.reserve(function->parameterDecls()->size());
			
			size_t index = 0;
			
			for (auto& astVarNode: *(function->parameterDecls())) {
				if (!astVarNode->isNamed()) {
					context.issueDiag(PatternMatchingNotSupportedForParameterVariablesDiag(),
					                  astVarNode.location());
					continue;
				}
				
				auto paramVar = ConvertVar(context, Debug::VarInfo::VAR_ARGUMENT, astVarNode);
				assert(paramVar->isNamed());
				
				paramVar->setIndex(index++);
				
				parameterTypes.push_back(paramVar->type());
				parameterVars.push_back(paramVar);
			}
			
			function->setParameters(std::move(parameterVars));
			
			auto noExceptPredicate = ConvertNoExceptSpecifier(context, function->noexceptSpecifier());
			if (function->fullName().last() == "__destroy") {
				// Destructors are always noexcept.
				noExceptPredicate = AST::Predicate::True();
			}
			
			if (!noExceptPredicate.isTrue() && function->fullName().last().starts_with("__")) {
				context.issueDiag(LifetimeMethodNotNoExceptDiag(function->fullName().toString()),
				                  function.location());
				noExceptPredicate = AST::Predicate::True();
			}
			
			const bool isDynamicMethod = function->isMethod() && !function->isStatic();
			const bool isTemplatedMethod = !function->templateVariables().empty() ||
				(thisTypeInstance != nullptr && !thisTypeInstance->templateVariables().empty());
			
			AST::FunctionAttributes attributes(function->isVarArg(), isDynamicMethod, isTemplatedMethod, std::move(noExceptPredicate));
			AST::FunctionType functionType(std::move(attributes), astReturnType, std::move(parameterTypes));
			validateFunctionType(context, function->fullName(),
			                     thisTypeInstance, functionType,
			                     function->constPredicate(),
			                     function.location());
			
			function->setType(functionType);
		}
		
	}
	
}


