#include <locic/AST.hpp>
#include <locic/SemanticAnalysis/AliasTypeResolver.hpp>
#include <locic/SemanticAnalysis/Context.hpp>
#include <locic/SemanticAnalysis/ConvertModuleScope.hpp>
#include <locic/SemanticAnalysis/Exception.hpp>
#include <locic/SemanticAnalysis/ScopeStack.hpp>
#include <locic/Support/PrimitiveID.hpp>
#include <locic/Support/SharedMaps.hpp>

namespace locic {
	
	namespace SemanticAnalysis {
		
		Debug::TemplateVarInfo makeTemplateVarInfo(const AST::Node<AST::TemplateTypeVar>& astTemplateVarNode) {
			Debug::TemplateVarInfo templateVarInfo;
			templateVarInfo.declLocation = astTemplateVarNode.location();
			
			// TODO
			templateVarInfo.scopeLocation = Debug::SourceLocation::Null();
			return templateVarInfo;
		}
		
		SEM::TypeInstance::Kind ConvertTypeInstanceKind(AST::TypeInstance::Kind kind) {
			switch (kind) {
				case AST::TypeInstance::PRIMITIVE:
					return SEM::TypeInstance::PRIMITIVE;
				case AST::TypeInstance::ENUM:
					return SEM::TypeInstance::ENUM;
				case AST::TypeInstance::STRUCT:
					return SEM::TypeInstance::STRUCT;
				case AST::TypeInstance::OPAQUE_STRUCT:
					return SEM::TypeInstance::OPAQUE_STRUCT;
				case AST::TypeInstance::UNION:
					return SEM::TypeInstance::UNION;
				case AST::TypeInstance::CLASSDECL:
					return SEM::TypeInstance::CLASSDECL;
				case AST::TypeInstance::CLASSDEF:
					return SEM::TypeInstance::CLASSDEF;
				case AST::TypeInstance::DATATYPE:
					return SEM::TypeInstance::DATATYPE;
				case AST::TypeInstance::UNION_DATATYPE:
					return SEM::TypeInstance::UNION_DATATYPE;
				case AST::TypeInstance::INTERFACE:
					return SEM::TypeInstance::INTERFACE;
				case AST::TypeInstance::EXCEPTION:
					return SEM::TypeInstance::EXCEPTION;
			}
			
			std::terminate();
		}
		
		namespace {
			
			class ShadowsTemplateParameterDiag: public Error {
			public:
				ShadowsTemplateParameterDiag(String name)
				: name_(std::move(name)) { }
				
				std::string toString() const {
					return makeString("declaration of '%s' shadows template parameter",
					                  name_.c_str());
				}
				
			private:
				String name_;
				
			};
			
		}
		
		class DefinitionRequiredForInternalClassDiag: public Error {
		public:
			DefinitionRequiredForInternalClassDiag(Name name)
			: name_(std::move(name)) { }
			
