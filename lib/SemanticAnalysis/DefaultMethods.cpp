#include <stdexcept>
#include <vector>

#include <locic/Constant.hpp>
#include <locic/Name.hpp>
#include <locic/SEM.hpp>

#include <locic/SemanticAnalysis/Context.hpp>
#include <locic/SemanticAnalysis/DefaultMethods.hpp>
#include <locic/SemanticAnalysis/Exception.hpp>
#include <locic/SemanticAnalysis/Lval.hpp>
#include <locic/SemanticAnalysis/Ref.hpp>
#include <locic/SemanticAnalysis/TypeProperties.hpp>

namespace locic {

	namespace SemanticAnalysis {
	
		SEM::Function* CreateDefaultConstructorDecl(Context& context, SEM::TypeInstance* typeInstance, const Name& name) {
			const auto semFunction = new SEM::Function(name, typeInstance->moduleScope());
			
			semFunction->setMethod(true);
			semFunction->setStaticMethod(true);
			
			const bool isVarArg = false;
			const bool isDynamicMethod = false;
			const bool isTemplatedMethod = !typeInstance->templateVariables().empty();
			
			// Default constructor only moves, and since moves never
			// throw the constructor never throws.
			const bool isNoExcept = true;
			
			const auto constructTypes = typeInstance->constructTypes();
			
			std::vector<SEM::Var*> argVars;
			for (const auto constructType: constructTypes) {
				const bool isLvalConst = false;
				const auto lvalType = makeValueLvalType(context, isLvalConst, constructType);
				argVars.push_back(SEM::Var::Basic(constructType, lvalType));
			}
			
			semFunction->setParameters(std::move(argVars));
			semFunction->setType(SEM::Type::Function(isVarArg, isDynamicMethod, isTemplatedMethod, isNoExcept, typeInstance->selfType(), constructTypes));
			return semFunction;
		}
		
		SEM::Function* CreateDefaultMoveDecl(Context& context, SEM::TypeInstance* typeInstance, const Name& name) {
			const auto semFunction = new SEM::Function(name, typeInstance->moduleScope());
			semFunction->typeRequirements() = typeInstance->typeRequirements();
			
			semFunction->setMethod(true);
			
			const bool isVarArg = false;
			const bool isDynamicMethod = true;
			const bool isTemplatedMethod = !typeInstance->templateVariables().empty();
			
			// Move never throws.
			const bool isNoExcept = true;
			
			const auto voidType = getBuiltInType(context.scopeStack(), "void_t");
			const auto ptrTypeInstance = getBuiltInType(context.scopeStack(), "__ptr")->getObjectType();
			const auto voidPtrType = SEM::Type::Object(ptrTypeInstance, { voidType });
			
			const auto sizeType = getBuiltInType(context.scopeStack(), "size_t");
			
			const auto argTypes = std::vector<const SEM::Type*>{ voidPtrType, sizeType };
			
			std::vector<SEM::Var*> argVars;
			
			{
				const bool isLvalConst = false;
				const auto lvalType = makeValueLvalType(context, isLvalConst, voidPtrType);
				argVars.push_back(SEM::Var::Basic(voidPtrType, lvalType));
			}
			
			{
				const bool isLvalConst = false;
				const auto lvalType = makeValueLvalType(context, isLvalConst, sizeType);
				argVars.push_back(SEM::Var::Basic(sizeType, lvalType));
			}
			
			semFunction->setParameters(std::move(argVars));
			semFunction->setType(SEM::Type::Function(isVarArg, isDynamicMethod, isTemplatedMethod, isNoExcept, voidType, argTypes));
			return semFunction;
		}
		
		bool HasNoExceptImplicitCopy(Context& context, SEM::TypeInstance* typeInstance) {
			if (typeInstance->isUnionDatatype()) {
				for (auto variantTypeInstance: typeInstance->variants()) {
					if (!HasNoExceptImplicitCopy(context, variantTypeInstance)) {
						return false;
					}
				}
				return true;
			} else {
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TypeInstance(typeInstance));
				for (auto var: typeInstance->variables()) {
					if (!supportsNoExceptImplicitCopy(context, var->constructType())) {
						return false;
					}
				}
				return true;
			}
		}
		
