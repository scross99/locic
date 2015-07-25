#ifndef LOCIC_CODEGEN_IREMITTER_HPP
#define LOCIC_CODEGEN_IREMITTER_HPP

namespace locic {
	
	class MethodID;
	
	namespace SEM {
		
		class Type;
		
	}
	
	namespace CodeGen {
		
		class Function;
		
		/**
		 * \brief LLVM IR Emitter
		 * 
		 * This class provides methods to emit LLVM IR for common
		 * operations.
		 */
		class IREmitter {
		public:
			IREmitter(Function& functionGenerator);
			
			llvm::Value*
			emitAlloca(const SEM::Type* type,
			           llvm::Value* hintResultValue = nullptr);
			
			llvm::Value*
			emitMoveLoad(llvm::Value* value, const SEM::Type* type);
			
			void
			emitMoveStore(llvm::Value* value,
			              llvm::Value* memDest,
			              const SEM::Type* type);
			
			llvm::Value*
			emitLoadDatatypeTag(llvm::Value* datatypePtr);
			
			void
			emitStoreDatatypeTag(llvm::Value* tagValue,
			                     llvm::Value* datatypePtr);
			
			llvm::Value*
			emitGetDatatypeVariantPtr(llvm::Value* datatypePtr,
			                          const SEM::Type* datatypeType,
			                          const SEM::Type* variantType);
			
			llvm::Value*
			emitImplicitCopyCall(llvm::Value* value,
			                     const SEM::Type* type,
			                     llvm::Value* hintResultValue = nullptr);
			
			llvm::Value*
			emitExplicitCopyCall(llvm::Value* value,
			                     const SEM::Type* type,
			                     llvm::Value* hintResultValue = nullptr);
			
			llvm::Value*
			emitCopyCall(MethodID methodID,
			             llvm::Value* value,
			             const SEM::Type* type,
			             llvm::Value* hintResultValue = nullptr);
			
		private:
			Function& functionGenerator_;
			
		};
		
	}
	
}

#endif