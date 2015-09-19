#include <assert.h>

#include <string>

#include <locic/CodeGen/ArgInfo.hpp>
#include <locic/CodeGen/Destructor.hpp>
#include <locic/CodeGen/Function.hpp>
#include <locic/CodeGen/GenType.hpp>
#include <locic/CodeGen/Module.hpp>
#include <locic/CodeGen/Primitives.hpp>
#include <locic/CodeGen/SizeOf.hpp>
#include <locic/CodeGen/TypeGenerator.hpp>
#include <locic/CodeGen/TypeSizeKnowledge.hpp>
#include <locic/CodeGen/UnwindAction.hpp>

namespace locic {

	namespace CodeGen {
	
		void genPrimitiveDestructorCall(Function& function, const SEM::Type* const type, llvm::Value* value) {
			assert(value->getType()->isPointerTy());
			
			const auto id = type->primitiveID();
			if (id == PrimitiveValueLval || id == PrimitiveFinalLval) {
				const auto targetType = type->templateArguments().front().typeRefType();
				genDestructorCall(function, targetType, value);
			}
		}
		
		bool primitiveTypeHasDestructor(Module& module, const SEM::Type* const type) {
			const auto id = type->primitiveID();
			return (id == PrimitiveValueLval || id == PrimitiveFinalLval) && typeHasDestructor(module, type->templateArguments().front().typeRefType());
		}
		
		bool primitiveTypeInstanceHasDestructor(Module& /*module*/, const SEM::TypeInstance* const typeInstance) {
			const auto id = typeInstance->primitiveID();
			return (id == PrimitiveValueLval || id == PrimitiveFinalLval);
		}
		
	}
	
}