			std::string toString() const {
				return makeString("definition required for internal class '%s'",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		class CannotDefineImportedClassDiag: public Error {
		public:
			CannotDefineImportedClassDiag(Name name)
			: name_(std::move(name)) { }
			
			std::string toString() const {
				return makeString("cannot define imported class '%s'",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		class DefinitionRequiredForExportedClassDiag: public Error {
		public:
			DefinitionRequiredForExportedClassDiag(Name name)
			: name_(std::move(name)) { }
			
			std::string toString() const {
				return makeString("definition required for exported class '%s'",
				                  name_.toString(/*addPrefix=*/false).c_str());
			}
			
		private:
			Name name_;
			
		};
		
		SEM::TypeInstance* AddTypeInstance(Context& context, const AST::Node<AST::TypeInstance>& astTypeInstanceNode, const SEM::ModuleScope& moduleScope) {
			auto& parentNamespace = context.scopeStack().back().nameSpace();
			
			const auto& typeInstanceName = astTypeInstanceNode->name;
			
			const Name fullTypeName = parentNamespace.name() + typeInstanceName;
			
			// Check if there's anything with the same name.
			const auto iterator = parentNamespace.items().find(typeInstanceName);
			if (iterator != parentNamespace.items().end()) {
				const auto& existingTypeInstance = iterator->second.typeInstance();
				const auto& debugInfo = *(existingTypeInstance.debugInfo());
				throw ErrorException(makeString("Type instance name '%s', at position %s, clashes with existing name, at position %s.",
					fullTypeName.toString().c_str(), astTypeInstanceNode.location().toString().c_str(),
					debugInfo.location.toString().c_str()));
			}
			
			const auto typeInstanceKind = ConvertTypeInstanceKind(astTypeInstanceNode->kind);
			
			// Create a placeholder type instance.
			std::unique_ptr<SEM::TypeInstance> semTypeInstance(new SEM::TypeInstance(context.semContext(),
			                                                                         SEM::GlobalStructure::Namespace(parentNamespace),
			                                                                         fullTypeName.copy(),
			                                                                         typeInstanceKind,
			                                                                         moduleScope.copy()));
			
			if (semTypeInstance->isPrimitive()) {
				semTypeInstance->setPrimitiveID(context.sharedMaps().primitiveIDMap().getPrimitiveID(typeInstanceName));
			}
			
			switch (moduleScope.kind()) {
				case SEM::ModuleScope::INTERNAL: {
					if (semTypeInstance->isClassDecl()) {
						context.issueDiag(DefinitionRequiredForInternalClassDiag(fullTypeName.copy()),
						                  astTypeInstanceNode.location());
					}
					break;
				}
				case SEM::ModuleScope::IMPORT: {
					if (semTypeInstance->isClassDef()) {
						context.issueDiag(CannotDefineImportedClassDiag(fullTypeName.copy()),
						                  astTypeInstanceNode.location());
					}
					break;
				}
				case SEM::ModuleScope::EXPORT: {
					if (semTypeInstance->isClassDecl()) {
						context.issueDiag(DefinitionRequiredForExportedClassDiag(fullTypeName.copy()),
						                  astTypeInstanceNode.location());
					}
					break;
				}
			}
			
			semTypeInstance->setDebugInfo(Debug::TypeInstanceInfo(astTypeInstanceNode.location()));
			
			// Add template variables.
			size_t templateVarIndex = 0;
			for (const auto& astTemplateVarNode: *(astTypeInstanceNode->templateVariables)) {
				const auto& templateVarName = astTemplateVarNode->name;
				// TODO!
				const bool isVirtual = (typeInstanceName == "ref_t");
				const auto semTemplateVar =
					new SEM::TemplateVar(context.semContext(),
						fullTypeName + templateVarName,
						templateVarIndex++, isVirtual);
				
				const auto templateVarIterator = semTypeInstance->namedTemplateVariables().find(templateVarName);
				if (templateVarIterator == semTypeInstance->namedTemplateVariables().end()) {
					semTypeInstance->namedTemplateVariables().insert(std::make_pair(templateVarName, semTemplateVar));
				} else {
					context.issueDiag(ShadowsTemplateParameterDiag(templateVarName),
					                  astTemplateVarNode.location());
				}
				
				semTemplateVar->setDebugInfo(makeTemplateVarInfo(astTemplateVarNode));
				semTypeInstance->templateVariables().push_back(semTemplateVar);
			}
			
			if (semTypeInstance->isUnionDatatype()) {
				for (auto& astVariantNode: *(astTypeInstanceNode->variants)) {
					const auto variantTypeInstance = AddTypeInstance(context, astVariantNode, moduleScope);
					variantTypeInstance->setParentTypeInstance(semTypeInstance.get());
					variantTypeInstance->templateVariables() = semTypeInstance->templateVariables().copy();
					variantTypeInstance->namedTemplateVariables() = semTypeInstance->namedTemplateVariables().copy();
					semTypeInstance->variants().push_back(variantTypeInstance);
				}
			}
			
			if (!astTypeInstanceNode->noTagSet.isNull()) {
				SEM::TemplateVarArray noTagSet;
				
				for (const auto& astNoTagName: *(astTypeInstanceNode->noTagSet)) {
					const auto templateVarIterator = semTypeInstance->namedTemplateVariables().find(astNoTagName);
					if (templateVarIterator == semTypeInstance->namedTemplateVariables().end()) {
						throw ErrorException(makeString("Can't find template variable '%s' in notag() set in type '%s', at location %s.",
										astNoTagName.c_str(), fullTypeName.toString().c_str(),
										astTypeInstanceNode->noTagSet.location().toString().c_str()));
					}
					
					noTagSet.push_back(templateVarIterator->second);
				}
				
				semTypeInstance->setNoTagSet(std::move(noTagSet));
			}
			
			const auto typeInstancePtr = semTypeInstance.get();
			
			parentNamespace.items().insert(std::make_pair(typeInstanceName, SEM::NamespaceItem::TypeInstance(std::move(semTypeInstance))));
			
			return typeInstancePtr;
		}
		
		class CannotNestModuleScopesDiag: public Error {
		public:
			CannotNestModuleScopesDiag() { }
			
			std::string toString() const {
				return "cannot nest module scopes";
			}
			
		};
		
		void AddNamespaceData(Context& context, const AST::Node<AST::NamespaceData>& astNamespaceDataNode, const SEM::ModuleScope& moduleScope) {
			auto& semNamespace = context.scopeStack().back().nameSpace();
			
			for (const auto& astChildNamespaceNode: astNamespaceDataNode->namespaces) {
				const auto& childNamespaceName = astChildNamespaceNode->name();
				
				SEM::Namespace* semChildNamespace = nullptr;
				
				const auto iterator = semNamespace.items().find(childNamespaceName);
				if (iterator == semNamespace.items().end()) {
					std::unique_ptr<SEM::Namespace> childNamespace(new SEM::Namespace(semNamespace.name() + childNamespaceName,
					                                                                  SEM::GlobalStructure::Namespace(semNamespace)));
					semChildNamespace = childNamespace.get();
					semNamespace.items().insert(std::make_pair(childNamespaceName, SEM::NamespaceItem::Namespace(std::move(childNamespace))));
				} else {
					semChildNamespace = &(iterator->second.nameSpace());
				}
				
				astChildNamespaceNode->setNamespace(*semChildNamespace);
				
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::Namespace(*semChildNamespace));
				AddNamespaceData(context, astChildNamespaceNode->data(), moduleScope);
			}
			
			for (const auto& astModuleScopeNode: astNamespaceDataNode->moduleScopes) {
				if (!moduleScope.isInternal()) {
					context.issueDiag(CannotNestModuleScopesDiag(),
					                  astModuleScopeNode.location());
				}
				AddNamespaceData(context, astModuleScopeNode->data, ConvertModuleScope(astModuleScopeNode));
			}
			
			for (const auto& astAliasNode: astNamespaceDataNode->aliases) {
				const auto& aliasName = astAliasNode->name();
				const auto fullTypeName = semNamespace.name() + aliasName;
				const auto iterator = semNamespace.items().find(aliasName);
				if (iterator != semNamespace.items().end()) {
					throw ErrorException(makeString("Type alias name '%s' clashes with existing name, at position %s.",
						fullTypeName.toString().c_str(), astAliasNode.location().toString().c_str()));
				}
				
				std::unique_ptr<SEM::Alias> semAlias(new SEM::Alias(context.semContext(),
				                                                    SEM::GlobalStructure::Namespace(semNamespace),
				                                                    fullTypeName.copy(),
				                                                    astAliasNode));
				
				// Add template variables.
				size_t templateVarIndex = 0;
				for (const auto& astTemplateVarNode: *(astAliasNode->templateVariables())) {
					const auto& templateVarName = astTemplateVarNode->name;
					
					// TODO!
					const bool isVirtual = false;
					const auto semTemplateVar =
						new SEM::TemplateVar(context.semContext(),
							fullTypeName + templateVarName,
							templateVarIndex++, isVirtual);
					
					const auto templateVarIterator = semAlias->namedTemplateVariables().find(templateVarName);
					if (templateVarIterator != semAlias->namedTemplateVariables().end()) {
						throw TemplateVariableClashException(fullTypeName.copy(), templateVarName);
					}
					
					semAlias->templateVariables().push_back(semTemplateVar);
					semAlias->namedTemplateVariables().insert(std::make_pair(templateVarName, semTemplateVar));
				}
				
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::Alias(*semAlias));
				context.aliasTypeResolver().addAlias(*semAlias, astAliasNode->value(), context.scopeStack().copy());
				
				astAliasNode->setAlias(*semAlias);
				
				semNamespace.items().insert(std::make_pair(aliasName, SEM::NamespaceItem::Alias(std::move(semAlias))));
			}
			
			for (const auto& astTypeInstanceNode: astNamespaceDataNode->typeInstances) {
				(void) AddTypeInstance(context, astTypeInstanceNode, moduleScope);
			}
		}
		
		// Get all namespaces and type names, and build initial type instance structures.
		void AddGlobalStructuresPass(Context& context, const AST::NamespaceList& rootASTNamespaces) {
			for (const auto& astNamespaceNode: rootASTNamespaces) {
				AddNamespaceData(context, astNamespaceNode->data(), SEM::ModuleScope::Internal());
			}
		}
		
	}
	
}
