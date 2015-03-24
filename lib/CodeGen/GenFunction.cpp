#include <vector>

#include <locic/Debug/SourceLocation.hpp>
#include <locic/Support/Name.hpp>
#include <locic/SEM.hpp>

#include <locic/CodeGen/ArgInfo.hpp>
#include <locic/CodeGen/ConstantGenerator.hpp>
#include <locic/CodeGen/Debug.hpp>
#include <locic/CodeGen/Destructor.hpp>
#include <locic/CodeGen/Function.hpp>
#include <locic/CodeGen/GenFunction.hpp>
#include <locic/CodeGen/GenStatement.hpp>
#include <locic/CodeGen/GenType.hpp>
#include <locic/CodeGen/GenVar.hpp>
#include <locic/CodeGen/Interface.hpp>
#include <locic/CodeGen/Mangling.hpp>
#include <locic/CodeGen/Memory.hpp>
#include <locic/CodeGen/MethodInfo.hpp>
#include <locic/CodeGen/Move.hpp>
#include <locic/CodeGen/Primitives.hpp>
#include <locic/CodeGen/ScopeExitActions.hpp>
#include <locic/CodeGen/Template.hpp>
#include <locic/CodeGen/TypeGenerator.hpp>
#include <locic/CodeGen/VirtualCall.hpp>
#include <locic/CodeGen/VTable.hpp>

namespace locic {

	namespace CodeGen {
	
		llvm::GlobalValue::LinkageTypes getFunctionLinkage(const SEM::TypeInstance* const typeInstance, const SEM::Function* const function) {
			if (function->isPrimitive()) {
				// Primitive function (generated by CodeGen).
				assert(function->isDeclaration());
				return llvm::Function::LinkOnceODRLinkage;
			}
			
			const auto& moduleScope = function->moduleScope();
			
			if (moduleScope.isInternal()) {
				// Internal functions.
				return llvm::Function::InternalLinkage;
			} else if (moduleScope.isImport() && function->isDefault() && !typeInstance->isClass()) {
				// Auto-generated type method (e.g. an implicitcopy for a struct).
				return llvm::Function::LinkOnceODRLinkage;
			} else {
				// Imported declarations or exported definitions.
				return llvm::Function::ExternalLinkage;
			}
		}
		
		llvm::GlobalValue::LinkageTypes getTypeInstanceLinkage(const SEM::TypeInstance* const typeInstance) {
			if (typeInstance->isPrimitive()) {
				// Primitive type.
				return llvm::Function::LinkOnceODRLinkage;
			}
			
			const auto& moduleScope = typeInstance->moduleScope();
			
			if (moduleScope.isInternal()) {
				// Internal definition.
				return llvm::Function::InternalLinkage;
			} else if (typeInstance->isClass()) {
				return llvm::Function::ExternalLinkage;
			} else {
				// Non-class type.
				return llvm::Function::LinkOnceODRLinkage;
			}
		}
		
