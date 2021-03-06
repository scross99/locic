#ifndef LOCIC_CODEGEN_SIZEOF_HPP
#define LOCIC_CODEGEN_SIZEOF_HPP



#include <locic/CodeGen/ArgInfo.hpp>
#include <locic/CodeGen/Function.hpp>
#include <locic/CodeGen/Module.hpp>

namespace locic {

	namespace CodeGen {
	
		ArgInfo alignMaskArgInfo(Module& module, const AST::TypeInstance* typeInstance);
		
		ArgInfo sizeOfArgInfo(Module& module, const AST::TypeInstance* typeInstance);
		
		ArgInfo memberOffsetArgInfo(Module& module, const AST::TypeInstance* typeInstance);
		
		llvm::Function* genAlignMaskFunctionDecl(Module& module, const AST::TypeInstance* typeInstance);
		
		llvm::Value* genAlignOf(Function& function, const AST::Type* type);
		
		llvm::Value* genAlignMask(Function& function, const AST::Type* type);
		
		llvm::Function* genSizeOfFunctionDecl(Module& module, const AST::TypeInstance* typeInstance);
		
		llvm::Value* genSizeOf(Function& function, const AST::Type* type);
		
		llvm::Value* makeAligned(Function& function, llvm::Value* size, llvm::Value* alignMask);
		
		llvm::Value* genSuffixByteOffset(Function& function, const AST::TypeInstance& typeInstance);
		
		llvm::Value* genMemberOffset(Function& function, const AST::Type* type, size_t memberIndex);
		
		llvm::Value* genMemberPtr(Function& function, llvm::Value* objectPointer, const AST::Type* objectType, size_t memberIndex);
		
		std::pair<llvm::Value*, llvm::Value*>
		getVariantPointers(Function& function, const AST::Type* type,
		                   llvm::Value* objectPointer);
		
	}
	
}

#endif
