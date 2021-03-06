#include <vector>

#include <locic/AST/Type.hpp>
#include <locic/AST/TypeInstance.hpp>

#include <locic/CodeGen/ArgInfo.hpp>
#include <locic/CodeGen/ConstantGenerator.hpp>
#include <locic/CodeGen/Exception.hpp>
#include <locic/CodeGen/Function.hpp>
#include <locic/CodeGen/IREmitter.hpp>
#include <locic/CodeGen/LLVMIncludes.hpp>
#include <locic/CodeGen/Module.hpp>
#include <locic/CodeGen/ScopeExitActions.hpp>
#include <locic/CodeGen/Support.hpp>
#include <locic/CodeGen/TypeGenerator.hpp>
#include <locic/CodeGen/UnwindState.hpp>

namespace locic {

	namespace CodeGen {
	
		llvm::Function* getExceptionAllocateFunction(Module& module) {
			const auto functionName = module.getCString("__loci_allocate_exception");
			const auto iterator = module.getFunctionMap().find(functionName);
			
			if (iterator != module.getFunctionMap().end()) {
				return iterator->second;
			}
			
			const llvm_abi::Type argTypes[] = { llvm_abi::SizeTy };
			const auto argInfo = ArgInfo::Basic(module, llvm_abi::PointerTy, argTypes).withNoExcept();
			
			const auto function = argInfo.createFunction(functionName.c_str(),
			                                             llvm::Function::ExternalLinkage);
			module.getFunctionMap().insert(std::make_pair(functionName, function));
			return function;
		}
		
		llvm::Function* getExceptionFreeFunction(Module& module) {
			const auto functionName = module.getCString("__loci_free_exception");
			const auto iterator = module.getFunctionMap().find(functionName);
			
			if (iterator != module.getFunctionMap().end()) {
				return iterator->second;
			}
			
			const llvm_abi::Type argTypes[] = { llvm_abi::PointerTy };
			const auto argInfo = ArgInfo::Basic(module, llvm_abi::VoidTy, argTypes).withNoExcept();
			
			const auto function = argInfo.createFunction(functionName.c_str(),
			                                             llvm::Function::ExternalLinkage);
			module.getFunctionMap().insert(std::make_pair(functionName, function));
			return function;
		}
		
		llvm::Function* getExceptionThrowFunction(Module& module) {
			const auto functionName = module.getCString("__loci_throw");
			const auto iterator = module.getFunctionMap().find(functionName);
			
			if (iterator != module.getFunctionMap().end()) {
				return iterator->second;
			}
			
			const llvm_abi::Type argTypes[] = { llvm_abi::PointerTy, llvm_abi::PointerTy, llvm_abi::PointerTy };
			const auto argInfo = ArgInfo::Basic(module, llvm_abi::VoidTy, argTypes).withNoReturn();
			
			const auto function = argInfo.createFunction(functionName.c_str(),
			                                             llvm::Function::ExternalLinkage);
			module.getFunctionMap().insert(std::make_pair(functionName, function));
			return function;
		}
		
		llvm::Function* getExceptionRethrowFunction(Module& module) {
			const auto functionName = module.getCString("__loci_rethrow");
			const auto iterator = module.getFunctionMap().find(functionName);
			
			if (iterator != module.getFunctionMap().end()) {
				return iterator->second;
			}
			
			const llvm_abi::Type argTypes[] = { llvm_abi::PointerTy };
			const auto argInfo = ArgInfo::Basic(module, llvm_abi::VoidTy, argTypes).withNoReturn();
			
			const auto function = argInfo.createFunction(functionName.c_str(),
			                                             llvm::Function::ExternalLinkage);
			module.getFunctionMap().insert(std::make_pair(functionName, function));
			
			return function;
		}
		
		llvm::Function* getExceptionPersonalityFunction(Module& module) {
			const auto functionName = module.getCString("__loci_personality_v0");
			const auto iterator = module.getFunctionMap().find(functionName);
			
			if (iterator != module.getFunctionMap().end()) {
				return iterator->second;
			}
			
			const auto argInfo = ArgInfo::VarArgs(module, llvm_abi::Int32Ty, {}).withNoExcept();
			
			const auto function = argInfo.createFunction(functionName.c_str(),
			                                             llvm::Function::ExternalLinkage);
			module.getFunctionMap().insert(std::make_pair(functionName, function));
			return function;
		}
		
