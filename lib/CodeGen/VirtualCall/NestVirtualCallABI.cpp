#include <locic/CodeGen/VirtualCall/NestVirtualCallABI.hpp>

#include <vector>

#include <llvm-abi/Type.hpp>

#include <locic/AST/Function.hpp>
#include <locic/AST/Type.hpp>

#include <locic/CodeGen/ArgInfo.hpp>
#include <locic/CodeGen/ASTFunctionGenerator.hpp>
#include <locic/CodeGen/CallEmitter.hpp>
#include <locic/CodeGen/ConstantGenerator.hpp>
#include <locic/CodeGen/Function.hpp>
#include <locic/CodeGen/GenABIType.hpp>
#include <locic/CodeGen/Interface.hpp>
#include <locic/CodeGen/IREmitter.hpp>
#include <locic/CodeGen/Module.hpp>
#include <locic/CodeGen/Primitives.hpp>
#include <locic/CodeGen/Support.hpp>
#include <locic/CodeGen/Template.hpp>
#include <locic/CodeGen/TypeGenerator.hpp>
#include <locic/CodeGen/UnwindAction.hpp>
#include <locic/CodeGen/VirtualCallABI.hpp>
#include <locic/CodeGen/VTable.hpp>

namespace locic {
	
	namespace CodeGen {
		
		NestVirtualCallABI::NestVirtualCallABI(Module& module)
		: module_(module) { }
		
		NestVirtualCallABI::~NestVirtualCallABI() { }
		
		ArgInfo
		NestVirtualCallABI::getStubArgInfo() {
			TypeGenerator typeGen(module_);
			
			// Return i64 as a generic register sized value.
			return ArgInfo::VarArgs(module_, llvm_abi::Int64Ty, {}).withNestArgument();
		}
		
		llvm::AttributeList
		NestVirtualCallABI::conflictResolutionStubAttributes(const llvm::AttributeList& existingAttributes) {
			auto& context = module_.getLLVMContext();
			
			auto attributes = existingAttributes;
			
			// Always inline stubs.
			attributes = attributes.addAttribute(context, llvm::AttributeList::FunctionIndex, llvm::Attribute::AlwaysInline);
			
			return attributes;
		}
		
		llvm::Constant*
		NestVirtualCallABI::emitVTableSlot(const AST::TypeInstance& typeInstance,
		                                      llvm::ArrayRef<AST::Function*> methods) {
			ConstantGenerator constGen(module_);
			TypeGenerator typeGen(module_);
			
			if (methods.empty()) {
				return constGen.getNullPointer();
			}
			
			if (methods.size() == 1) {
				// Only one method, so place it directly in the slot.
				auto& astFunctionGenerator = module_.astFunctionGenerator();
				const auto llvmMethod = astFunctionGenerator.getDecl(&typeInstance,
				                                                     *methods[0]);
				return llvmMethod;
			}
			
			const auto stubArgInfo = getStubArgInfo();
			const auto linkage = llvm::Function::InternalLinkage;
			
			const auto llvmFunction = stubArgInfo.createFunction("__slot_conflict_resolution_stub",
			                                                     linkage);
			llvmFunction->setAttributes(conflictResolutionStubAttributes(llvmFunction->getAttributes()));
			
			Function function(module_, *llvmFunction, stubArgInfo);
			
			IREmitter irEmitter(function);
			auto& builder = function.getBuilder();
			
			const auto llvmHashValuePtr = function.getNestArgument();
			const auto llvmHashValue = builder.CreatePtrToInt(llvmHashValuePtr,
			                                                  typeGen.getI64Type());
			
			for (const auto method : methods) {
				const auto callMethodBasicBlock = irEmitter.createBasicBlock("callMethod");
				const auto tryNextMethodBasicBlock = irEmitter.createBasicBlock("tryNextMethod");
				
				const auto methodHash = CreateMethodNameHash(method->fullName().last());
				
				const auto cmpValue = builder.CreateICmpEQ(llvmHashValue, constGen.getI64(methodHash));
				irEmitter.emitCondBranch(cmpValue, callMethodBasicBlock,
				                         tryNextMethodBasicBlock);
				
				irEmitter.selectBasicBlock(callMethodBasicBlock);
				
				auto& astFunctionGenerator = module_.astFunctionGenerator();
				const auto llvmMethod = astFunctionGenerator.getDecl(&typeInstance,
				                                                     *method);
				llvm::Value* const parameters[] = { llvmHashValuePtr };
				
				// Use 'musttail' to ensure perfect forwarding.
				CallEmitter callEmitter(irEmitter);
				const auto result = callEmitter.emitRawCall(stubArgInfo, llvmMethod, parameters,
				                                            /*musttail=*/true);
				irEmitter.emitReturn(result);
				
				irEmitter.selectBasicBlock(tryNextMethodBasicBlock);
			}
			
			// Terminate function with unreachable
			// (notifies optimiser that this should
			// never be reached...).
			irEmitter.emitUnreachable();
			
			return llvmFunction;
		}
		
