#include <string>
#include <vector>

#include <locic/Support/Name.hpp>

#include <locic/AST/Function.hpp>
#include <locic/AST/Node.hpp>
#include <locic/AST/RequireSpecifier.hpp>
#include <locic/AST/Scope.hpp>
#include <locic/AST/TypeDecl.hpp>
#include <locic/AST/Var.hpp>

namespace locic {

	namespace AST {
		
		Function::Function() :
		isAutoGenerated_(false), isMethod_(false),
		isStatic_(false), isImported_(false), isExported_(false),
		isPrimitive_(false), isVarArg_(false),
		templateVariableDecls_(makeDefaultNode<TemplateVarList>()),
		constPredicate_(Predicate::False()),
		requiresPredicate_(Predicate::True()),
		moduleScope_(ModuleScope::Internal()) { }
		
		Function::~Function() { }
		
		GlobalStructure& Function::parent() {
			return *parent_;
		}
		
		const GlobalStructure& Function::parent() const {
			return *parent_;
		}
		
		void Function::setParent(GlobalStructure argParent) {
			parent_ = make_optional(std::move(argParent));
		}
		
		Namespace& Function::nameSpace() {
			return parent().nextNamespace();
		}
		
		const Namespace& Function::nameSpace() const {
			return parent().nextNamespace();
		}
		
		const ModuleScope& Function::moduleScope() const {
			return moduleScope_;
		}
		
		void Function::setModuleScope(ModuleScope argModuleScope) {
			moduleScope_ = std::move(argModuleScope);
		}
		
		bool Function::isAutoGenerated() const {
			return isAutoGenerated_;
		}
		
		void Function::setAutoGenerated(const bool value) {
			 assert(!(value && hasScope()) &&
			        "Functions can't be marked auto-generated and "
				"have user-specified code.");
			isAutoGenerated_ = value;
		}
		
		bool Function::isStatic() const {
			return isStatic_;
		}
		
		void Function::setIsStatic(const bool value) {
			isStatic_ = value;
		}
		
		bool Function::isVarArg() const {
			return isVarArg_;
		}
		
		void Function::setIsVarArg(const bool value) {
			isVarArg_ = value;
		}
		
		bool Function::isImported() const {
			return isImported_;
		}
		
		void Function::setIsImported(const bool value) {
			isImported_ = value;
		}
		
		bool Function::isExported() const {
			return isExported_;
		}
		
		void Function::setIsExported(const bool value) {
			isExported_ = value;
		}
		
		bool Function::isPrimitive() const {
			return isPrimitive_;
		}
		
		void Function::setIsPrimitive(const bool value) {
			isPrimitive_ = value;
		}
		
		bool Function::isMethod() const {
			return isMethod_;
		}
		
		void Function::setMethod(const bool value) {
			isMethod_ = value;
		}
		
		bool Function::isStaticMethod() const {
			return isMethod() && isStatic();
		}
		
		const Node<Name>& Function::nameDecl() const {
			return nameDecl_;
		}
		
		void Function::setNameDecl(Node<Name> pNameDecl) {
			nameDecl_ = std::move(pNameDecl);
		}
		
		const Name& Function::fullName() const {
			assert(!fullName_.empty());
			return fullName_;
		}
		
		void Function::setFullName(Name pFullName) {
			fullName_ = std::move(pFullName);
		}
		
		String Function::canonicalName() const {
			return CanonicalizeMethodName(fullName().last());
		}
		
		void Function::setType(const FunctionType pType) {
			type_ = pType;
		}
		
		const FunctionType& Function::type() const {
			return type_;
		}
		
		Node<TypeDecl>& Function::returnType() {
			assert(!isAutoGenerated());
			return returnType_;
		}
		
		const Node<TypeDecl>& Function::returnType() const {
			assert(!isAutoGenerated());
			return returnType_;
		}
		
		void Function::setReturnType(Node<TypeDecl> pReturnType) {
			returnType_ = std::move(pReturnType);
		}
		