		llvm::Function* getExceptionPtrFunction(Module& module) {
			const auto functionName = module.getCString("__loci_get_exception");
			const auto iterator = module.getFunctionMap().find(functionName);
			
			if (iterator != module.getFunctionMap().end()) {
				return iterator->second;
			}
			
			const llvm_abi::Type argTypes[] = { llvm_abi::PointerTy };
			const auto argInfo = ArgInfo::Basic(module, llvm_abi::PointerTy, argTypes).withNoExcept().withNoMemoryAccess();
			
			const auto function = argInfo.createFunction(functionName.c_str(),
			                                             llvm::Function::ExternalLinkage);
			module.getFunctionMap().insert(std::make_pair(functionName, function));
			
			return function;
		}
		
		llvm::BasicBlock* getLatestLandingPadBlock(Function& function, UnwindState unwindState) {
			assert(unwindState == UnwindStateThrow || unwindState == UnwindStateRethrow);
			
			const auto& unwindStack = function.unwindStack();
			
			for (size_t i = 0; i < unwindStack.size(); i++) {
				const size_t pos = unwindStack.size() - i - 1;
				const auto& action = unwindStack.at(pos);
				
				if (action.landingPadBlock(unwindState) != nullptr) {
					return action.landingPadBlock(unwindState);
				}
				
				if (action.isActiveForState(unwindState)) {
					break;
				}
			}
			
			return nullptr;
		}
		
		void setLatestLandingPadBlock(Function& function, UnwindState unwindState, llvm::BasicBlock* landingPadBB) {
			assert(unwindState == UnwindStateThrow || unwindState == UnwindStateRethrow);
			
			auto& unwindStack = function.unwindStack();
			
			for (size_t i = 0; i < unwindStack.size(); i++) {
				const size_t pos = unwindStack.size() - i - 1;
				auto& action = unwindStack.at(pos);
				
				action.setLandingPadBlock(unwindState, landingPadBB);
				
				if (action.isActiveForState(unwindState)) {
					return;
				}
			}
		}
		
		llvm::BasicBlock* genLandingPad(Function& function, UnwindState unwindState) {
			assert(unwindState == UnwindStateThrow || unwindState == UnwindStateRethrow);
			assert(anyUnwindActions(function, unwindState));
			
			// See if the landing pad has already been generated (this
			// depends on whether any extra actions have been added that
			// need to be run when unwinding, in case a new landing pad
			// must be created).
			const auto latestLandingPadBlock = getLatestLandingPadBlock(function, unwindState);
			if (latestLandingPadBlock != nullptr) {
				return latestLandingPadBlock;
			}
			
			IREmitter irEmitter(function);
			
			auto& module = function.module();
			const auto& unwindStack = function.unwindStack();
			
			TypeGenerator typeGen(module);
			const auto landingPadType = typeGen.getStructType(std::vector<llvm::Type*> {typeGen.getPtrType(), typeGen.getI32Type()});
			const auto personalityFunction = getExceptionPersonalityFunction(module);
			function.setPersonalityFunction(personalityFunction);
			
			// Find all catch types on the stack.
			llvm::SmallVector<llvm::Constant*, 5> catchTypes;
			
			for (size_t i = 0; i < unwindStack.size(); i++) {
				const size_t pos = unwindStack.size() - i - 1;
				const auto& action = unwindStack.at(pos);
				
				if (action.isCatch()) {
					catchTypes.push_back(action.catchTypeInfo());
				}
			}
			
			const auto currentBB = function.getBuilder().GetInsertBlock();
			
			const auto landingPadBB = irEmitter.createBasicBlock("");
			
			irEmitter.selectBasicBlock(landingPadBB);
			
			const auto landingPad = irEmitter.emitLandingPad(landingPadType,
			                                                 catchTypes.size());
			landingPad->setCleanup(anyUnwindCleanupActions(function, unwindState));
			
			for (size_t i = 0; i < catchTypes.size(); i++) {
				landingPad->addClause(ConstantGenerator(module).getPointerCast(catchTypes[i], typeGen.getPtrType()));
			}
			
			irEmitter.emitRawStore(landingPad, function.exceptionInfo());
			
			// Unwind stack due to exception.
			irEmitter.emitUnwind(unwindState);
			
			// Save the landing pad so it can be re-used.
			setLatestLandingPadBlock(function, unwindState, landingPadBB);
			
			irEmitter.selectBasicBlock(currentBB);
			
			return landingPadBB;
		}
		
		llvm::Constant* getTypeNameGlobal(Module& module, const String& typeName) {
			ConstantGenerator constGen(module);
			TypeGenerator typeGen(module);
			
			// Generate the name of the exception type as a constant C string.
			const auto typeNameConstant = constGen.getString(typeName);
			const auto typeNameType = typeGen.getArrayType(typeGen.getI8Type(), typeName.size() + 1);
			const auto typeNameGlobal = module.createConstGlobal(module.getCString("type_name"), typeNameType, llvm::GlobalValue::InternalLinkage, typeNameConstant);
			typeNameGlobal->setAlignment(1);
			
			// Convert array to a pointer.
			return constGen.getGetElementPtr(typeNameType, typeNameGlobal, std::vector<llvm::Constant*> {constGen.getI32(0), constGen.getI32(0)});
		}
		
