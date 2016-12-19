#ifndef LOCIC_CODEGEN_CONTEXT_HPP
#define LOCIC_CODEGEN_CONTEXT_HPP

#include <memory>

namespace locic {
	
	class SharedMaps;
	
	namespace AST {
		
		class Context;
		
	}
	
	namespace CodeGen {
		
		class InternalContext;
		struct TargetOptions;
		
		class Context {
			public:
				Context(const AST::Context& semContext,
				        const SharedMaps& sharedMaps,
				        const TargetOptions& targetOptions);
				~Context();
				
				InternalContext& internal();
				
				const InternalContext& internal() const;
				
			private:
				std::unique_ptr<InternalContext> internalContext_;
				
		};
		
	}
	
}

#endif
