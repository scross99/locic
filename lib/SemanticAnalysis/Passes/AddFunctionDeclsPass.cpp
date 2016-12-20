#include <locic/AST.hpp>
#include <locic/SemanticAnalysis/Context.hpp>
#include <locic/SemanticAnalysis/ConvertFunctionDecl.hpp>
#include <locic/SemanticAnalysis/ConvertModuleScope.hpp>
#include <locic/SemanticAnalysis/DefaultMethods.hpp>
#include <locic/SemanticAnalysis/Exception.hpp>
#include <locic/SemanticAnalysis/NameSearch.hpp>
#include <locic/SemanticAnalysis/ScopeStack.hpp>
#include <locic/SemanticAnalysis/SearchResult.hpp>

namespace locic {
	
	namespace SemanticAnalysis {
		
		Debug::FunctionInfo makeFunctionInfo(const AST::Node<AST::Function>& function) {
			Debug::FunctionInfo functionInfo;
			functionInfo.name = function->fullName().copy();
			functionInfo.declLocation = function.location();
			
			// TODO
			functionInfo.scopeLocation = function.location();
			return functionInfo;
		}
		
		class CannotNestImportedFunctionInModuleScopeDiag: public Error {
		public:
			CannotNestImportedFunctionInModuleScopeDiag(Name name)
			: name_(std::move(name)) { }
			
