#ifndef LOCIC_CODEGEN_VIRTUALCALL_GENERICVIRTUALCALLABI_HPP
#define LOCIC_CODEGEN_VIRTUALCALL_GENERICVIRTUALCALLABI_HPP

#include <locic/CodeGen/VirtualCallABI.hpp>

namespace locic {
	
	namespace SEM {
		
		class FunctionType;
		class TypeInstance;
		
	}
	
	namespace CodeGen {
		
		class IREmitter;
		class Module;
		struct VirtualMethodComponents;
		
		/**
		 * \brief Generic Virtual Call ABI
		 * 
		 * This is a 'generic' virtual call mechanism which passes the
		 * call arguments on the stack. It is therefore less efficient
		 * but easy to implement.
		 */
		class GenericVirtualCallABI: public VirtualCallABI {
		public:
			GenericVirtualCallABI(Module& module);
			~GenericVirtualCallABI();
			
			ArgInfo
			getStubArgInfo();
			
			llvm::AttributeSet
			conflictResolutionStubAttributes(const llvm::AttributeSet& existingAttributes);
			
			llvm::Value*
			makeArgsStruct(IREmitter& irEmitter,
			               llvm::ArrayRef<const SEM::Type*> argTypes,
			               llvm::ArrayRef<llvm::Value*> args);
			
			llvm::Constant*
			emitVTableSlot(const SEM::TypeInstance& typeInstance,
			               llvm::ArrayRef<SEM::Function*> methods);
		
			void
			emitCallWithReturnVar(IREmitter& irEmitter,
			                      const SEM::FunctionType functionType,
			                      llvm::Value* returnVarPointer,
			                      VirtualMethodComponents methodComponents,
			                      llvm::ArrayRef<llvm::Value*> args);
			
			llvm::Value*
			emitCall(IREmitter& irEmitter,
			         SEM::FunctionType functionType,
			         VirtualMethodComponents methodComponents,
			         llvm::ArrayRef<llvm::Value*> args);
			
			llvm::Value*
			emitCountFnCall(IREmitter& irEmitter,
			                llvm::Value* typeInfoValue,
			                CountFnKind kind);
			
			ArgInfo
			virtualMoveArgInfo();
			
			void
			emitMoveCall(IREmitter& irEmitter,
			             llvm::Value* typeInfoValue,
			             llvm::Value* sourceValue,
			             llvm::Value* destValue,
			             llvm::Value* positionValue);
			
			ArgInfo
			virtualDestructorArgInfo();
			
			void
			emitDestructorCall(IREmitter& irEmitter,
			                   llvm::Value* typeInfoValue,
			                   llvm::Value* objectValue);
			
		private:
			Module& module_;
			
		};
		
	}
	
}

#endif