		const Node<VarList>& Function::parameterDecls() const {
			assert(!isAutoGenerated());
			return parameterDecls_;
		}
		
		void Function::setParameterDecls(Node<VarList> pParameterDecls) {
			parameterDecls_ = std::move(pParameterDecls);
		}
		
		const std::vector<Var*>& Function::parameters() const {
			return parameters_;
		}
		
		void Function::setParameters(std::vector<Var*> pParameters) {
			parameters_ = std::move(pParameters);
		}
		
		bool Function::hasScope() const {
			return scope_.get() != nullptr;
		}
		
		Node<Scope>& Function::scope() {
			assert(hasScope());
			assert(!isAutoGenerated() &&
			       "Functions can't be marked auto-generated and "
			       "have user-specified code.");
			return scope_;
		}
		
		const Node<Scope>& Function::scope() const {
			assert(hasScope());
			assert(!isAutoGenerated() &&
			       "Functions can't be marked auto-generated and "
			       "have user-specified code.");
			return scope_;
		}
		
		void Function::setScope(Node<Scope> newScope) {
			assert(!hasScope());
			assert(newScope.get() != nullptr);
			assert(!isAutoGenerated() &&
			       "Functions can't be marked auto-generated and "
			       "have user-specified code.");
			scope_ = std::move(newScope);
		}
		
		const Node<ConstSpecifier>& Function::constSpecifier() const {
			return constSpecifier_;
		}
		
		void Function::setConstSpecifier(Node<ConstSpecifier> pConstSpecifier) {
			constSpecifier_ = std::move(pConstSpecifier);
		}
		
		const Predicate& Function::constPredicate() const {
			return constPredicate_;
		}
		
		void Function::setConstPredicate(Predicate predicate) {
			constPredicate_ = std::move(predicate);
		}
		
		const Node<RequireSpecifier>& Function::noexceptSpecifier() const {
			return noexceptSpecifier_;
		}
		
		void Function::setNoexceptSpecifier(Node<RequireSpecifier> pNoexceptSpecifier) {
			noexceptSpecifier_ = std::move(pNoexceptSpecifier);
		}
		
		const Predicate& Function::noexceptPredicate() const {
			return type().attributes().noExceptPredicate();
		}
		
		const Node<RequireSpecifier>& Function::requireSpecifier() const {
			return requireSpecifier_;
		}
		
		void Function::setRequireSpecifier(Node<RequireSpecifier> pRequireSpecifier) {
			requireSpecifier_ = std::move(pRequireSpecifier);
		}
		
		const Predicate& Function::requiresPredicate() const {
			return requiresPredicate_;
		}
		
		void Function::setRequiresPredicate(Predicate predicate) {
			requiresPredicate_ = std::move(predicate);
		}
		
		const Node<TemplateVarList>& Function::templateVariableDecls() const {
			return templateVariableDecls_;
		}
		
		void Function::setTemplateVariableDecls(Node<TemplateVarList> pTemplateVariables) {
			templateVariableDecls_ = std::move(pTemplateVariables);
		}
		
		TemplateVarArray& Function::templateVariables() {
			return templateVariables_;
		}
		
		const TemplateVarArray& Function::templateVariables() const {
			return templateVariables_;
		}
		
		FastMap<String, TemplateVar*>& Function::namedTemplateVariables() {
			return namedTemplateVariables_;
		}
		
		const FastMap<String, TemplateVar*>& Function::namedTemplateVariables() const {
			return namedTemplateVariables_;
		}
		
		FastMap<String, Var*>& Function::namedVariables() {
			return namedVariables_;
		}
		
		const FastMap<String, Var*>& Function::namedVariables() const {
			return namedVariables_;
		}
		
		void Function::setDebugInfo(Debug::FunctionInfo newDebugInfo) {
			debugInfo_ = make_optional(std::move(newDebugInfo));
		}
		
		const Optional<Debug::FunctionInfo>& Function::debugInfo() const {
			return debugInfo_;
		}
		
		std::string Function::toString() const {
			// TODO!
			return "FunctionDecl";
		}
		
	}
	
}

