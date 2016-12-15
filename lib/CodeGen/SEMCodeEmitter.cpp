#include <locic/CodeGen/ArgInfo.hpp>
#include <locic/CodeGen/DefaultMethodEmitter.hpp>
#include <locic/CodeGen/Destructor.hpp>
#include <locic/CodeGen/Function.hpp>
#include <locic/CodeGen/GenVar.hpp>
#include <locic/CodeGen/InternalContext.hpp>
#include <locic/CodeGen/IREmitter.hpp>
#include <locic/CodeGen/Memory.hpp>
#include <locic/CodeGen/Module.hpp>
#include <locic/CodeGen/Move.hpp>
#include <locic/CodeGen/PrimitiveFunctionEmitter.hpp>
#include <locic/CodeGen/ScopeEmitter.hpp>
#include <locic/CodeGen/ScopeExitActions.hpp>
#include <locic/CodeGen/SEMCodeEmitter.hpp>
#include <locic/CodeGen/Support.hpp>

#include <locic/SEM/Function.hpp>
#include <locic/SEM/TypeInstance.hpp>

#include <locic/Support/MethodID.hpp>

namespace locic {
	
	namespace CodeGen {
		
		SEMCodeEmitter::SEMCodeEmitter(Function& functionGenerator)
		: functionGenerator_(functionGenerator) { }
		
		void
		SEMCodeEmitter::emitFunctionCode(const SEM::TypeInstance* const typeInstance,
		                                 const SEM::Function& function,
		                                 const bool isInnerMethod) {
			// TODO: remove this horrible code...
			const bool isOuterMethod = (function.name().last() == "__moveto" ||
			                            function.name().last() == "__destroy") &&
			                           !isInnerMethod;
			if (function.isAutoGenerated() || function.isPrimitive() || isOuterMethod) {
				emitBuiltInFunctionCode(typeInstance,
				                        function,
				                        isInnerMethod);
			} else {
				emitUserFunctionCode(function);
			}
		}
		
		llvm::Value*
		SEMCodeEmitter::emitBuiltInFunctionContents(const MethodID methodID,
		                                            const bool isInnerMethod,
		                                            const SEM::TypeInstance* const typeInstance,
		                                            const SEM::Function& function,
		                                            PendingResultArray args,
		                                            llvm::Value* const hintResultValue) {
			if (!function.isPrimitive()) {
				DefaultMethodEmitter defaultMethodEmitter(functionGenerator_);
				return defaultMethodEmitter.emitMethod(methodID,
				                                       isInnerMethod,
				                                       typeInstance->selfType(),
				                                       function.type(),
				                                       std::move(args),
				                                       hintResultValue);
			} else {
				assert(!isInnerMethod);
				
				SEM::ValueArray templateArgs;
				for (const auto& templateVar: function.templateVariables()) {
					templateArgs.push_back(templateVar->selfRefValue());
				}
				
				const auto type = typeInstance != nullptr ? typeInstance->selfType() : nullptr;
				
				IREmitter irEmitter(functionGenerator_);
				PrimitiveFunctionEmitter primitiveFunctionEmitter(irEmitter);
				return primitiveFunctionEmitter.emitFunction(methodID, type,
				                                             arrayRef(templateArgs),
				                                             std::move(args),
				                                             hintResultValue);
			}
		}
		
		void
		SEMCodeEmitter::emitBuiltInFunctionCode(const SEM::TypeInstance* const typeInstance,
		                                        const SEM::Function& function,
		                                        const bool isInnerMethod) {
			auto& module = functionGenerator_.module();
			
			const auto& argInfo = functionGenerator_.getArgInfo();
			
			PendingResultArray args;
			
			Array<RefPendingResult, 1> contextPendingResult;
			if (argInfo.hasContextArgument()) {
				assert(typeInstance != nullptr);
				const auto contextValue = functionGenerator_.getContextValue();
				contextPendingResult.push_back(RefPendingResult(contextValue,
				                                                typeInstance->selfType()));
				args.push_back(contextPendingResult.back());
			}
			
			// Need an array to store all the pending results
			// being referred to in 'genTrivialPrimitiveFunctionCall'.
			Array<ValuePendingResult, 10> pendingResultArgs;
			
			const auto& argTypes = function.type().parameterTypes();
			for (size_t i = 0; i < argTypes.size(); i++) {
				const auto argValue = functionGenerator_.getArg(i);
				pendingResultArgs.push_back(ValuePendingResult(argValue, argTypes[i]));
				args.push_back(pendingResultArgs.back());
			}
			
			const auto& methodName = function.name().last();
			const auto methodID = module.context().getMethodID(CanonicalizeMethodName(methodName));
			
			const auto hintResultValue = functionGenerator_.getReturnVarOrNull();
			
			const auto result = emitBuiltInFunctionContents(methodID,
			                                                isInnerMethod,
			                                                typeInstance,
			                                                function,
			                                                std::move(args),
			                                                hintResultValue);
			
			const auto returnType = function.type().returnType();
			
			IREmitter irEmitter(functionGenerator_);
			
			// Return the result in the appropriate way.
			if (argInfo.hasReturnVarArgument()) {
				irEmitter.emitMoveStore(result,
				                        functionGenerator_.getReturnVar(),
				                        returnType);
				irEmitter.emitReturnVoid();
			} else if (!returnType->isBuiltInVoid()) {
				functionGenerator_.returnValue(result);
			} else {
				irEmitter.emitReturnVoid();
			}
		}
		
		void
		SEMCodeEmitter::emitUserFunctionCode(const SEM::Function& function) {
			assert(!function.isAutoGenerated());
			
			emitParameterAllocas(function);
			emitScopeFunctionCode(function);
		}
		
		void
		SEMCodeEmitter::emitParameterAllocas(const SEM::Function& function) {
			auto& module = functionGenerator_.module();
			
			for (size_t i = 0; i < function.parameters().size(); i++) {
				const auto& paramVar = function.parameters()[i];
				
				// If value can't be passed by value it will be
				// passed by pointer, which can be re-used for
				// a variable to avoid an alloca.
				const auto hintResultValue =
					canPassByValue(module, paramVar->constructType()) ?
						nullptr :
						functionGenerator_.getArg(i);
				
				genVarAlloca(functionGenerator_, paramVar, hintResultValue);
			}
		}
		
		void
		SEMCodeEmitter::emitScopeFunctionCode(const SEM::Function& function) {
			ScopeLifetime functionScopeLifetime(functionGenerator_);
			
			// Parameters need to be copied to the stack, so that it's
			// possible to assign to them, take their address, etc.
			const auto& parameterVars = function.parameters();
			
			for (size_t i = 0; i < parameterVars.size(); i++) {
				const auto paramVar = parameterVars.at(i);
				
				// Get the variable's alloca.
				const auto stackObject = functionGenerator_.getLocalVarMap().get(paramVar);
				
				// Store the argument into the stack alloca.
				genStoreVar(functionGenerator_,
				            functionGenerator_.getArg(i),
				            stackObject,
				            paramVar);
				
				// Add this to the list of variables to be
				// destroyed at the end of the function.
				scheduleDestructorCall(functionGenerator_,
				                       paramVar->lvalType(),
				                       stackObject);
			}
			
			IREmitter irEmitter(functionGenerator_);
			ScopeEmitter(irEmitter).emitScope(function.scope());
		}
		
	}
	
}
