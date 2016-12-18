#ifndef LOCIC_CODEGEN_CODEGENERATOR_HPP
#define LOCIC_CODEGEN_CODEGENERATOR_HPP

#include <cstddef>
#include <memory>
#include <string>

namespace locic {
	
	namespace AST {
		
		class Namespace;
		
	}
	
	namespace Debug {
		
		class Module;
		
	}
	
	namespace CodeGen {
		
		class BuildOptions;
		class Context;
		class Module;
		class ModulePtr;
		
		class CodeGenerator {
			public:
				CodeGenerator(Context& context, const std::string& moduleName, Debug::Module& debugModule, const BuildOptions& buildOptions);
				~CodeGenerator();
				
				Module& module();
				
				ModulePtr releaseModule();
				
				void applyOptimisations(size_t optLevel);
				
				void genNamespace(AST::Namespace* nameSpace);
				
				void writeToFile(const std::string& fileName);
				
				void dumpToFile(const std::string& fileName);
				
				void dump();
				
			private:
				std::unique_ptr<Module> module_;
				
		};
		
	}
	
}

#endif