		llvm::Function* genFunctionDecl(Module& module, const SEM::TypeInstance* typeInstance, SEM::Function* function) {
			if (function->isMethod()) {
				assert(typeInstance != nullptr);
			} else {
				assert(typeInstance == nullptr);
			}
			
			assert(!function->requiresPredicate().isFalse());
			
			{
				const auto iterator = module.getFunctionDeclMap().find(function);
				
				if (iterator != module.getFunctionDeclMap().end()) {
					return iterator->second;
				}
			}
			
			const auto mangledName =
				mangleModuleScope(module, function->moduleScope()) +
				(function->isMethod() ?
				 mangleMethodName(module, typeInstance, function->name().last()) :
				 mangleFunctionName(module, function->name()));
			
			{
				const auto iterator = module.getFunctionMap().find(mangledName);
				
				if (iterator != module.getFunctionMap().end()) {
					return iterator->second;
				}
			}
			
			const auto argInfo = getFunctionArgInfo(module, function->type());
			const auto linkage = getFunctionLinkage(typeInstance, function);
			const auto llvmFunction = createLLVMFunction(module, argInfo, linkage, mangledName);
			
			module.getFunctionMap().insert(std::make_pair(mangledName, llvmFunction));
			module.getFunctionDeclMap().insert(std::make_pair(function, llvmFunction));
			
			if (!canPassByValue(module, function->type()->getFunctionReturnType())) {
				// Class return values are allocated by the caller,
				// which passes a pointer to the callee. The caller
				// and callee must, for the sake of optimisation,
				// ensure that the following attributes hold...
				
				// Caller must ensure pointer is always valid.
				llvmFunction->addAttribute(1, llvm::Attribute::StructRet);
				
				// Caller must ensure pointer does not alias with
				// any other arguments.
				llvmFunction->addAttribute(1, llvm::Attribute::NoAlias);
				
				// Callee must not capture the pointer.
				llvmFunction->addAttribute(1, llvm::Attribute::NoCapture);
			}
			
			if (function->isPrimitive()) {
				// Generate primitive methods as needed.
				createPrimitiveMethod(module, typeInstance, function, *llvmFunction);
			}
			
			return llvmFunction;
		}
		
		namespace {
		
			void genFunctionCode(Function& functionGenerator, SEM::Function* function) {
				ScopeLifetime functionScopeLifetime(functionGenerator);
				
				// Parameters need to be copied to the stack, so that it's
				// possible to assign to them, take their address, etc.
				const auto& parameterVars = function->parameters();
				
				for (size_t i = 0; i < parameterVars.size(); i++) {
					const auto paramVar = parameterVars.at(i);
					
					// Get the variable's alloca.
					const auto stackObject = functionGenerator.getLocalVarMap().get(paramVar);
					
					// Store the argument into the stack alloca.
					genStoreVar(functionGenerator, functionGenerator.getArg(i), stackObject, paramVar);
					
					// Add this to the list of variables to be
					// destroyed at the end of the function.
					scheduleDestructorCall(functionGenerator, paramVar->type(), stackObject);
				}
				
				genScope(functionGenerator, function->scope());
			}
			
		}
		
		llvm::Function* genFunctionDef(Module& module, const SEM::TypeInstance* typeInstance, SEM::Function* function) {
			const auto llvmFunction = genFunctionDecl(module, typeInstance, function);
			
			// --- Generate function code.
			
			if (function->isPrimitive()) {
				// Already generated in genFunctionDecl().
				return llvmFunction;
			}
			
			if (function->isDeclaration()) {
				// A declaration, so it has no associated code.
				return llvmFunction;
			}
			
			const auto templatedObject =
				!function->templateVariables().empty() || typeInstance == nullptr ?
					TemplatedObject::Function(typeInstance, function) :
					TemplatedObject::TypeInstance(typeInstance);
			
			auto& templateBuilder = module.templateBuilder(templatedObject);
			const auto argInfo = getFunctionArgInfo(module, function->type());
			Function functionGenerator(module, *llvmFunction, argInfo, &templateBuilder);
			
			if (argInfo.hasTemplateGeneratorArgument() || (typeInstance != nullptr && typeInstance->isPrimitive())) {
				// Always inline if possible for templated functions
				// or methods of primitives.
				llvmFunction->addFnAttr(llvm::Attribute::AlwaysInline);
			}
			
			const auto debugSubprogram = genDebugFunctionInfo(module, function, llvmFunction);
			assert(debugSubprogram);
			functionGenerator.attachDebugInfo(*debugSubprogram);
			
			functionGenerator.setDebugPosition(function->debugInfo()->scopeLocation.range().start());
			
			// Generate allocas for parameters.
			for (const auto paramVar : function->parameters()) {
				genVarAlloca(functionGenerator, paramVar);
			}
			
			genFunctionCode(functionGenerator, function);
			
			if (!function->templateVariables().empty()) {
				(void) genTemplateIntermediateFunction(module, templatedObject, templateBuilder);
				
				// Update all instructions needing the bits required value
				// with the correct value (now it is known).
				templateBuilder.updateAllInstructions(module);
			}
			
			// Check the generated function is correct.
			functionGenerator.verify();
			
			return llvmFunction;
		}
		