		llvm::Constant* genCatchInfo(Module& module, const AST::TypeInstance* const catchTypeInstance) {
			assert(catchTypeInstance->isException());
			
			const auto typeName = catchTypeInstance->fullName().genString();
			const auto typeNameGlobalPtr = getTypeNameGlobal(module, typeName);
			
			ConstantGenerator constGen(module);
			TypeGenerator typeGen(module);
			
			// Create a constant struct {i32, i8*} for the catch type info.
			const auto typeInfoType = typeGen.getStructType(std::vector<llvm::Type*> {typeGen.getI32Type(), typeGen.getPtrType()});
			
			// Calculate offset to check based on number of parents.
			size_t offset = 0;
			const AST::TypeInstance* currentInstance = catchTypeInstance;
			while (currentInstance->parentType() != nullptr) {
				offset++;
				currentInstance = currentInstance->parentType()->getObjectType();
			}
			
			const auto castedTypeNamePtr = constGen.getPointerCast(typeNameGlobalPtr, typeGen.getPtrType());
			const auto typeInfoValue = constGen.getStruct(typeInfoType, std::vector<llvm::Constant*> {constGen.getI32(offset), castedTypeNamePtr});
			
			const auto typeInfoGlobal = module.createConstGlobal(module.getCString("catch_type_info"), typeInfoType, llvm::GlobalValue::InternalLinkage, typeInfoValue);
			
			return constGen.getGetElementPtr(typeInfoType, typeInfoGlobal, std::vector<llvm::Constant*> {constGen.getI32(0), constGen.getI32(0)});
		}
		
		llvm::Constant* genThrowInfo(Module& module,const  AST::TypeInstance* const throwTypeInstance) {
			assert(throwTypeInstance->isException());
			
			Array<String, 10> typeNames;
			
			// Add type names in REVERSE order.
			const AST::TypeInstance* currentInstance = throwTypeInstance;
			while (currentInstance != nullptr) {
				typeNames.push_back(currentInstance->fullName().genString());
				currentInstance = currentInstance->parentType() != nullptr ?
					currentInstance->parentType()->getObjectType() : nullptr;
			}
			
			assert(!typeNames.empty());
			
			// Since type names were added in reverse
			// order, fix this by reversing the array.
			std::reverse(typeNames.begin(), typeNames.end());
			
			ConstantGenerator constGen(module);
			TypeGenerator typeGen(module);
			
			// Create a constant struct {i32, i8*} for the throw type info.
			const auto typeNameArrayType = typeGen.getArrayType(typeGen.getPtrType(), typeNames.size());
			const auto typeInfoType = typeGen.getStructType(std::vector<llvm::Type*> {typeGen.getI32Type(), typeNameArrayType});
			
			std::vector<llvm::Constant*> typeNameConstants;
			
			for (const auto& typeName : typeNames) {
				const auto typeNameGlobalPtr = getTypeNameGlobal(module, typeName);
				const auto castedTypeNamePtr = constGen.getPointerCast(typeNameGlobalPtr, typeGen.getPtrType());
				typeNameConstants.push_back(castedTypeNamePtr);
			}
			
			const auto typeNameArray = constGen.getArray(typeNameArrayType, typeNameConstants);
			const auto typeInfoValue = constGen.getStruct(typeInfoType, std::vector<llvm::Constant*> {constGen.getI32(typeNames.size()), typeNameArray});
			
			const auto typeInfoGlobal = module.createConstGlobal(module.getCString("throw_type_info"), typeInfoType, llvm::GlobalValue::InternalLinkage, typeInfoValue);
			
			return constGen.getGetElementPtr(typeInfoType, typeInfoGlobal, std::vector<llvm::Constant*> {constGen.getI32(0), constGen.getI32(0)});
		}
		
		TryScope::TryScope(Function& function, llvm::BasicBlock* catchBlock, llvm::ArrayRef<llvm::Constant*> catchTypeList)
			: function_(function), catchCount_(catchTypeList.size()) {
			assert(!catchTypeList.empty());
			for (size_t i = 0; i < catchTypeList.size(); i++) {
				// Push in reverse order.
				const auto catchType = catchTypeList[catchTypeList.size() - i - 1];
				function_.pushUnwindAction(UnwindAction::CatchException(catchBlock, catchType));
			}
		}
		
		TryScope::~TryScope() {
			for (size_t i = 0; i < catchCount_; i++) {
				assert(function_.unwindStack().back().isCatch());
				function_.popUnwindAction();
			}
		}
		
	}
	
}

