#ifndef LOCIC_CODEGEN_FUNCTIONCALLINFO_HPP
#define LOCIC_CODEGEN_FUNCTIONCALLINFO_HPP

#include <locic/Support/Optional.hpp>

namespace locic {
	
	namespace SEM {
		
		class Function;
		class Type;
		class Value;
		
	}
	
	namespace CodeGen {
		
		class Function;
		class Module;
		class PendingResult;
		struct VirtualMethodComponents;
		
		struct FunctionCallInfo {
			llvm::Value* functionPtr;
			llvm::Value* templateGenerator;
			llvm::Value* contextPointer;
			
			FunctionCallInfo()
				: functionPtr(nullptr),
				templateGenerator(nullptr),
				contextPointer(nullptr) { }
		};
		
		llvm::Value* genFunctionRef(Function& function, const SEM::Type* parentType, SEM::Function* semFunction, const SEM::Type* functionType);
		
		bool isTrivialFunction(Module& module, const SEM::Value& value);
		
		llvm::Value* genTrivialFunctionCall(Function& function, const SEM::Value& value, llvm::ArrayRef<SEM::Value> args,
			Optional<llvm::DebugLoc> debugLoc = None, llvm::Value* const hintResultValue = nullptr);
		
		FunctionCallInfo genFunctionCallInfo(Function& function, const SEM::Value& value);
		
		TypeInfoComponents genTypeInfoComponents(Function& function, const SEM::Value& value);
		
		TypeInfoComponents genBoundTypeInfoComponents(Function& function, const SEM::Value& value);
		
		VirtualMethodComponents genVirtualMethodComponents(Function& function, const SEM::Value& value);
		
	}
	
}

#endif