			std::string toString() const {
				return makeString("cannot nest imported function '%s' in module scope",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		class CannotNestExportedFunctionInModuleScopeDiag: public Error {
		public:
			CannotNestExportedFunctionInModuleScopeDiag(Name name)
			: name_(std::move(name)) { }
			
			std::string toString() const {
				return makeString("cannot nest exported function '%s' in module scope",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		AST::ModuleScope
		getFunctionScope(Context& context, const AST::Node<AST::Function>& function,
		                 const AST::ModuleScope& moduleScope) {
			if (function->isImported()) {
				if (!moduleScope.isInternal()) {
					context.issueDiag(CannotNestImportedFunctionInModuleScopeDiag(function->nameDecl()->copy()),
					                  function.location());
				}
				return AST::ModuleScope::Import(Name::Absolute(), Version(0,0,0));
			} else if (function->isExported()) {
				if (!moduleScope.isInternal()) {
					context.issueDiag(CannotNestExportedFunctionInModuleScopeDiag(function->nameDecl()->copy()),
					                  function.location());
				}
				return AST::ModuleScope::Export(Name::Absolute(), Version(0,0,0));
			} else {
				return moduleScope.copy();
			}
		}
		
		class DefinitionRequiredForInternalFunctionDiag: public Error {
		public:
			DefinitionRequiredForInternalFunctionDiag(Name name)
			: name_(std::move(name)) { }
			
			std::string toString() const {
				return makeString("definition required for internal function '%s'",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		class CannotDefineImportedFunctionDiag: public Error {
		public:
			CannotDefineImportedFunctionDiag(Name name)
			: name_(std::move(name)) { }
			
			std::string toString() const {
				return makeString("cannot define imported function '%s'",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		class DefinitionRequiredForExportedFunctionDiag: public Error {
		public:
			DefinitionRequiredForExportedFunctionDiag(Name name)
			: name_(std::move(name)) { }
			
			std::string toString() const {
				return makeString("definition required for exported function '%s'",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		void
		AddFunctionDecl(Context& context, AST::Node<AST::Function>& function,
		                const Name& fullName, const AST::ModuleScope& parentModuleScope) {
			const auto& topElement = context.scopeStack().back();
			
			const auto moduleScope = getFunctionScope(context, function, parentModuleScope);
			
			const bool isParentInterface = topElement.isTypeInstance() && topElement.typeInstance().isInterface();
			const bool isParentPrimitive = topElement.isTypeInstance() && topElement.typeInstance().isPrimitive();
			
			const bool isDefinition = function->hasScopeDecl() || function->isAutoGenerated();
			
			switch (moduleScope.kind()) {
				case AST::ModuleScope::INTERNAL: {
					const bool isPrimitive = isParentPrimitive || function->isPrimitive();
					if (!isParentInterface && !isPrimitive && !isDefinition) {
						context.issueDiag(DefinitionRequiredForInternalFunctionDiag(fullName.copy()),
						                  function.location());
					}
					break;
				}
				case AST::ModuleScope::IMPORT: {
					if (!isParentInterface && isDefinition) {
						context.issueDiag(CannotDefineImportedFunctionDiag(fullName.copy()),
						                  function.location());
					}
					break;
				}
				case AST::ModuleScope::EXPORT: {
					if (!isParentInterface && !isDefinition) {
						context.issueDiag(DefinitionRequiredForExportedFunctionDiag(fullName.copy()),
						                  function.location());
					}
					break;
				}
			}
			
			function->setFullName(fullName.copy());
			function->setModuleScope(moduleScope.copy());
			
			if (function->isAutoGenerated()) {
				assert(topElement.isTypeInstance());
				
				function->setMethod(true);
				DefaultMethods(context).completeDefaultMethodDecl(&(topElement.typeInstance()),
				                                                  function);
			} else {
				ConvertFunctionDecl(context, function);
			}
			
			const auto functionInfo = makeFunctionInfo(function);
			function->setDebugInfo(functionInfo);
		}
		
		class FunctionClashesWithExistingNameDiag: public Error {
		public:
			FunctionClashesWithExistingNameDiag(const Name& name)
			: name_(name.copy()) { }
			
			std::string toString() const {
				return makeString("function '%s' clashes with existing name",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		class UnknownParentTypeForExceptionMethodDiag: public Error {
		public:
			UnknownParentTypeForExceptionMethodDiag(Name parentName, String methodName)
			: parentName_(std::move(parentName)), methodName_(methodName) { }
			
			std::string toString() const {
				return makeString("unknown parent type '%s' of extension method '%s'",
				                  parentName_.toString(/*addPrefix=*/false).c_str(),
				                  methodName_.c_str());
			}
			
		private:
			Name parentName_;
			String methodName_;
			
		};
		
		class ParentOfExtentionMethodIsNotATypeDiag: public Error {
		public:
			ParentOfExtentionMethodIsNotATypeDiag(Name parentName, String methodName)
			: parentName_(std::move(parentName)), methodName_(methodName) { }
			
			std::string toString() const {
				return makeString("parent '%s' of extension method '%s' is not a type",
				                  parentName_.toString(/*addPrefix=*/false).c_str(),
				                  methodName_.c_str());
			}
			
		private:
			Name parentName_;
			String methodName_;
			
		};
		
		namespace {
			
			class PreviousDefinedDiag: public Error {
			public:
				PreviousDefinedDiag() { }
				
				std::string toString() const {
					return "previously defined here";
				}
				
			};
			
		}
		
		void AddNamespaceFunctionDecl(Context& context, AST::Node<AST::Function>& function,
		                              const AST::ModuleScope& moduleScope) {
			auto& parentNamespace = context.scopeStack().back().nameSpace();
			
			const auto& name = function->nameDecl();
			assert(!name->empty());
			
			if (name->size() == 1) {
				// Just a normal function.
				const auto fullName = parentNamespace.name() + name->last();
				
				const auto iterator = parentNamespace.items().find(name->last());
				if (iterator != parentNamespace.items().end()) {
					OptionalDiag previousDefinedDiag(PreviousDefinedDiag(), iterator->second.location());
					context.issueDiag(FunctionClashesWithExistingNameDiag(fullName),
					                  name.location(), std::move(previousDefinedDiag));
				}
				
				AddFunctionDecl(context, function, fullName, moduleScope);
				
				parentNamespace.items().insert(std::make_pair(name->last(), AST::NamespaceItem::Function(*function)));
			} else {
				// An extension method; search for the parent type.
				assert(name->size() > 1);
				const auto searchResult = performSearch(context, name->getPrefix());
				if (searchResult.isNone()) {
					context.issueDiag(UnknownParentTypeForExceptionMethodDiag(name->getPrefix(), name->last()),
					                  name.location());
					return;
				}
				
				if (!searchResult.isTypeInstance()) {
					context.issueDiag(ParentOfExtentionMethodIsNotATypeDiag(name->getPrefix(), name->last()),
					                  name.location());
					return;
				}
				
				auto& parentTypeInstance = searchResult.typeInstance();
				
				// Push the type instance on the scope stack, since the extension method is
				// effectively within the scope of the type instance.
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TypeInstance(parentTypeInstance));
				
				const auto fullName = parentTypeInstance.fullName() + name->last();
				
				AddFunctionDecl(context, function, fullName, moduleScope);
				
				parentTypeInstance.attachFunction(*function);
			}
		}
		
		class DestructorMethodClashDiag: public Error {
		public:
			DestructorMethodClashDiag() { }
			
			std::string toString() const {
				return "destructor method clashes with previous destructor method";
			}
			
		};
		
		class MethodNameClashDiag: public Error {
		public:
			MethodNameClashDiag(String methodName)
			: methodName_(methodName) { }
			
			std::string toString() const {
				return makeString("method '%s' clashes with previous method of the same name",
				                  methodName_.c_str());
			}
			
		private:
			String methodName_;
			
		};
		
		class MethodNameCanonicalizationClashDiag: public Error {
		public:
			MethodNameCanonicalizationClashDiag(String methodName, String previousMethodName)
			: methodName_(methodName), previousMethodName_(previousMethodName) { }
			
			std::string toString() const {
				return makeString("method '%s' clashes with previous method '%s' due to method canonicalization",
				                  methodName_.c_str(), previousMethodName_.c_str());
			}
			
		private:
			String methodName_;
			String previousMethodName_;
			
		};
		
		void AddTypeInstanceFunctionDecl(Context& context, AST::Node<AST::Function>& function,
		                                 const AST::ModuleScope& moduleScope) {
			auto& parentTypeInstance = context.scopeStack().back().typeInstance();
			
			const auto& name = function->nameDecl()->last();
			auto canonicalMethodName = CanonicalizeMethodName(name);
			const auto fullName = parentTypeInstance.fullName() + name;
			
			const auto existingFunction = parentTypeInstance.findFunction(canonicalMethodName);
			if (existingFunction != nullptr) {
				const auto previousName = existingFunction->fullName().last();
				if (name == "__destroy") {
					context.issueDiag(DestructorMethodClashDiag(),
					                  function->nameDecl().location());
				} else if (previousName == name) {
					context.issueDiag(MethodNameClashDiag(name),
					                  function->nameDecl().location());
				} else {
					context.issueDiag(MethodNameCanonicalizationClashDiag(name, previousName),
					                  function->nameDecl().location());
				}
			}
			
			AddFunctionDecl(context, function, fullName, moduleScope);
			parentTypeInstance.attachFunction(*function);
		}
		
		class EnumConstructorClashesWithExistingNameDiag: public Error {
		public:
			EnumConstructorClashesWithExistingNameDiag(const Name& name)
			: name_(name.copy()) { }
			
			std::string toString() const {
				return makeString("enum constructor '%s' clashes with existing name",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		void AddEnumConstructorDecls(Context& context, const AST::Node<AST::TypeInstance>& typeInstanceNode) {
			for (const auto& constructorName: *(typeInstanceNode->constructors)) {
				const auto canonicalMethodName = CanonicalizeMethodName(constructorName);
				auto fullName = typeInstanceNode->fullName() + constructorName;
				
				const auto existingFunction = typeInstanceNode->findFunction(canonicalMethodName);
				if (existingFunction != nullptr) {
					context.issueDiag(EnumConstructorClashesWithExistingNameDiag(fullName),
					                  typeInstanceNode->constructors.location());
				}
				
				std::unique_ptr<AST::Function> function(new AST::Function());
				function->setParent(AST::GlobalStructure::TypeInstance(*typeInstanceNode));
				function->setFullName(std::move(fullName));
				function->setModuleScope(typeInstanceNode->moduleScope().copy());
				
				function->setDebugInfo(makeDefaultFunctionInfo(*typeInstanceNode, *function));
				
				function->setMethod(true);
				function->setIsStatic(true);
				
				const bool isVarArg = false;
				const bool isDynamicMethod = false;
				const bool isTemplatedMethod = false;
				auto noExceptPredicate = SEM::Predicate::True();
				const auto returnType = typeInstanceNode->selfType();
				
				AST::FunctionAttributes attributes(isVarArg, isDynamicMethod, isTemplatedMethod, std::move(noExceptPredicate));
				function->setType(AST::FunctionType(std::move(attributes), returnType, {}));
				typeInstanceNode->attachFunction(*function);
				
				// TODO: memory leak!
				(void) function.release();
			}
		}
		
		void AddNamespaceDataFunctionDecls(Context& context, const AST::Node<AST::NamespaceData>& astNamespaceDataNode,
		                                   const AST::ModuleScope& moduleScope) {
			for (auto& astFunctionNode: astNamespaceDataNode->functions) {
				AddNamespaceFunctionDecl(context, astFunctionNode, moduleScope);
			}
			
			for (const auto& astModuleScopeNode: astNamespaceDataNode->moduleScopes) {
				AddNamespaceDataFunctionDecls(context, astModuleScopeNode->data(),
				                              ConvertModuleScope(astModuleScopeNode));
			}
			
			for (const auto& astNamespaceNode: astNamespaceDataNode->namespaces) {
				auto& semChildNamespace = astNamespaceNode->nameSpace();
				
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::Namespace(semChildNamespace));
				AddNamespaceDataFunctionDecls(context, astNamespaceNode->data(), moduleScope);
			}
			
			for (const auto& typeInstanceNode: astNamespaceDataNode->typeInstances) {
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TypeInstance(*typeInstanceNode));
				for (auto& functionNode: *(typeInstanceNode->functionDecls)) {
					AddTypeInstanceFunctionDecl(context, functionNode, moduleScope);
				}
				
				if (typeInstanceNode->isEnum()) {
					AddEnumConstructorDecls(context, typeInstanceNode);
				}
			}
		}
		
		void AddFunctionDeclsPass(Context& context, const AST::NamespaceList& rootASTNamespaces) {
			for (const auto& astNamespaceNode: rootASTNamespaces) {
				AddNamespaceDataFunctionDecls(context, astNamespaceNode->data(),
				                              AST::ModuleScope::Internal());
			}
		}
		
	}
	
}