		llvm::Value*
		NestVirtualCallABI::emitRawCall(IREmitter& irEmitter,
		                                const ArgInfo& argInfo,
		                                const VirtualMethodComponents methodComponents,
		                                llvm::ArrayRef<llvm::Value*> args,
		                                llvm::Value* const returnVarPointer) {
			ConstantGenerator constantGen(module_);
			
			// Calculate the slot for the virtual call.
			const auto vtableSizeValue = constantGen.getI64(VTABLE_SIZE);
			const auto vtableOffsetValue = irEmitter.builder().CreateURem(methodComponents.hashValue,
			                                                              vtableSizeValue, "vtableOffset");
			const auto castVTableOffsetValue = irEmitter.builder().CreateTrunc(vtableOffsetValue,
			                                                                   irEmitter.typeGenerator().getI32Type());
			
			// Get a pointer to the slot.
			llvm::SmallVector<llvm::Value*, 3> vtableEntryGEP;
			vtableEntryGEP.push_back(constantGen.getI32(0));
			vtableEntryGEP.push_back(constantGen.getI32(2));
			vtableEntryGEP.push_back(castVTableOffsetValue);
			
			const auto vtableEntryPointer = irEmitter.emitInBoundsGEP(vtableType(module_),
			                                                          methodComponents.object.typeInfo.vtablePointer,
			                                                          vtableEntryGEP);
			
			// Load the slot.
			const auto methodFunctionPointer = irEmitter.emitRawLoad(vtableEntryPointer,
			                                                         llvm_abi::PointerTy);
			
			// Cast hash value to pointer so we can pass it through
			// as the 'nest' parameter.
			const auto hashValuePtr = irEmitter.builder().CreateIntToPtr(methodComponents.hashValue,
			                                                             irEmitter.typeGenerator().getPtrType());
			
			llvm::SmallVector<llvm::Value*, 8> newArgs;
			newArgs.reserve(args.size() + 4);
			
			// Add the 'nest' parameter.
			newArgs.push_back(hashValuePtr);
			
			// Add return pointer if necessary.
			if (argInfo.hasReturnVarArgument()) {
				assert(returnVarPointer != nullptr);
				newArgs.push_back(returnVarPointer);
			} else {
				assert(returnVarPointer == nullptr);
			}
			
			// Add object pointer.
			if (argInfo.hasContextArgument()) {
				newArgs.push_back(methodComponents.object.objectPointer);
			}
			
			// Add the method arguments.
			for (const auto& arg: args) {
				newArgs.push_back(arg);
			}
			
			// Add template generator.
			if (argInfo.hasTemplateGeneratorArgument()) {
				newArgs.push_back(methodComponents.object.typeInfo.templateGenerator);
			}
			
			// Call the stub function.
			CallEmitter callEmitter(irEmitter);
			return callEmitter.emitRawCall(argInfo.withNestArgument(),
			                               methodFunctionPointer, newArgs);
		}
		
		llvm::Value*
		NestVirtualCallABI::emitCall(IREmitter& irEmitter,
		                             const AST::FunctionType functionType,
		                             const VirtualMethodComponents methodComponents,
		                             llvm::ArrayRef<llvm::Value*> args,
		                             llvm::Value* const resultPtr) {
			const auto argInfo = ArgInfo::FromAST(irEmitter.module(),
			                                      functionType);
			
			// If necessary allocate space on the stack for the return value.
			const auto returnType = functionType.returnType();
			llvm::Value* returnVarPointer = nullptr;
			if (argInfo.hasReturnVarArgument()) {
				returnVarPointer = irEmitter.emitAlloca(returnType, resultPtr);
			}
			
			const auto result = emitRawCall(irEmitter,
			                                argInfo,
			                                methodComponents,
			                                args,
			                                returnVarPointer);
			
			return returnVarPointer != nullptr ?
			       irEmitter.emitLoad(returnVarPointer, returnType) : result;
		}
		
		llvm::Value*
		NestVirtualCallABI::emitCountFnCall(IREmitter& irEmitter,
		                                       llvm::Value* const typeInfoValue,
		                                       const CountFnKind kind) {
			// Extract vtable and template generator.
			const auto vtablePointer = irEmitter.builder().CreateExtractValue(typeInfoValue, { 0 }, "vtable");
			const auto templateGeneratorValue = irEmitter.builder().CreateExtractValue(typeInfoValue, { 1 }, "templateGenerator");
			
			// Get a pointer to the slot.
			ConstantGenerator constGen(module_);
			llvm::SmallVector<llvm::Value*, 2> vtableEntryGEP;
			vtableEntryGEP.push_back(constGen.getI32(0));
			vtableEntryGEP.push_back(constGen.getI32(kind == ALIGNOF ? 0 : 1));
			
			const auto vtableEntryPointer = irEmitter.emitInBoundsGEP(vtableType(module_),
			                                                          vtablePointer,
			                                                          vtableEntryGEP);
			
			const auto argInfo = ArgInfo::TemplateOnly(module_, llvm_abi::SizeTy).withNoMemoryAccess().withNoExcept();
			
			// Load the slot.
			const auto methodFunctionPointer = irEmitter.emitRawLoad(vtableEntryPointer,
			                                                         llvm_abi::PointerTy);
			
			CallEmitter callEmitter(irEmitter);
			return callEmitter.emitRawCall(argInfo, methodFunctionPointer,
			                               { templateGeneratorValue });
		}
		
	}
	
}
