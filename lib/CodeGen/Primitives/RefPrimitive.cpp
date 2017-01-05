#include <assert.h>

#include <stdexcept>
#include <string>
#include <vector>

#include <llvm-abi/ABI.hpp>
#include <llvm-abi/ABITypeInfo.hpp>
#include <llvm-abi/Type.hpp>
#include <llvm-abi/TypeBuilder.hpp>

#include <locic/AST/ValueDecl.hpp>
#include <locic/AST/TemplateVar.hpp>
#include <locic/AST/Type.hpp>

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
#include <locic/CodeGen/Module.hpp>
#include <locic/CodeGen/Primitive.hpp>
#include <locic/CodeGen/Primitives.hpp>
#include <locic/CodeGen/Primitives/RefPrimitive.hpp>
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
		
		RefPrimitive::RefPrimitive(const AST::TypeInstance& typeInstance)
		: typeInstance_(typeInstance) { }
		
		bool RefPrimitive::isSizeAlwaysKnown(const TypeInfo& /*typeInfo*/,
		                                               llvm::ArrayRef<AST::Value> templateArguments) const {
			const auto refTargetType = templateArguments.front().typeRefType();
			return !refTargetType->isTemplateVar() ||
			       !refTargetType->getTemplateVar()->isVirtual();
		}
		
		bool RefPrimitive::isSizeKnownInThisModule(const TypeInfo& /*typeInfo*/,
		                                           llvm::ArrayRef<AST::Value> templateArguments) const {
			const auto refTargetType = templateArguments.front().typeRefType();
			return !refTargetType->isTemplateVar() ||
			       !refTargetType->getTemplateVar()->isVirtual();
		}
		
		bool RefPrimitive::hasCustomDestructor(const TypeInfo& /*typeInfo*/,
		                                        llvm::ArrayRef<AST::Value> /*templateArguments*/) const {
			return false;
		}
		
		bool RefPrimitive::hasCustomMove(const TypeInfo& /*typeInfo*/,
		                                  llvm::ArrayRef<AST::Value> /*templateArguments*/) const {
			return false;
		}
		
		llvm_abi::Type RefPrimitive::getABIType(Module& module,
		                                        const llvm_abi::TypeBuilder& /*abiTypeBuilder*/,
		                                        llvm::ArrayRef<AST::Value> templateArguments) const {
			if (templateArguments.front().typeRefType()->isInterface()) {
				return interfaceStructType(module).first;
			} else {
				return llvm_abi::PointerTy;
			}
		}
		
		llvm::Type* RefPrimitive::getIRType(Module& module,
		                                    const TypeGenerator& typeGenerator,
		                                    llvm::ArrayRef<AST::Value> templateArguments) const {
			const auto argType = templateArguments.front().typeRefType();
			if (argType->isTemplateVar() && argType->getTemplateVar()->isVirtual()) {
				// Unknown whether the argument type is virtual, so use an opaque struct type.
				const auto iterator = module.typeInstanceMap().find(&typeInstance_);
				if (iterator != module.typeInstanceMap().end()) {
					return iterator->second;
				}
				
				const auto structType = TypeGenerator(module).getForwardDeclaredStructType(module.getCString("ref_t"));
				
				module.typeInstanceMap().insert(std::make_pair(&typeInstance_, structType));
				return structType;
			} else if (argType->isInterface()) {
				// Argument type is definitely virtual.
				return interfaceStructType(module).second;
			} else {
				// Argument type is definitely not virtual.
				return typeGenerator.getPtrType();
			}
		}
		
		namespace {
			
			bool isVirtualnessKnown(const AST::Type* const type) {
				// Virtual template variables may or may not be
				// instantiated with virtual types.
				return !type->isTemplateVar() ||
					!type->getTemplateVar()->isVirtual();
			}
			
			const AST::Type* getRefTarget(const AST::Type* const type) {
				const auto refTarget = type->templateArguments().at(0).typeRefType();
				assert(!refTarget->isAlias());
				return refTarget;
			}
			
			bool isRefVirtualnessKnown(const AST::Type* const type) {
				return isVirtualnessKnown(getRefTarget(type));
			}
			
			bool isRefVirtual(const AST::Type* const type) {
				assert(isRefVirtualnessKnown(type));
				return getRefTarget(type)->isInterface();
			}
			
			llvm::Type* getVirtualRefLLVMType(Module& module) {
				return interfaceStructType(module).second;
			}
			
			llvm::Type* getNotVirtualLLVMType(Module& module) {
				return TypeGenerator(module).getPtrType();
			}
			
			llvm::Type* getRefLLVMType(Module& module, const AST::Type* const type) {
				return isRefVirtual(type) ?
					getVirtualRefLLVMType(module) :
					getNotVirtualLLVMType(module);
			}
			
			template <typename Fn>
			llvm::Value* genRefPrimitiveMethodForVirtualCases(Function& function, const AST::Type* const type, Fn f) {
				auto& module = function.module();
				
				IREmitter irEmitter(function);
				
				if (isRefVirtualnessKnown(type)) {
					// If the reference target type is not a virtual template variable,
					// we already know whether it's a simple pointer or a 'fat'
					// pointer (i.e. a struct containing a pointer to the object
					// as well as the vtable and the template generator), so we
					// only generate for this known case.
					return f(getRefLLVMType(module, type));
				}
				
				// If the reference target type is a template variable, we need
				// to query the virtual-ness of it at run-time and hence we must
				// emit code to handle both cases.
				
				const auto refTarget = getRefTarget(type);
				
				// Look at our template argument to see if it's virtual.
				const auto argTypeInfo = function.getBuilder().CreateExtractValue(function.getTemplateArgs(), { (unsigned int) refTarget->getTemplateVar()->index() });
				const auto argVTablePointer = function.getBuilder().CreateExtractValue(argTypeInfo, { 0 });
				
				// If the VTable pointer is NULL, it's a virtual type, which
				// means we are larger (to store the type information etc.).
				const auto nullVtablePtr = ConstantGenerator(module).getNullPointer();
				const auto isVirtualCondition = function.getBuilder().CreateICmpEQ(argVTablePointer, nullVtablePtr, "isVirtual");
				
				const auto ifVirtualBlock = irEmitter.createBasicBlock("ifRefVirtual");
				const auto ifNotVirtualBlock = irEmitter.createBasicBlock("ifRefNotVirtual");
				const auto mergeBlock = irEmitter.createBasicBlock("mergeRefVirtual");
				
				irEmitter.emitCondBranch(isVirtualCondition, ifVirtualBlock,
				                         ifNotVirtualBlock);
				
				irEmitter.selectBasicBlock(ifVirtualBlock);
				const auto virtualType = getVirtualRefLLVMType(module);
				const auto virtualResult = f(virtualType);
				irEmitter.emitBranch(mergeBlock);
				
				irEmitter.selectBasicBlock(ifNotVirtualBlock);
				const auto notVirtualType = getNotVirtualLLVMType(module);
				const auto notVirtualResult = f(notVirtualType);
				irEmitter.emitBranch(mergeBlock);
				
				irEmitter.selectBasicBlock(mergeBlock);
				
				if (!virtualResult->getType()->isVoidTy()) {
					assert(!notVirtualResult->getType()->isVoidTy());
					if (virtualResult == notVirtualResult) {
						return virtualResult;
					} else {
						assert(virtualResult->getType() == notVirtualResult->getType());
						const auto phiNode = function.getBuilder().CreatePHI(virtualResult->getType(), 2);
						phiNode->addIncoming(virtualResult, ifVirtualBlock);
						phiNode->addIncoming(notVirtualResult, ifNotVirtualBlock);
						return phiNode;
					}
				} else {
					assert(notVirtualResult->getType()->isVoidTy());
					return ConstantGenerator(module).getVoidUndef();
				}
			}
			
			class RefMethodOwner {
			public:
				static RefMethodOwner AsRef(Function& function, const AST::Type* const type, PendingResultArray& args) {
					return RefMethodOwner(function, type, args, false);
				}
				
				static RefMethodOwner AsValue(Function& function, const AST::Type* const type, PendingResultArray& args) {
					return RefMethodOwner(function, type, args, true);
				}
				
			private:
				RefMethodOwner(Function& function, const AST::Type* const type, PendingResultArray& args, const bool loaded)
				: function_(function), type_(type), args_(args), loaded_(loaded), value_(nullptr) {
					if (loaded_) {
						// If the virtual-ness of the reference is known
						// then we can load the reference-to-reference
						// here, otherwise we need to wait until the
						// two cases are evaluated.
						if (isRefVirtualnessKnown(type_)) {
							value_ = args[0].resolveWithoutBind(function);
						} else {
							value_ = args[0].resolve(function);
						}
					} else {
						value_ = args[0].resolve(function);
					}
				}
				
			public:
				llvm::Value* get(llvm::Type* const llvmType) {
					if (loaded_) {
						// If the virtual-ness of the reference is known
						// then the reference is already loaded, otherwise it
						// needs to be loaded here.
						if (isRefVirtualnessKnown(type_)) {
							return value_;
						} else {
							IREmitter irEmitter(function_);
							return irEmitter.emitRawLoad(value_,
							                             llvmType);
						}
					} else {
						return value_;
					}
				}
				
			private:
				Function& function_;
				const AST::Type* const type_;
				PendingResultArray& args_;
				bool loaded_;
				llvm::Value* value_;
				
			};
			
		}
		
		llvm::Value* RefPrimitive::emitMethod(IREmitter& irEmitter,
		                                      const MethodID methodID,
		                                      llvm::ArrayRef<AST::Value> typeTemplateArguments,
		                                      llvm::ArrayRef<AST::Value> /*functionTemplateArguments*/,
		                                      PendingResultArray args,
		                                      llvm::Value* const hintResultValue) const {
			auto& builder = irEmitter.builder();
			auto& function = irEmitter.function();
			auto& module = irEmitter.module();
			
			AST::ValueArray valueArray;
			for (const auto& value: typeTemplateArguments) {
				valueArray.push_back(value.copy());
			}
			const auto type = AST::Type::Object(&typeInstance_,
			                                    std::move(valueArray));
			
			switch (methodID) {
				case METHOD_ALIGNMASK: {
					return genRefPrimitiveMethodForVirtualCases(function, type,
						[&](llvm::Type* const llvmType) {
							if (llvmType->isPointerTy()) {
								const auto nonVirtualAlign = module.abi().typeInfo().getTypeRequiredAlign(llvm_abi::PointerTy);
								return ConstantGenerator(module).getSizeTValue(nonVirtualAlign.asBytes() - 1);
							} else {
								const auto virtualAlign = module.abi().typeInfo().getTypeRequiredAlign(interfaceStructType(module).first);
								return ConstantGenerator(module).getSizeTValue(virtualAlign.asBytes() - 1);
							}
						}
					);
				}
				case METHOD_SIZEOF: {
					return genRefPrimitiveMethodForVirtualCases(function, type,
						[&](llvm::Type* const llvmType) {
							if (llvmType->isPointerTy()) {
								const auto nonVirtualSize = module.abi().typeInfo().getTypeRawSize(llvm_abi::PointerTy);
								return ConstantGenerator(module).getSizeTValue(nonVirtualSize.asBytes());
							} else {
								const auto virtualSize = module.abi().typeInfo().getTypeRawSize(interfaceStructType(module).first);
								return ConstantGenerator(module).getSizeTValue(virtualSize.asBytes());
							}
						}
					);
				}
				case METHOD_COPY:
				case METHOD_IMPLICITCOPY:
				case METHOD_MOVE: {
					// If the virtualness of the reference is known, we
					// can load it, otherwise we have to keep accessing
					// it by pointer.
					if (!isRefVirtualnessKnown(type) && hintResultValue == nullptr) {
						return args[0].resolve(function);
					}
					
					auto methodOwner = RefMethodOwner::AsValue(function, type, args);
					
					return genRefPrimitiveMethodForVirtualCases(function, type,
						[&](llvm::Type* const llvmType) {
							const auto loadedValue = methodOwner.get(llvmType);
							if (!isRefVirtualnessKnown(type)) {
								assert(hintResultValue != nullptr);
								irEmitter.emitRawStore(loadedValue,
								                       hintResultValue);
								return hintResultValue;
							} else {
								return loadedValue;
							}
						});
				}
				case METHOD_ISVALID: {
					auto methodOwner = RefMethodOwner::AsValue(function, type, args);
					
					return genRefPrimitiveMethodForVirtualCases(function, type,
						[&](llvm::Type* const llvmType) {
							const auto methodOwnerValue = methodOwner.get(llvmType);
							if (llvmType->isPointerTy()) {
								const auto nullValue = ConstantGenerator(module).getNull(llvmType);
								return irEmitter.emitI1ToBool(builder.CreateICmpNE(methodOwnerValue, nullValue));
							} else {
								const auto pointerValue = builder.CreateExtractValue(methodOwnerValue, { 0 });
								const auto nullValue = ConstantGenerator(module).getNull(pointerValue->getType());
								return irEmitter.emitI1ToBool(builder.CreateICmpNE(pointerValue, nullValue));
							}
						});
				}
				case METHOD_SETINVALID: {
					auto methodOwner = RefMethodOwner::AsRef(function, type, args);
					
					return genRefPrimitiveMethodForVirtualCases(function, type,
						[&](llvm::Type* const llvmType) {
							const auto methodOwnerPtr = methodOwner.get(llvmType);
							const auto nullValue = ConstantGenerator(module).getNull(llvmType);
							irEmitter.emitRawStore(nullValue, methodOwnerPtr);
							return ConstantGenerator(module).getVoidUndef();
						});
				}
				case METHOD_ISLIVE: {
					(void) args[0].resolveWithoutBind(function);
					return ConstantGenerator(module).getBool(true);
				}
				case METHOD_DESTROY:
				case METHOD_SETDEAD: {
					// Do nothing.
					(void) args[0].resolveWithoutBind(function);
					return ConstantGenerator(module).getVoidUndef();
				}
				case METHOD_ADDRESS: {
					auto methodOwner = RefMethodOwner::AsValue(function, type, args);
					
					return genRefPrimitiveMethodForVirtualCases(function, type,
						[&](llvm::Type* const llvmType) {
							const auto refValue = methodOwner.get(llvmType);
							if (llvmType->isPointerTy()) {
								return refValue;
							}
							
							// Load first member (which is 'this' pointer) from interface ref struct.
							return function.getBuilder().CreateExtractValue(refValue, { 0 });
						}
					);
				}
				default:
					llvm_unreachable("Unknown ref primitive method.");
			}
		}
		
	}
	
}

