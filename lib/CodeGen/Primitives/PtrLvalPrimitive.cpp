#include <assert.h>

#include <stdexcept>
#include <string>
#include <vector>

#include <llvm-abi/ABI.hpp>
#include <llvm-abi/ABITypeInfo.hpp>
#include <llvm-abi/Type.hpp>
#include <llvm-abi/TypeBuilder.hpp>

#include <locic/CodeGen/ArgInfo.hpp>
#include <locic/CodeGen/ConstantGenerator.hpp>
#include <locic/CodeGen/Debug.hpp>
#include <locic/CodeGen/Destructor.hpp>
#include <locic/CodeGen/Function.hpp>
#include <locic/CodeGen/FunctionCallInfo.hpp>
#include <locic/CodeGen/GenABIType.hpp>
#include <locic/CodeGen/GenFunctionCall.hpp>
#include <locic/CodeGen/GenType.hpp>
#include <locic/CodeGen/GenVTable.hpp>
#include <locic/CodeGen/Interface.hpp>
#include <locic/CodeGen/InternalContext.hpp>
#include <locic/CodeGen/IREmitter.hpp>
#include <locic/CodeGen/Liveness.hpp>
#include <locic/CodeGen/Memory.hpp>
#include <locic/CodeGen/Module.hpp>
#include <locic/CodeGen/Move.hpp>
#include <locic/CodeGen/Primitive.hpp>
#include <locic/CodeGen/Primitives.hpp>
#include <locic/CodeGen/Primitives/PtrLvalPrimitive.hpp>
#include <locic/CodeGen/Routines.hpp>
#include <locic/CodeGen/SizeOf.hpp>
#include <locic/CodeGen/Support.hpp>
#include <locic/CodeGen/Template.hpp>
#include <locic/CodeGen/TypeGenerator.hpp>
#include <locic/CodeGen/UnwindAction.hpp>
#include <locic/CodeGen/VTable.hpp>

#include <locic/Support/MethodID.hpp>

namespace locic {
	
	namespace CodeGen {
		
		PtrLvalPrimitive::PtrLvalPrimitive(const SEM::TypeInstance& typeInstance)
		: typeInstance_(typeInstance) {
			(void) typeInstance_;
		}
		
		bool PtrLvalPrimitive::isSizeAlwaysKnown(const TypeInfo& /*typeInfo*/,
		                                               llvm::ArrayRef<AST::Value> /*templateArguments*/) const {
			return true;
		}
		
		bool PtrLvalPrimitive::isSizeKnownInThisModule(const TypeInfo& /*typeInfo*/,
		                                                     llvm::ArrayRef<AST::Value> /*templateArguments*/) const {
			return true;
		}
		
		bool PtrLvalPrimitive::hasCustomDestructor(const TypeInfo& /*typeInfo*/,
		                                        llvm::ArrayRef<AST::Value> /*templateArguments*/) const {
			return false;
		}
		
		bool PtrLvalPrimitive::hasCustomMove(const TypeInfo& /*typeInfo*/,
		                                  llvm::ArrayRef<AST::Value> /*templateArguments*/) const {
			return false;
		}
		
		llvm_abi::Type PtrLvalPrimitive::getABIType(Module& /*module*/,
		                                            const llvm_abi::TypeBuilder& /*abiTypeBuilder*/,
		                                            llvm::ArrayRef<AST::Value> /*templateArguments*/) const {
			return llvm_abi::PointerTy;
		}
		
		llvm::Type* PtrLvalPrimitive::getIRType(Module& /*module*/,
		                                        const TypeGenerator& typeGenerator,
		                                        llvm::ArrayRef<AST::Value> /*templateArguments*/) const {
			return typeGenerator.getPtrType();
		}
		
		llvm::Value* PtrLvalPrimitive::emitMethod(IREmitter& irEmitter,
		                                          const MethodID methodID,
		                                          llvm::ArrayRef<AST::Value> typeTemplateArguments,
		                                          llvm::ArrayRef<AST::Value> /*functionTemplateArguments*/,
		                                          PendingResultArray args,
		                                          llvm::Value* /*hintResultValue*/) const {
			auto& function = irEmitter.function();
			auto& module = irEmitter.module();
			
			const auto targetType = typeTemplateArguments.front().typeRefType();
			
			switch (methodID) {
				case METHOD_ALIGNMASK: {
					const auto abiType = this->getABIType(module,
					                                      module.abiTypeBuilder(),
					                                      typeTemplateArguments);
					return ConstantGenerator(module).getSizeTValue(module.abi().typeInfo().getTypeRequiredAlign(abiType).asBytes() - 1);
				}
				case METHOD_SIZEOF: {
					const auto abiType = this->getABIType(module,
					                                      module.abiTypeBuilder(),
					                                      typeTemplateArguments);
					return ConstantGenerator(module).getSizeTValue(module.abi().typeInfo().getTypeAllocSize(abiType).asBytes());
				}
				case METHOD_MOVETO: {
					const auto moveToPtr = args[1].resolve(function);
					const auto moveToPosition = args[2].resolve(function);
					const auto methodOwner = args[0].resolveWithoutBind(function);
					
					const auto destPtr = irEmitter.emitInBoundsGEP(irEmitter.typeGenerator().getI8Type(),
					                                               moveToPtr,
					                                               moveToPosition);
					irEmitter.emitRawStore(methodOwner, destPtr);
					return ConstantGenerator(module).getVoidUndef();
				}
				case METHOD_ADDRESS:
				case METHOD_DISSOLVE: {
					return args[0].resolveWithoutBind(function);
				}
				case METHOD_ASSIGN: {
					const auto operand = args[1].resolve(function);
					const auto methodOwner = args[0].resolveWithoutBind(function);
					
					// Destroy existing value.
					irEmitter.emitDestructorCall(methodOwner, targetType);
					
					// Assign new value.
					irEmitter.emitMoveStore(operand, methodOwner, targetType);
					
					return ConstantGenerator(module).getVoidUndef();
				}
				case METHOD_MOVE: {
					const auto methodOwner = args[0].resolveWithoutBind(function);
					return irEmitter.emitMoveLoad(methodOwner, targetType);
				}
				case METHOD_SETVALUE: {
					const auto operand = args[1].resolve(function);
					const auto methodOwner = args[0].resolveWithoutBind(function);
					
					// Assign new value.
					irEmitter.emitMoveStore(operand, methodOwner, targetType);
					
					return ConstantGenerator(module).getVoidUndef();
				}
				case METHOD_EXTRACTVALUE: {
					const auto methodOwner = args[0].resolveWithoutBind(function);
					return irEmitter.emitMoveLoad(methodOwner, targetType);
				}
				case METHOD_DESTROYVALUE: {
					const auto methodOwner = args[0].resolveWithoutBind(function);
					// Destroy existing value.
					irEmitter.emitDestructorCall(methodOwner, targetType);
					return ConstantGenerator(module).getVoidUndef();
				}
				default:
					llvm_unreachable("Unknown ptr_lval primitive method.");
			}
		}
		
	}
	
}

