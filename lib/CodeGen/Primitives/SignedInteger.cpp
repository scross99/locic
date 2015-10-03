#include <assert.h>

#include <stdexcept>
#include <string>
#include <vector>

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
#include <locic/CodeGen/Liveness.hpp>
#include <locic/CodeGen/Memory.hpp>
#include <locic/CodeGen/Module.hpp>
#include <locic/CodeGen/Move.hpp>
#include <locic/CodeGen/Primitives.hpp>
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
		
		bool isFloatType(Module& module, const SEM::Type* const rawType);
		
		llvm::Value* callCastMethod(Function& function, llvm::Value* const castFromValue, const SEM::Type* const castFromType,
				MethodID methodID, const SEM::Type* const rawCastToType, llvm::Value* const hintResultValue);
		
		llvm::Value* genSignedIntegerPrimitiveMethodCall(Function& function, const SEM::Type* type, const MethodID methodID, const SEM::FunctionType functionType,
				llvm::ArrayRef<SEM::Value> templateArgs, PendingResultArray args, llvm::Value* const hintResultValue) {
			auto& module = function.module();
			auto& builder = function.getBuilder();
			
			const auto methodOwner = methodID.isConstructor() ? nullptr : args[0].resolveWithoutBind(function);
			
			const bool unsafe = module.buildOptions().unsafe;
			const size_t selfWidth = module.abi().typeSize(genABIType(module, type)) * 8;
			const auto selfType = TypeGenerator(module).getIntType(selfWidth);
			const auto zero = ConstantGenerator(module).getPrimitiveInt(type->primitiveID(), 0);
			const auto unit = ConstantGenerator(module).getPrimitiveInt(type->primitiveID(), 1);
			
			switch (methodID) {
				case METHOD_ALIGNMASK:
					return ConstantGenerator(module).getSizeTValue(module.abi().typeAlign(genABIType(module, type)) - 1);
				case METHOD_SIZEOF:
					return ConstantGenerator(module).getSizeTValue(module.abi().typeSize(genABIType(module, type)));
				case METHOD_IMPLICITCOPY:
				case METHOD_COPY:
					return methodOwner;
				case METHOD_MOVETO: {
					const auto moveToPtr = args[1].resolve(function);
					const auto moveToPosition = args[2].resolve(function);
					
					const auto destPtr = builder.CreateInBoundsGEP(moveToPtr, moveToPosition);
					const auto castedDestPtr = builder.CreatePointerCast(destPtr, genPointerType(module, type));
					
					genMoveStore(function, methodOwner, castedDestPtr, type);
					return ConstantGenerator(module).getVoidUndef();
				}
				case METHOD_CREATE:
					return zero;
				case METHOD_UNIT:
					return unit;
				case METHOD_SETDEAD:
					// Do nothing.
					return ConstantGenerator(module).getVoidUndef();
				case METHOD_IMPLICITCASTFROM:
				case METHOD_CASTFROM: {
					const auto argType = functionType.parameterTypes().front();
					const auto operand = args[0].resolve(function);
					if (isFloatType(module, argType)) {
						return builder.CreateFPToSI(operand, selfType);
					} else {
						return builder.CreateSExtOrTrunc(operand, selfType);
					}
				}
				case METHOD_IMPLICITCAST:
				case METHOD_CAST:
					return callCastMethod(function, methodOwner, type, methodID, templateArgs.front().typeRefType(), hintResultValue);
				case METHOD_PLUS:
					return methodOwner;
				case METHOD_MINUS:
					return builder.CreateNeg(methodOwner);
				case METHOD_ISZERO:
					return builder.CreateICmpEQ(methodOwner, zero);
				case METHOD_ISPOSITIVE:
					return builder.CreateICmpSGT(methodOwner, zero);
				case METHOD_ISNEGATIVE:
					return builder.CreateICmpSLT(methodOwner, zero);
				case METHOD_UNSIGNEDVALUE:
					return methodOwner;
				case METHOD_ABS: {
					// Generates: (value < 0) ? -value : value.
					const auto lessThanZero = builder.CreateICmpSLT(methodOwner, zero);
					return builder.CreateSelect(lessThanZero, builder.CreateNeg(methodOwner), methodOwner);
				}
				case METHOD_ADD: {
					const auto operand = args[1].resolveWithoutBind(function);
					if (unsafe) {
						return builder.CreateAdd(methodOwner,
						                         operand,
						                         /*name=*/"",
						                         /*hasNUW=*/false,
						                         /*hasNSW=*/true);
					} else {
						llvm::Value* const binaryArgs[] = { methodOwner, operand };
						return callArithmeticNoOverflowIntrinsic(function, llvm::Intrinsic::sadd_with_overflow, binaryArgs);
					}
				}
				case METHOD_SUBTRACT: {
					const auto operand = args[1].resolveWithoutBind(function);
					if (unsafe) {
						return builder.CreateSub(methodOwner,
						                         operand,
						                         /*name=*/"",
						                         /*hasNUW=*/false,
						                         /*hasNSW=*/true);
					} else {
						llvm::Value* const binaryArgs[] = { methodOwner, operand };
						return callArithmeticNoOverflowIntrinsic(function, llvm::Intrinsic::ssub_with_overflow, binaryArgs);
					}
				}
				case METHOD_MULTIPLY: {
					const auto operand = args[1].resolveWithoutBind(function);
					if (unsafe) {
						return builder.CreateMul(methodOwner,
						                         operand,
						                         /*name=*/"",
						                         /*hasNUW=*/false,
						                         /*hasNSW=*/true);
					} else {
						const auto checkDivBB = function.createBasicBlock("");
						const auto trapBB = function.createBasicBlock("");
						const auto endBB = function.createBasicBlock("");
						
						const auto mulResult = builder.CreateMul(methodOwner,
						                                         operand);
						
						// Check if methodOwner == 0.
						const auto methodOwnerIsZero = builder.CreateICmpEQ(methodOwner,
						                                                    zero);
						builder.CreateCondBr(methodOwnerIsZero,
						                     endBB,
						                     checkDivBB);
						
						// If methodOwner != 0, check (mulResult / methodOwner) == operand.
						function.selectBasicBlock(checkDivBB);
						const auto divResult = builder.CreateSDiv(mulResult,
						                                          methodOwner);
						const auto divResultIsOperand = builder.CreateICmpEQ(divResult,
						                                                     operand);
						builder.CreateCondBr(divResultIsOperand,
						                     endBB,
						                     trapBB);
						
						// (mulResult / methodOwner) != operand -> trap.
						function.selectBasicBlock(trapBB);
						callTrapIntrinsic(function);
						
						function.selectBasicBlock(endBB);
						return mulResult;
					}
				}
				case METHOD_DIVIDE: {
					const auto operand = args[1].resolveWithoutBind(function);
					if (!unsafe) {
						// TODO: also check for case of MIN_INT / -1 leading to overflow.
						const auto divisorIsZero = builder.CreateICmpEQ(operand, zero);
						const auto isZeroBB = function.createBasicBlock("isZero");
						const auto isNotZeroBB = function.createBasicBlock("isNotZero");
						builder.CreateCondBr(divisorIsZero, isZeroBB, isNotZeroBB);
						function.selectBasicBlock(isZeroBB);
						callTrapIntrinsic(function);
						function.selectBasicBlock(isNotZeroBB);
					}
					return builder.CreateSDiv(methodOwner, operand);
				}
				case METHOD_MODULO: {
					const auto operand = args[1].resolveWithoutBind(function);
					if (!unsafe) {
						const auto divisorIsZero = builder.CreateICmpEQ(operand, zero);
						const auto isZeroBB = function.createBasicBlock("isZero");
						const auto isNotZeroBB = function.createBasicBlock("isNotZero");
						builder.CreateCondBr(divisorIsZero, isZeroBB, isNotZeroBB);
						function.selectBasicBlock(isZeroBB);
						callTrapIntrinsic(function);
						function.selectBasicBlock(isNotZeroBB);
					}
					return builder.CreateSRem(methodOwner, operand);
				}
				case METHOD_EQUAL: {
					const auto operand = args[1].resolveWithoutBind(function);
					return builder.CreateICmpEQ(methodOwner, operand);
				}
				case METHOD_NOTEQUAL: {
					const auto operand = args[1].resolveWithoutBind(function);
					return builder.CreateICmpNE(methodOwner, operand);
				}
				case METHOD_LESSTHAN: {
					const auto operand = args[1].resolveWithoutBind(function);
					return builder.CreateICmpSLT(methodOwner, operand);
				}
				case METHOD_LESSTHANOREQUAL: {
					const auto operand = args[1].resolveWithoutBind(function);
					return builder.CreateICmpSLE(methodOwner, operand);
				}
				case METHOD_GREATERTHAN: {
					const auto operand = args[1].resolveWithoutBind(function);
					return builder.CreateICmpSGT(methodOwner, operand);
				}
				case METHOD_GREATERTHANOREQUAL: {
					const auto operand = args[1].resolveWithoutBind(function);
					return builder.CreateICmpSGE(methodOwner, operand);
				}
				case METHOD_COMPARE: {
					const auto operand = args[1].resolveWithoutBind(function);
					const auto isLessThan = builder.CreateICmpSLT(methodOwner, operand);
					const auto isGreaterThan = builder.CreateICmpSGT(methodOwner, operand);
					const auto minusOneResult = ConstantGenerator(module).getI8(-1);
					const auto zeroResult = ConstantGenerator(module).getI8(0);
					const auto plusOneResult = ConstantGenerator(module).getI8(1);
					return builder.CreateSelect(isLessThan, minusOneResult,
							builder.CreateSelect(isGreaterThan, plusOneResult, zeroResult));
				}
				default:
					llvm_unreachable("Unknown primitive method.");
			}
		}
		
	}
	
}

