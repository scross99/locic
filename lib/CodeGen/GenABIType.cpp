#include <string>
#include <stdexcept>
#include <vector>

#include <llvm-abi/ABI.hpp>
#include <llvm-abi/ABITypeInfo.hpp>
#include <llvm-abi/Type.hpp>
#include <llvm-abi/TypeBuilder.hpp>

#include <locic/CodeGen/ArgInfo.hpp>
#include <locic/CodeGen/GenABIType.hpp>
#include <locic/CodeGen/Interface.hpp>
#include <locic/CodeGen/Liveness.hpp>
#include <locic/CodeGen/LivenessIndicator.hpp>
#include <locic/CodeGen/Mangling.hpp>
#include <locic/CodeGen/Module.hpp>
#include <locic/CodeGen/Primitives.hpp>
#include <locic/CodeGen/Support.hpp>
#include <locic/CodeGen/Template.hpp>

namespace locic {

	namespace CodeGen {
		
		llvm_abi::Type genABIArgType(Module& module, const SEM::Type* type) {
			if (canPassByValue(module, type)) {
				return genABIType(module, type);
			} else {
				return module.abiTypeBuilder().getPointerTy();
			}
		}
		
		llvm_abi::Type genABIType(Module& module, const SEM::Type* type) {
			auto& abiTypeBuilder = module.abiTypeBuilder();
			
			switch (type->kind()) {
				case SEM::Type::OBJECT: {
					const auto typeInstance = type->getObjectType();
					
					if (typeInstance->isPrimitive()) {
						return getPrimitiveABIType(module, type);
					} else {
						llvm::SmallVector<llvm_abi::Type, 8> members;
						
						if (typeInstance->isUnionDatatype()) {
							members.reserve(2);
							members.push_back(llvm_abi::Int8Ty);
							
							llvm_abi::Type maxStructType = abiTypeBuilder.getStructTy({});
							size_t maxStructSize = 0;
							size_t maxStructAlign = 0;
							
							for (auto variantTypeInstance: typeInstance->variants()) {
								const auto variantStructType = genABIType(module, variantTypeInstance->selfType());
								const auto& abiTypeInfo = module.abi().typeInfo();
								const auto variantStructSize = abiTypeInfo.getTypeStoreSize(variantStructType).asBytes();
								const auto variantStructAlign = abiTypeInfo.getTypePreferredAlign(variantStructType).asBytes();
								
								if (variantStructAlign > maxStructAlign || (variantStructAlign == maxStructAlign && variantStructSize > maxStructSize)) {
									maxStructType = variantStructType;
									maxStructSize = variantStructSize;
									maxStructAlign = variantStructAlign;
								} else {
									assert(variantStructAlign <= maxStructAlign && variantStructSize <= maxStructSize);
								}
							}
							
							members.push_back(maxStructType);
						} else {
							members.reserve(typeInstance->variables().size() + 1);
							
							for (const auto var: typeInstance->variables()) {
								members.push_back(genABIType(module, var->type()));
							}
							
							const auto livenessIndicator = getLivenessIndicator(module, *typeInstance);
							if (livenessIndicator.isSuffixByte()) {
								// Add suffix byte.
								members.push_back(llvm_abi::Int8Ty);
							}
						}
						
						return abiTypeBuilder.getStructTy(members);
					}
				}
				case SEM::Type::ALIAS: {
					return genABIType(module, type->resolveAliases());
				}
				default: {
					llvm_unreachable("Unknown type kind for generating ABI type.");
				}
			}
		}
		
	}
	
}