		/**
		 * \brief Create template function stub.
		 * 
		 * This creates a function to be referenced when calling methods
		 * on a templated type. For example:
		 * 
		 * template <typename T : SomeRequirement>
		 * void f(T& value) {
		 *     value.method();
		 * }
		 * 
		 * It would be possible to generate the virtual call inline,
		 * but it's possible to reference the method itself. For example:
		 * 
		 * template <typename T : SomeRequirement>
		 * void f(T& value) {
		 *     auto methodValue = value.method;
		 *     methodValue();
		 * }
		 * 
		 * Hence this function needs to be created so it can be
		 * subsequently referenced.
		 */
		llvm::Function* genTemplateFunctionStub(Module& module, const SEM::TemplateVar* templateVar, const String& functionName,
				const SEM::Type* const functionType, llvm::DebugLoc debugLoc) {
			// --- Generate function declaration.
			const auto argInfo = getFunctionArgInfo(module, functionType);
			const auto llvmFunction = createLLVMFunction(module, argInfo, llvm::Function::InternalLinkage, module.getCString("templateFunctionStub"));
			
			// Always inline template function stubs.
			llvmFunction->addFnAttr(llvm::Attribute::AlwaysInline);
			
			const bool hasReturnVar = !canPassByValue(module, functionType->getFunctionReturnType());
			
			if (hasReturnVar) {
				// Class return values are allocated by the caller,
				// which passes a pointer to the callee. The caller
				// and callee must, for the sake of optimisation,
				// ensure that the following attributes hold...
				
				// Caller must ensure pointer is always valid.
				llvmFunction->addAttribute(1, llvm::Attribute::StructRet);
				
				// Caller must ensure pointer does not alias with
				// any other arguments.
				llvmFunction->addAttribute(1, llvm::Attribute::NoAlias);
				
				// Callee must not capture the pointer.
				llvmFunction->addAttribute(1, llvm::Attribute::NoCapture);
			}
			
			// --- Generate function code.
			
			Function functionGenerator(module, *llvmFunction, argInfo);
			functionGenerator.getBuilder().SetCurrentDebugLocation(debugLoc);
			
			const auto typeInfoValue = functionGenerator.getBuilder().CreateExtractValue(functionGenerator.getTemplateArgs(), { (unsigned) templateVar->index() });
			
			const auto i8PtrT = TypeGenerator(module).getI8PtrType();
			const auto contextPointer = argInfo.hasContextArgument() ? functionGenerator.getRawContextValue() : ConstantGenerator(module).getNull(i8PtrT);
			
			const auto methodHash = CreateMethodNameHash(functionName);
			const auto methodHashValue = ConstantGenerator(module).getI64(methodHash);
			
			const VirtualObjectComponents objectComponents(getTypeInfoComponents(functionGenerator, typeInfoValue), contextPointer);
			const VirtualMethodComponents methodComponents(objectComponents, methodHashValue);
			
			const auto argList = functionGenerator.getArgList();
			
			const auto hintResultValue = hasReturnVar ? functionGenerator.getReturnVar() : nullptr;
			const auto result = VirtualCall::generateCall(functionGenerator, functionType, methodComponents, argList, hintResultValue);
			
			if (hasReturnVar) {
				genMoveStore(functionGenerator, result, functionGenerator.getReturnVar(), functionType->getFunctionReturnType());
				functionGenerator.getBuilder().CreateRetVoid();
			} else if (result->getType()->isVoidTy()) {
				functionGenerator.getBuilder().CreateRetVoid();
			} else {
				functionGenerator.returnValue(result);
			}
			
			// Check the generated function is correct.
			functionGenerator.verify();
			
			return llvmFunction;
		}
		
	}
	
}

