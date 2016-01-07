#include <locic/AST.hpp>
#include <locic/Debug/SourceLocation.hpp>
#include <locic/Debug/SourcePosition.hpp>
#include <locic/Parser/TokenReader.hpp>
#include <locic/Parser/TypeInstanceBuilder.hpp>

namespace locic {
	
	namespace Parser {
		
		TypeInstanceBuilder::TypeInstanceBuilder(const TokenReader& reader)
		: reader_(reader) { }
		
		TypeInstanceBuilder::~TypeInstanceBuilder() { }
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeTypeInstanceNode(AST::TypeInstance* const typeInstance,
		                                          const Debug::SourcePosition& start) {
			const auto location = reader_.locationWithRangeFrom(start);
			return AST::makeNode(location, typeInstance);
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeClassDecl(String name, AST::Node<AST::FunctionList> methods,
		                                   const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::ClassDecl(name, methods), start);
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeClassDef(String name, AST::Node<AST::TypeVarList> variables,
		                                  AST::Node<AST::FunctionList> methods,
		                                  const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::ClassDef(name, variables,
			                                                        methods), start);
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeDatatype(String name, AST::Node<AST::TypeVarList> variables,
		                                  const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::Datatype(name, variables), start);
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeUnionDatatype(String name, AST::Node<AST::TypeInstanceList> variants,
		                                       const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::UnionDatatype(name, variants), start);
		}
		
		AST::Node<AST::TypeInstanceList>
		TypeInstanceBuilder::makeTypeInstanceList(AST::TypeInstanceList list,
		                                          const Debug::SourcePosition& start) {
			const auto location = reader_.locationWithRangeFrom(start);
			return AST::makeNode(location, new AST::TypeInstanceList(std::move(list)));
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeEnum(String name, AST::Node<AST::StringList> constructorList,
		                              const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::Enum(name, constructorList), start);
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeException(String name, AST::Node<AST::TypeVarList> variables,
		                                   AST::Node<AST::ExceptionInitializer> initializer,
		                                   const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::Exception(name, variables,
			                                                         initializer), start);
		}
		
		AST::Node<AST::ExceptionInitializer>
		TypeInstanceBuilder::makeNoneExceptionInitializer(const Debug::SourcePosition& start) {
			const auto location = reader_.locationWithRangeFrom(start);
			return AST::makeNode(location, AST::ExceptionInitializer::None());
		}
		
		AST::Node<AST::ExceptionInitializer>
		TypeInstanceBuilder::makeExceptionInitializer(AST::Node<AST::Symbol> symbol,
		                                              AST::Node<AST::ValueList> valueList,
		                                              const Debug::SourcePosition& start) {
			const auto location = reader_.locationWithRangeFrom(start);
			return AST::makeNode(location, AST::ExceptionInitializer::Initialize(symbol, valueList));
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeInterface(String name, AST::Node<AST::FunctionList> methods,
		                                   const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::Interface(name, methods), start);
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makePrimitive(String name, AST::Node<AST::FunctionList> methods,
		                                   const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::Primitive(name, methods), start);
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeOpaqueStruct(String name, const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::OpaqueStruct(name), start);
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeStruct(String name, AST::Node<AST::TypeVarList> variables,
		                                const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::Struct(name, variables), start);
		}
		
		AST::Node<AST::TypeInstance>
		TypeInstanceBuilder::makeUnion(String name, AST::Node<AST::TypeVarList> variables,
		                               const Debug::SourcePosition& start) {
			return makeTypeInstanceNode(AST::TypeInstance::Union(name, variables), start);
		}
		
		AST::Node<AST::FunctionList>
		TypeInstanceBuilder::makeFunctionList(AST::FunctionList functionList,
		                                      const Debug::SourcePosition& start) {
			const auto location = reader_.locationWithRangeFrom(start);
			return AST::makeNode(location, new AST::FunctionList(std::move(functionList)));
		}
		
	}
	
}
