#ifndef LOCIC_CODEGEN_DEBUG_HPP
#define LOCIC_CODEGEN_DEBUG_HPP

#include <string>
#include <vector>

#include <locic/CodeGen/LLVMIncludes.hpp>
#include <locic/Debug.hpp>
#include <locic/Support/Name.hpp>
#include <locic/Support/Optional.hpp>

namespace locic {
	
	class PrimitiveID;
	
	namespace AST {
		
		class Function;
		class TypeInstance;
		class Value;
		
	}
	
	namespace CodeGen {
		
		struct DebugCompileUnit {
			std::string compilerName;
			std::string directoryName;
			std::string fileName;
			std::string flags;
		};
		
		class Function;
		class Module;
		
		class DebugBuilder {
			public:
				DebugBuilder(Module& module);
				~DebugBuilder();
				
				void finalize();
				
				void finalizeSubprogram(DISubprogram subprogram);
				
				DICompileUnit createCompileUnit(const DebugCompileUnit& compileUnit);
				
				DICompileUnit compileUnit() const;
				
				DIFile createFile(const std::string& path);
				
				DISubprogram createFunction(DIScope scope,
				                            unsigned int lineNumber,
				                            bool isInternal,
				                            bool isDefinition,
				                            const Name& name,
				                            DISubroutineType functionType,
				                            llvm::Function* function);
				
				DILocalVariable createVar(DIScope scope,
				                          bool isParam,
				                          const String& name,
				                          DIFile file,
				                          unsigned lineNumber,
				                          DIType type,
				                          size_t argIndex);
				
				DIType createUnspecifiedType(const String& name);
				
				DIType createVoidType();
				
				DIType createNullType();
				
				DIType createReferenceType(DIType type);
				
				DIType createPointerType(DIType type);
				
				DIType createIntType(PrimitiveID primitiveID);
				
				DIType createObjectType(DIFile file,
				                        unsigned int lineNumber,
				                        const Name& name,
				                        size_t sizeInBits,
				                        size_t alignInBits);
				
				DISubroutineType createFunctionType(const std::vector<LLVMMetadataValue*>& parameters);
				
				llvm::Instruction* insertVariableDeclare(Function& function,
				                                         DILocalVariable variable,
				                                         llvm::DebugLoc location,
				                                         llvm::Value* varValue);
				
			private:
				Module& module_;
				llvm::DIBuilder builder_;
			
		};
		
		std::pair<std::string, std::string> splitPath(const std::string& path);
		
		DISubprogram genDebugFunction(Module& module,
		                              const Debug::FunctionInfo& functionInfo,
		                              DISubroutineType functionType,
		                              llvm::Function* function,
		                              bool isInternal,
		                              bool isDefinition);
		
		Optional<DISubprogram> genDebugFunctionInfo(Module& module,
		                                            const AST::TypeInstance* parentType,
		                                            const AST::Function& function,
		                                            llvm::Function* llvmFunction);
		
		llvm::Instruction* genDebugVar(Function& function,
		                               const Debug::VarInfo& varInfo,
		                               DIType type,
		                               llvm::Value* varValue,
		                               size_t argIndex);
		
		llvm::DebugLoc getDebugLocation(Function& function,
		                                const Debug::SourceLocation& debugSourceLocation);
		
		Optional<llvm::DebugLoc> getFunctionDebugLocation(Function& function,
		                                                  const AST::Function& astFunction);
		
		Optional<llvm::DebugLoc> getValueDebugLocation(Function& function,
		                                               const AST::Value& value);
		
	}
	
}

#endif