		bool HasNoExceptExplicitCopy(Context& context, SEM::TypeInstance* typeInstance) {
			if (typeInstance->isUnionDatatype()) {
				for (auto variantTypeInstance: typeInstance->variants()) {
					if (!HasNoExceptExplicitCopy(context, variantTypeInstance)) {
						return false;
					}
				}
				return true;
			} else {
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TypeInstance(typeInstance));
				for (auto var: typeInstance->variables()) {
					if (!supportsNoExceptExplicitCopy(context, var->constructType())) {
						return false;
					}
				}
				return true;
			}
		}
		
		SEM::Function* CreateDefaultImplicitCopyDecl(Context& context, SEM::TypeInstance* typeInstance, const Name& name) {
			const auto semFunction = new SEM::Function(name, typeInstance->moduleScope());
			
			semFunction->setMethod(true);
			semFunction->setConstMethod(true);
			
			const bool isVarArg = false;
			const bool isDynamicMethod = true;
			const bool isTemplatedMethod = !typeInstance->templateVariables().empty();
			
			// Default copy method may throw since it
			// may call child copy methods that throw.
			const bool isNoExcept = HasNoExceptImplicitCopy(context, typeInstance);
			
			semFunction->setType(SEM::Type::Function(isVarArg, isDynamicMethod, isTemplatedMethod, isNoExcept, typeInstance->selfType(), {}));
			return semFunction;
		}
		
		SEM::Function* CreateDefaultExplicitCopyDecl(Context& context, SEM::TypeInstance* typeInstance, const Name& name) {
			const auto semFunction = new SEM::Function(name, typeInstance->moduleScope());
			
			semFunction->setMethod(true);
			semFunction->setConstMethod(true);
			
			const bool isVarArg = false;
			const bool isDynamicMethod = true;
			const bool isTemplatedMethod = !typeInstance->templateVariables().empty();
			
			// Default copy method may throw since it
			// may call child copy methods that throw.
			const bool isNoExcept = HasNoExceptExplicitCopy(context, typeInstance);
			
			semFunction->setType(SEM::Type::Function(isVarArg, isDynamicMethod, isTemplatedMethod, isNoExcept, typeInstance->selfType(), {}));
			return semFunction;
		}
		
		SEM::Function* CreateDefaultCompareDecl(Context& context, SEM::TypeInstance* typeInstance, const Name& name) {
			const auto semFunction = new SEM::Function(name, typeInstance->moduleScope());
			
			semFunction->setMethod(true);
			semFunction->setConstMethod(true);
			
			const bool isVarArg = false;
			const bool isDynamicMethod =true;
			const bool isTemplatedMethod = !typeInstance->templateVariables().empty();
			
			// Default compare method may throw since it
			// may call child compare methods that throw.
			const bool isNoExcept = false;
			
			const auto selfType = typeInstance->selfType();
			const auto argType = createReferenceType(context, selfType->createConstType());
			const auto compareResultType = getBuiltInType(context.scopeStack(), "compare_result_t");
			semFunction->setType(SEM::Type::Function(isVarArg, isDynamicMethod, isTemplatedMethod, isNoExcept, compareResultType, { argType }));
			semFunction->setParameters({ SEM::Var::Basic(argType, argType) });
			return semFunction;
		}
		
		SEM::Function* CreateDefaultMethodDecl(Context& context, SEM::TypeInstance* typeInstance, bool isStatic, const Name& name, const Debug::SourceLocation& location) {
			assert(!typeInstance->isClassDecl());
			assert(!typeInstance->isInterface());
			
			const auto canonicalName = CanonicalizeMethodName(name.last());
			if (canonicalName == "create") {
				if (!isStatic) {
					throw ErrorException(makeString("Default method '%s' must be static at position %s.",
						name.toString().c_str(), location.toString().c_str()));
				}
				
				return CreateDefaultConstructorDecl(context, typeInstance, name);
			} else if (canonicalName == "implicitcopy") {
				if (isStatic) {
					throw ErrorException(makeString("Default method '%s' must be non-static at position %s.",
						name.toString().c_str(), location.toString().c_str()));
				}
				
				return CreateDefaultImplicitCopyDecl(context, typeInstance, name);
			} else if (canonicalName == "copy") {
				if (isStatic) {
					throw ErrorException(makeString("Default method '%s' must be non-static at position %s.",
						name.toString().c_str(), location.toString().c_str()));
				}
				
				return CreateDefaultExplicitCopyDecl(context, typeInstance, name);
			} else if (canonicalName == "compare") {
				if (isStatic) {
					throw ErrorException(makeString("Default method '%s' must be non-static at position %s.",
						name.toString().c_str(), location.toString().c_str()));
				}
				
				return CreateDefaultCompareDecl(context, typeInstance, name);
			}
			
			throw ErrorException(makeString("Unknown default method '%s' at position %s.",
				name.toString().c_str(), location.toString().c_str()));
		}
		
		bool HasDefaultConstructor(Context&, SEM::TypeInstance* const typeInstance) {
			return typeInstance->isDatatype() || typeInstance->isStruct() || typeInstance->isException();
		}
		
		bool HasDefaultMove(Context&, SEM::TypeInstance* const typeInstance) {
			assert(typeInstance->isClass() ||
				typeInstance->isDatatype() ||
				typeInstance->isUnionDatatype() ||
				typeInstance->isException() ||
				typeInstance->isStruct());
			
			// There's only a default move method if the user
			// hasn't specified a custom move method.
			return typeInstance->functions().find("__moveto") == typeInstance->functions().end();
		}
		
		bool HasDefaultImplicitCopy(Context& context, SEM::TypeInstance* const typeInstance) {
			assert(typeInstance->isClassDef() ||
				typeInstance->isDatatype() ||
				typeInstance->isUnionDatatype() ||
				typeInstance->isException() ||
				typeInstance->isStruct());
			
			if (typeInstance->isUnionDatatype()) {
				for (auto variantTypeInstance: typeInstance->variants()) {
					if (!HasDefaultImplicitCopy(context, variantTypeInstance)) {
						return false;
					}
				}
				return true;
			} else {
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TypeInstance(typeInstance));
				for (auto var: typeInstance->variables()) {
					if (!supportsImplicitCopy(context, var->constructType())) {
						return false;
					}
				}
				return true;
			}
		}
		
		bool HasDefaultExplicitCopy(Context& context, SEM::TypeInstance* const typeInstance) {
			assert(typeInstance->isClassDef() ||
				typeInstance->isDatatype() ||
				typeInstance->isUnionDatatype() ||
				typeInstance->isException() ||
				typeInstance->isStruct());
			
			if (typeInstance->isUnionDatatype()) {
				for (auto variantTypeInstance: typeInstance->variants()) {
					if (!HasDefaultExplicitCopy(context, variantTypeInstance)) {
						return false;
					}
				}
				return true;
			} else {
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TypeInstance(typeInstance));
				for (auto var: typeInstance->variables()) {
					if (!supportsExplicitCopy(context, var->constructType())) {
						return false;
					}
				}
				return true;
			}
		}
		
		bool HasDefaultCompare(Context& context, SEM::TypeInstance* const typeInstance) {
			assert(typeInstance->isClassDef() ||
				typeInstance->isDatatype() ||
				typeInstance->isUnionDatatype() ||
				typeInstance->isException() ||
				typeInstance->isStruct());
			
			if (typeInstance->isUnionDatatype()) {
				for (auto variantTypeInstance: typeInstance->variants()) {
					if (!supportsCompare(context, variantTypeInstance->selfType())) {
						return false;
					}
				}
				return true;
			} else {
				PushScopeElement pushScopeElement(context.scopeStack(), ScopeElement::TypeInstance(typeInstance));
				for (auto var: typeInstance->variables()) {
					if (!supportsCompare(context, var->constructType())) {
						return false;
					}
				}
				return true;
			}
		}
		
		void CreateDefaultConstructor(Context& context, SEM::TypeInstance* typeInstance, SEM::Function* function, const Debug::SourceLocation& location) {
			auto functionScope = SEM::Scope::Create();
			
			assert(!typeInstance->isUnionDatatype());
			
			std::vector<SEM::Value*> constructValues;
			for (const auto argVar: function->parameters()) {
				const auto argVarValue = createLocalVarRef(context, argVar);
				constructValues.push_back(CallValue(context, GetSpecialMethod(context, argVarValue, "move", location), {}, location));
			}
			
			const auto internalConstructedValue = SEM::Value::InternalConstruct(typeInstance, constructValues);
			functionScope->statements().push_back(SEM::Statement::Return(internalConstructedValue));
			
			function->setScope(std::move(functionScope));
		}
		
		void CreateDefaultMove(Context& context, SEM::TypeInstance* typeInstance, SEM::Function* function, const Debug::SourceLocation& location) {
			const auto selfValue = createSelfRef(context, typeInstance->selfType());
			
			auto functionScope = SEM::Scope::Create();
			
			const auto ptrVar = function->parameters().at(0);
			const auto ptrValue = createLocalVarRef(context, ptrVar);
			
			const auto positionVar = function->parameters().at(1);
			const auto positionValue = createLocalVarRef(context, positionVar);
			
			const auto ubyteType = getBuiltInType(context.scopeStack(), "ubyte_t");
			const auto sizeType = getBuiltInType(context.scopeStack(), "size_t");
			
			if (typeInstance->isUnionDatatype()) {
				{
					// Move the tag.
					const auto tagValue = SEM::Value::UnionTag(derefAll(selfValue), ubyteType);
					const std::vector<SEM::Value*> moveArgs = { ptrValue, positionValue };
					const auto moveResult = CallValue(context, GetSpecialMethod(context, tagValue, "__moveto", location), moveArgs, location);
					functionScope->statements().push_back(SEM::Statement::ValueStmt(moveResult));
				}
				
				// Calculate the position of the union data so that this
				// can be passed to the move methods of the union types.
				const auto unionDataOffset = SEM::Value::UnionDataOffset(typeInstance, sizeType);
				const auto unionDataPosition = CallValue(context, GetMethod(context, positionValue, "add", location), { unionDataOffset }, location);
				
				std::vector<SEM::SwitchCase*> switchCases;
				for (const auto variantTypeInstance: typeInstance->variants()) {
					const auto variantType = variantTypeInstance->selfType();
					const auto caseVar = SEM::Var::Basic(variantType, variantType);
					const auto caseVarValue = createLocalVarRef(context, caseVar);
					
					auto caseScope = SEM::Scope::Create();
					const std::vector<SEM::Value*> moveArgs = { ptrValue, unionDataPosition };
					const auto moveResult = CallValue(context, GetSpecialMethod(context, caseVarValue, "__moveto", location), moveArgs, location);
					caseScope->statements().push_back(SEM::Statement::ValueStmt(moveResult));
					caseScope->statements().push_back(SEM::Statement::ReturnVoid());
					
					switchCases.push_back(new SEM::SwitchCase(caseVar, std::move(caseScope)));
				}
				functionScope->statements().push_back(SEM::Statement::Switch(derefAll(selfValue), switchCases, nullptr));
			} else {
				for (size_t i = 0; i < typeInstance->variables().size(); i++) {
					const auto& memberVar = typeInstance->variables().at(i);
					const auto memberOffset = SEM::Value::MemberOffset(typeInstance, i, sizeType);
					const auto memberPosition = CallValue(context, GetMethod(context, positionValue, "add", location), { memberOffset }, location);
					
					const std::vector<SEM::Value*> moveArgs = { ptrValue, memberPosition };
					const auto selfMember = createMemberVarRef(context, selfValue, memberVar);
					const auto moveResult = CallValue(context, GetSpecialMethod(context, selfMember, "__moveto", location), moveArgs, location);
					
					functionScope->statements().push_back(SEM::Statement::ValueStmt(moveResult));
				}
				
				functionScope->statements().push_back(SEM::Statement::ReturnVoid());
			}
			
			function->setScope(std::move(functionScope));
		}
		
		void CreateDefaultCopy(Context& context, const std::string& functionName, SEM::TypeInstance* typeInstance, SEM::Function* function, const Debug::SourceLocation& location) {
			const auto selfType = typeInstance->selfType();
			const auto selfValue = createSelfRef(context, typeInstance->selfType());
			
			auto functionScope = SEM::Scope::Create();
			
			if (typeInstance->isUnionDatatype()) {
				std::vector<SEM::SwitchCase*> switchCases;
				for (const auto variantTypeInstance: typeInstance->variants()) {
					const auto variantType = variantTypeInstance->selfType();
					const auto caseVar = SEM::Var::Basic(variantType, variantType);
					const auto caseVarValue = createLocalVarRef(context, caseVar);
					
					auto caseScope = SEM::Scope::Create();
					const auto copyResult = CallValue(context, GetSpecialMethod(context, caseVarValue, functionName, location), {}, location);
					const auto copyResultCast = SEM::Value::Cast(selfType, copyResult);
					caseScope->statements().push_back(SEM::Statement::Return(copyResultCast));
					
					switchCases.push_back(new SEM::SwitchCase(caseVar, std::move(caseScope)));
				}
				functionScope->statements().push_back(SEM::Statement::Switch(derefAll(selfValue), switchCases, nullptr));
			} else {
				std::vector<SEM::Value*> copyValues;
				
				for (const auto memberVar: typeInstance->variables()) {
					const auto selfMember = dissolveLval(context, createMemberVarRef(context, selfValue, memberVar), location);
					const auto copyResult = CallValue(context, GetSpecialMethod(context, selfMember, functionName, location), {}, location);
					copyValues.push_back(copyResult);
				}
				
				const auto internalConstructedValue = SEM::Value::InternalConstruct(typeInstance, copyValues);
				functionScope->statements().push_back(SEM::Statement::Return(internalConstructedValue));
			}
			
			function->setScope(std::move(functionScope));
		}
		
		void CreateDefaultImplicitCopy(Context& context, SEM::TypeInstance* typeInstance, SEM::Function* function, const Debug::SourceLocation& location) {
			CreateDefaultCopy(context, "implicitcopy", typeInstance, function, location);
		}
		
		void CreateDefaultExplicitCopy(Context& context, SEM::TypeInstance* typeInstance, SEM::Function* function, const Debug::SourceLocation& location) {
			CreateDefaultCopy(context, "copy", typeInstance, function, location);
		}
		
		void CreateDefaultCompare(Context& context, SEM::TypeInstance* typeInstance, SEM::Function* function, const Debug::SourceLocation& location) {
			const auto selfValue = createSelfRef(context, typeInstance->selfType());
			
			const auto compareResultType = getBuiltInType(context.scopeStack(), "compare_result_t");
			
			const auto operandVar = function->parameters().at(0);
			const auto operandValue = createLocalVarRef(context, operandVar);
			
			auto functionScope = SEM::Scope::Create();
			
			if (typeInstance->isUnionDatatype()) {
				std::vector<SEM::SwitchCase*> switchCases;
				for (size_t i = 0; i < typeInstance->variants().size(); i++) {
					const auto variantTypeInstance = typeInstance->variants().at(i);
					const auto variantType = variantTypeInstance->selfType();
					const auto caseVar = SEM::Var::Basic(variantType, variantType);
					const auto caseVarValue = createLocalVarRef(context, caseVar);
					
					auto caseScope = SEM::Scope::Create();
					
					std::vector<SEM::SwitchCase*> subSwitchCases;
					for (size_t j = 0; j < typeInstance->variants().size(); j++) {
						const auto subVariantTypeInstance = typeInstance->variants().at(j);
						const auto subVariantType = subVariantTypeInstance->selfType();
						const auto subCaseVar = SEM::Var::Basic(subVariantType, subVariantType);
						const auto subCaseVarValue = createLocalVarRef(context, subCaseVar);
						
						auto subCaseScope = SEM::Scope::Create();
						const auto minusOneConstant = SEM::Value::Constant(Constant::Integer(-1), compareResultType);
						const auto plusOneConstant = SEM::Value::Constant(Constant::Integer(1), compareResultType);
						if (i < j) {
							subCaseScope->statements().push_back(SEM::Statement::Return(minusOneConstant));
						} else if (i > j) {
							subCaseScope->statements().push_back(SEM::Statement::Return(plusOneConstant));
						} else {
							const auto compareResult = CallValue(context, GetMethod(context, caseVarValue, "compare", location), { subCaseVarValue }, location);
							subCaseScope->statements().push_back(SEM::Statement::Return(compareResult));
						}
						
						subSwitchCases.push_back(new SEM::SwitchCase(subCaseVar, std::move(subCaseScope)));
					}
					
					caseScope->statements().push_back(SEM::Statement::Switch(derefAll(operandValue), subSwitchCases, nullptr));
					
					switchCases.push_back(new SEM::SwitchCase(caseVar, std::move(caseScope)));
				}
				
				functionScope->statements().push_back(SEM::Statement::Switch(derefAll(selfValue), switchCases, nullptr));
			} else {
				auto currentScope = functionScope.get();
				
				for (const auto memberVar: typeInstance->variables()) {
					const auto selfMember = createMemberVarRef(context, selfValue, memberVar);
					const auto operandMember = createMemberVarRef(context, operandValue, memberVar);
					
					const auto compareResult = CallValue(context, GetMethod(context, selfMember, "compare", location), { operandMember }, location);
					const auto isEqual = CallValue(context, GetMethod(context, compareResult, "isEqual", location), {}, location);
					
					auto ifTrueScope = SEM::Scope::Create();
					auto ifFalseScope = SEM::Scope::Create();
					
					const auto nextScope = ifTrueScope.get();
					
					ifFalseScope->statements().push_back(SEM::Statement::Return(compareResult));
					
					const auto ifStatement = SEM::Statement::If({ new SEM::IfClause(isEqual, std::move(ifTrueScope)) }, std::move(ifFalseScope));
					currentScope->statements().push_back(ifStatement);
					
					currentScope = nextScope;
				}
				
				const auto zeroConstant = SEM::Value::Constant(Constant::Integer(0), compareResultType);
				currentScope->statements().push_back(SEM::Statement::Return(zeroConstant));
			}
			
			function->setScope(std::move(functionScope));
		}
		
		void CreateDefaultMethod(Context& context, SEM::TypeInstance* typeInstance, SEM::Function* function, const Debug::SourceLocation& location) {
			assert(!typeInstance->isClassDecl());
			assert(!typeInstance->isInterface());
			assert(function->isDeclaration());
			
			const auto& name = function->name();
			const auto canonicalName = CanonicalizeMethodName(name.last());
			if (canonicalName == "__moveto") {
				CreateDefaultMove(context, typeInstance, function, location);
			} else if (canonicalName == "create") {
				assert(!typeInstance->isException());
				CreateDefaultConstructor(context, typeInstance, function, location);
			} else if (canonicalName == "implicitcopy") {
				if (!HasDefaultImplicitCopy(context, typeInstance)) {
					throw ErrorException(makeString("Default method '%s' cannot be generated because member types do not support it, at position %s.",
						name.toString().c_str(), location.toString().c_str()));
				}
				
				CreateDefaultImplicitCopy(context, typeInstance, function, location);
			} else if (canonicalName == "copy") {
				if (!HasDefaultExplicitCopy(context, typeInstance)) {
					throw ErrorException(makeString("Default method '%s' cannot be generated because member types do not support it, at position %s.",
						name.toString().c_str(), location.toString().c_str()));
				}
				
				CreateDefaultExplicitCopy(context, typeInstance, function, location);
			} else if (canonicalName == "compare") {
				if (!HasDefaultCompare(context, typeInstance)) {
					throw ErrorException(makeString("Default method '%s' cannot be generated because member types do not support it, at position %s.",
						name.toString().c_str(), location.toString().c_str()));
				}
				
				CreateDefaultCompare(context, typeInstance, function, location);
			} else {
				throw std::runtime_error(makeString("Unknown default method '%s'.", name.toString().c_str()));
			}
		}
		
	}
	
}

