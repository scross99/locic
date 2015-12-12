#include <locic/CodeGen/Context.hpp>
#include <locic/CodeGen/InternalContext.hpp>

namespace locic {
	
	namespace CodeGen {
		
		Context::Context(const SEM::Context& semContext,
		                 const SharedMaps& sharedMaps,
		                 const TargetOptions& targetOptions)
		: internalContext_(new InternalContext(semContext, sharedMaps, targetOptions)) { }
		
		Context::~Context() { }
		
		InternalContext& Context::internal() {
			return *internalContext_;
		}
		
		const InternalContext& Context::internal() const {
			return *internalContext_;
		}
		
	}
	
}

