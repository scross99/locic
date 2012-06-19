#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Basic/TargetOptions.h>
#include <clang/Basic/TargetInfo.h>

#include <assert.h>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <Locic/SEM.hpp>
#include <Locic/CodeGen/CodeGen.hpp>

using namespace llvm;

class CodeGen {
	private:
		std::string name_;
		Module* module_;
		IRBuilder<> builder_;
		FunctionType* currentFunctionType_;
		Function* currentFunction_;
		BasicBlock* currentBasicBlock_;
		FunctionPassManager fpm_;
		std::map<SEM::TypeInstance*, StructType*> structs_;
		std::map<SEM::FunctionDecl*, Function*> functions_;
		std::map<std::size_t, AllocaInst*> localVariables_, paramVariables_;
		clang::TargetInfo* targetInfo_;
		
	public:
		CodeGen(const std::string& moduleName)
			: name_(moduleName),
			  module_(new Module(name_.c_str(), getGlobalContext())),
			  builder_(getGlobalContext()),
			  fpm_(module_),
			  targetInfo_(0){
			  
			InitializeNativeTarget();
			
			std::cout << "Default target triple: " << sys::getHostTriple() << std::endl;
			
			module_->setTargetTriple(sys::getHostTriple());
			
			std::string error;
			const Target* target = TargetRegistry::lookupTarget(sys::getHostTriple(), error);
			
			if(target != NULL) {
				std::cout << "Target: name=" << target->getName() << ", description=" << target->getShortDescription() << std::endl;
				
				std::cout << "--Does " << (target->hasJIT() ? "" : "not ") << "support just-in-time compilation." << std::endl;
				std::cout << "--Does " << (target->hasTargetMachine() ? "" : "not ") << "support code generation." << std::endl;
				std::cout << "--Does " << (target->hasMCAsmBackend() ? "" : "not ") << "support .o generation." << std::endl;
				std::cout << "--Does " << (target->hasMCAsmLexer() ? "" : "not ") << "support .s lexing." << std::endl;
				std::cout << "--Does " << (target->hasMCAsmParser() ? "" : "not ") << "support .s parsing." << std::endl;
				std::cout << "--Does " << (target->hasAsmPrinter() ? "" : "not ") << "support .s printing." << std::endl;
				std::cout << "--Does " << (target->hasMCDisassembler() ? "" : "not ") << "support disassembling." << std::endl;
				std::cout << "--Does " << (target->hasMCInstPrinter() ? "" : "not ") << "support printing instructions." << std::endl;
				std::cout << "--Does " << (target->hasMCCodeEmitter() ? "" : "not ") << "support instruction encoding." << std::endl;
				std::cout << "--Does " << (target->hasMCObjectStreamer() ? "" : "not ") << "support streaming to files." << std::endl;
				std::cout << "--Does " << (target->hasAsmStreamer() ? "" : "not ") << "support streaming ASM to files." << std::endl;
				
				if(target->hasTargetMachine()) {
					std::auto_ptr<TargetMachine> targetMachine(target->createTargetMachine(sys::getHostTriple(), "", ""));
					const TargetData* targetData = targetMachine->getTargetData();
					
					if(targetData != 0) {
						std::cout << "--Pointer size = " << targetData->getPointerSize() << std::endl;
						std::cout << "--Pointer size (in bits) = " << targetData->getPointerSizeInBits() << std::endl;
						std::cout << "--Little endian = " << (targetData->isLittleEndian() ? "true" : "false") << std::endl;
						std::cout << "--Big endian = " << (targetData->isBigEndian() ? "true" : "false") << std::endl;
						std::cout << "--Legal integer sizes = {";
						
						bool b = false;
						
						for(unsigned int i = 0; i < 1000; i++) {
							if(targetData->isLegalInteger(i)) {
								if(b) {
									std::cout << ", ";
								}
								
								std::cout << i;
								b = true;
							}
						}
						
						std::cout << "}" << std::endl;
						std::cout << std::endl;
						
						clang::CompilerInstance ci;
						ci.createDiagnostics(0, NULL);
						
						clang::TargetOptions to;
						to.Triple = sys::getHostTriple();
						targetInfo_ = clang::TargetInfo::CreateTargetInfo(ci.getDiagnostics(), to);
						
						std::cout << "Information from Clang:" << std::endl;
						std::cout << "--Short width: " << targetInfo_->getShortWidth() << std::endl;
						std::cout << "--Int width: " << targetInfo_->getIntWidth() << std::endl;
						std::cout << "--Long width: " << targetInfo_->getLongWidth() << std::endl;
						std::cout << "--Long long width: " << targetInfo_->getLongLongWidth() << std::endl;
						std::cout << "--Float width: " << targetInfo_->getFloatWidth() << std::endl;
						std::cout << "--Double width: " << targetInfo_->getDoubleWidth() << std::endl;
						std::cout << std::endl;
					}
				}
			} else {
				std::cout << "Error when looking up default target: " << error << std::endl;
			}
			
			// Set up the optimizer pipeline.
			// Provide basic AliasAnalysis support for GVN.
			fpm_.add(createBasicAliasAnalysisPass());
			// Promote allocas to registers.
			fpm_.add(createPromoteMemoryToRegisterPass());
			// Do simple "peephole" optimizations and bit-twiddling optzns.
			fpm_.add(createInstructionCombiningPass());
			// Reassociate expressions.
			fpm_.add(createReassociatePass());
			// Eliminate Common SubExpressions.
			fpm_.add(createGVNPass());
			// Simplify the control flow graph (deleting unreachable blocks, etc).
			fpm_.add(createCFGSimplificationPass());
			
			fpm_.doInitialization();
		}
		
		~CodeGen() {
			delete module_;
		}
		
		void dump() {
			module_->dump();
		}
		
		void genFile(SEM::Module* module) {
			assert(module != NULL);
			
			std::map<std::string, SEM::TypeInstance *>::const_iterator typeIt;
			for(typeIt = module->typeInstances.begin(); typeIt != module->typeInstances.end(); ++typeIt){
				SEM::TypeInstance* typeInstance = typeIt->second;
				assert(typeInstance != NULL);
				
				switch(typeInstance->typeEnum) {
					case SEM::TypeInstance::STRUCT: {
						std::vector<Type*> structMembers;
						structMembers.push_back(Type::getInt1Ty(getGlobalContext()));
						structs_[typeInstance] = StructType::create(structMembers, typeInstance->name);
						break;
					}
					default: {
						std::cerr << "Unimplemented type with name '" << typeInstance->name << "'." << std::endl;
					}
				}
			}
			
			std::map<std::string, SEM::FunctionDecl *>::const_iterator declIt;
			for(declIt = module->functionDeclarations.begin(); declIt != module->functionDeclarations.end(); ++declIt){
				SEM::FunctionDecl* decl = declIt->second;
				assert(decl != NULL);
				functions_[decl] = Function::Create(genFunctionType(decl->type), Function::ExternalLinkage, decl->name, module_);
			}
			
			std::list<SEM::FunctionDef *>::const_iterator defIt;
			for(defIt = module->functionDefinitions.begin(); defIt != module->functionDefinitions.end(); ++defIt){
				genFunctionDef(*defIt);
			}
		}
		
		FunctionType* genFunctionType(SEM::Type* type) {
			assert(type != NULL);
			assert(type->typeEnum == SEM::Type::FUNCTION);
			
			Type* returnType = genType(type->functionType.returnType);
			
			std::vector<Type*> paramTypes;
			
			const std::list<SEM::Type *>& params = type->functionType.parameterTypes;
			std::list<SEM::Type *>::const_iterator it;
			
			for(it = params.begin(); it != params.end(); ++it) {
				paramTypes.push_back(genType(*it));
			}
			
			bool isVarArg = false;
			return FunctionType::get(returnType, paramTypes, isVarArg);
		}
		
		Type* genType(SEM::Type* type) {
			switch(type->typeEnum) {
				case SEM::Type::VOID: {
					return Type::getVoidTy(getGlobalContext());
				}
				case SEM::Type::NULLT: {
					return PointerType::getUnqual(Type::getInt8Ty(getGlobalContext()));
				}
				case SEM::Type::BASIC: {
					switch(type->basicType.typeEnum) {
						case SEM::Type::BasicType::INTEGER:
							return IntegerType::get(getGlobalContext(), targetInfo_->getLongWidth());
						case SEM::Type::BasicType::BOOLEAN:
							return Type::getInt1Ty(getGlobalContext());
						case SEM::Type::BasicType::FLOAT:
							return Type::getFloatTy(getGlobalContext());
						default:
							std::cerr << "CodeGen error: Unknown basic type." << std::endl;
							return Type::getVoidTy(getGlobalContext());
							
					}
				}
				case SEM::Type::NAMED: {
					SEM::TypeInstance* typeInstance = type->namedType.typeInstance;
					
					if(typeInstance->typeEnum == SEM::TypeInstance::STRUCT) {
						StructType* structType = structs_[typeInstance];
						assert(structType != NULL);
						return structType;
					} else {
						std::cerr << "CodeGen error: Named type not implemented." << std::endl;
						return Type::getVoidTy(getGlobalContext());
					}
				}
				case SEM::Type::POINTER: {
					Type* pointerType = genType(type->pointerType.targetType);
					
					if(pointerType->isVoidTy()) {
						// LLVM doesn't support 'void *' => use 'int8_t *' instead.
						return PointerType::getUnqual(Type::getInt8Ty(getGlobalContext()));
					} else {
						return pointerType->getPointerTo();
					}
				}
				case SEM::Type::FUNCTION: {
					return genFunctionType(type)->getPointerTo();
				}
				default: {
					std::cerr << "CodeGen error: Unknown type." << std::endl;
					return Type::getVoidTy(getGlobalContext());
				}
			}
		}
		
		void genFunctionDef(SEM::FunctionDef* functionDef) {
			// Create function.
			currentFunction_ = functions_[functionDef->declaration];
			assert(currentFunction_ != NULL);
			
			currentBasicBlock_ = BasicBlock::Create(getGlobalContext(), "entry", currentFunction_);
			builder_.SetInsertPoint(currentBasicBlock_);
			
			// Store arguments onto stack.
			Function::arg_iterator arg;
			
			const std::list<SEM::Var *>& parameterVars = functionDef->declaration->parameters;
			std::list<SEM::Var *>::const_iterator it;
			
			for(it = parameterVars.begin(), arg = currentFunction_->arg_begin(); it != parameterVars.end(); ++arg, ++it) {
				SEM::Var* paramVar = *it;
				
				// Create an alloca for this variable.
				AllocaInst* stackObject = builder_.CreateAlloca(genType(paramVar->type));
				
				paramVariables_[paramVar->id] = stackObject;
				
				// Store the initial value into the alloca.
				builder_.CreateStore(arg, stackObject);
			}
			
			genScope(functionDef->scope);
			
			// Need to terminate the final basic block.
			// (just make it loop to itself - this will
			// be removed by dead code elimination)
			builder_.CreateBr(builder_.GetInsertBlock());
			
			std::cout << "---Before verification:" << std::endl;
			
			module_->dump();
			
			// Check the generated function is correct.
			verifyFunction(*currentFunction_);
			
			std::cout << "---Before optimisation:" << std::endl;
			
			module_->dump();
			
			std::cout << "---Running optimisations..." << std::endl;
			
			// Run optimisations.
			fpm_.run(*currentFunction_);
			
			paramVariables_.clear();
			localVariables_.clear();
		}
		
		void genScope(SEM::Scope* scope) {
			for(std::size_t i = 0; i < scope->localVariables.size(); i++) {
				SEM::Var* localVar = scope->localVariables.at(i);
				
				// Create an alloca for this variable.
				AllocaInst* stackObject = builder_.CreateAlloca(genType(localVar->type));
				
				localVariables_[localVar->id] = stackObject;
			}
			
			std::list<SEM::Statement *>::const_iterator it;			
			for(it = scope->statementList.begin(); it != scope->statementList.end(); ++it) {
				genStatement(*it);
			}
		}
		
		void genStatement(SEM::Statement* statement) {
			switch(statement->typeEnum) {
				case SEM::Statement::VALUE: {
					genValue(statement->valueStmt.value);
					break;
				}
				case SEM::Statement::SCOPE: {
					genScope(statement->scopeStmt.scope);
					break;
				}
				case SEM::Statement::IF: {
					BasicBlock* thenBB = BasicBlock::Create(getGlobalContext(), "then", currentFunction_);
					BasicBlock* elseBB = BasicBlock::Create(getGlobalContext(), "else");
					BasicBlock* mergeBB = BasicBlock::Create(getGlobalContext(), "ifmerge");
					
					builder_.CreateCondBr(genValue(statement->ifStmt.condition), thenBB, elseBB);
					
					// Create 'then'.
					builder_.SetInsertPoint(thenBB);
					
					genScope(statement->ifStmt.ifTrue);
					
					builder_.CreateBr(mergeBB);
					
					// Create 'else'.
					currentFunction_->getBasicBlockList().push_back(elseBB);
					builder_.SetInsertPoint(elseBB);
					
					if(statement->ifStmt.ifFalse != NULL) {
						genScope(statement->ifStmt.ifFalse);
					}
					
					builder_.CreateBr(mergeBB);
					
					// Create merge.
					currentFunction_->getBasicBlockList().push_back(mergeBB);
					builder_.SetInsertPoint(mergeBB);
					break;
				}
				case SEM::Statement::WHILE: {
					BasicBlock* insideLoopBB = BasicBlock::Create(getGlobalContext(), "insideLoop", currentFunction_);
					BasicBlock* afterLoopBB = BasicBlock::Create(getGlobalContext(), "afterLoop");
					
					builder_.CreateCondBr(genValue(statement->whileStmt.condition), insideLoopBB, afterLoopBB);
					
					// Create loop contents.
					builder_.SetInsertPoint(insideLoopBB);
					
					genScope(statement->whileStmt.whileTrue);
					
					builder_.CreateCondBr(genValue(statement->whileStmt.condition), insideLoopBB, afterLoopBB);
					
					// Create 'else'.
					currentFunction_->getBasicBlockList().push_back(afterLoopBB);
					builder_.SetInsertPoint(afterLoopBB);
					break;
				}
				case SEM::Statement::ASSIGN: {
					SEM::Value* lValue = statement->assignStmt.lValue;
					SEM::Value* rValue = statement->assignStmt.rValue;
					
					builder_.CreateStore(genValue(rValue), genValue(lValue, true));
					break;
				}
				case SEM::Statement::RETURN: {
					if(statement->returnStmt.value != NULL) {
						builder_.CreateRet(genValue(statement->returnStmt.value));
					} else {
						builder_.CreateRetVoid();
					}
					
					// Need a basic block after a return statement in case anything more is generated.
					// This (and any following code) will be removed by dead code elimination.
					builder_.SetInsertPoint(BasicBlock::Create(getGlobalContext(), "next", currentFunction_));
					break;
				}
				default:
					std::cerr << "CodeGen error: Unknown statement." << std::endl;
			}
		}
		
		Value* genValue(SEM::Value* value, bool genLValue = false) {
			switch(value->typeEnum) {
				case SEM::Value::CONSTANT: {
					switch(value->constant.typeEnum) {
						case SEM::Value::Constant::BOOLEAN:
							return ConstantInt::get(getGlobalContext(), APInt(1, value->constant.boolConstant));
						case SEM::Value::Constant::INTEGER:
							return ConstantInt::get(getGlobalContext(), APInt(targetInfo_->getLongWidth(), value->constant.intConstant));
						case SEM::Value::Constant::FLOAT:
							return ConstantFP::get(getGlobalContext(), APFloat(value->constant.floatConstant));
						case SEM::Value::Constant::NULLVAL:
							return ConstantPointerNull::get(PointerType::getUnqual(Type::getInt8Ty(getGlobalContext())));
						default:
							std::cerr << "CodeGen error: Unknown constant." << std::endl;
							return UndefValue::get(Type::getVoidTy(getGlobalContext()));
					}
				}
				case SEM::Value::COPY: {
					return genValue(value->copyValue.value);
				}
				case SEM::Value::VAR: {
					SEM::Var* var = value->varValue.var;
					
					switch(var->typeEnum) {
						case SEM::Var::PARAM: {
							if(genLValue) {
								return paramVariables_[var->id];
							} else {
								return builder_.CreateLoad(paramVariables_[var->id]);
							}
						}
						case SEM::Var::LOCAL: {
							if(genLValue) {
								return localVariables_[var->id];
							} else {
								return builder_.CreateLoad(localVariables_[var->id]);
							}
						}
						case SEM::Var::THIS: {
							std::cerr << "CodeGen error: Unimplemented member variable access." << std::endl;
							return ConstantInt::get(getGlobalContext(), APInt(32, 1));
						}
						default: {
							std::cerr << "CodeGen error: Unknown variable type in variable access." << std::endl;
							return ConstantInt::get(getGlobalContext(), APInt(32, 0));
						}
					}
				}
				case SEM::Value::UNARY: {
					SEM::Value::Op::TypeEnum opType = value->unary.opType;
					
					switch(value->unary.typeEnum) {
						case SEM::Value::Unary::PLUS:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							return genValue(value->unary.value);
						case SEM::Value::Unary::MINUS:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateNeg(genValue(value->unary.value));
							} else if(opType == SEM::Value::Op::FLOAT) {
								return builder_.CreateFNeg(genValue(value->unary.value));
							}
							
						case SEM::Value::Unary::NOT:
							assert(opType == SEM::Value::Op::BOOLEAN);
							return builder_.CreateNot(genValue(value->unary.value));
						case SEM::Value::Unary::ADDRESSOF:
							assert(opType == SEM::Value::Op::POINTER);
							return genValue(value->unary.value, true);
						case SEM::Value::Unary::DEREF:
							assert(opType == SEM::Value::Op::POINTER);
							
							if(genLValue) {
								return genValue(value->unary.value);
							} else {
								return builder_.CreateLoad(genValue(value->unary.value));
							}
							
						default:
							std::cerr << "CodeGen error: Unknown unary bool operand." << std::endl;
							return genValue(value->unary.value);
					}
				}
				case SEM::Value::BINARY: {
					SEM::Value::Op::TypeEnum opType = value->binary.opType;
					
					switch(value->binary.typeEnum) {
						case SEM::Value::Binary::ADD:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateAdd(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFAdd(genValue(value->binary.left), genValue(value->binary.right));
							}
							
							return builder_.CreateAdd(genValue(value->binary.left), genValue(value->binary.right));
						case SEM::Value::Binary::SUBTRACT:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateSub(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFSub(genValue(value->binary.left), genValue(value->binary.right));
							}
							
							return builder_.CreateSub(genValue(value->binary.left), genValue(value->binary.right));
						case SEM::Value::Binary::MULTIPLY:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateMul(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFMul(genValue(value->binary.left), genValue(value->binary.right));
							}
							
						case SEM::Value::Binary::DIVIDE:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateSDiv(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFDiv(genValue(value->binary.left), genValue(value->binary.right));
							}
							
						case SEM::Value::Binary::ISEQUAL:
							assert(opType == SEM::Value::Op::BOOLEAN || opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::BOOLEAN || opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateICmpEQ(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFCmpOEQ(genValue(value->binary.left), genValue(value->binary.right));
							}
							
						case SEM::Value::Binary::NOTEQUAL:
							assert(opType == SEM::Value::Op::BOOLEAN || opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::BOOLEAN || opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateICmpNE(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFCmpONE(genValue(value->binary.left), genValue(value->binary.right));
							}
							
						case SEM::Value::Binary::LESSTHAN:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateICmpSLT(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFCmpOLT(genValue(value->binary.left), genValue(value->binary.right));
							}
							
						case SEM::Value::Binary::GREATERTHAN:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateICmpSGT(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFCmpOGT(genValue(value->binary.left), genValue(value->binary.right));
							}
							
						case SEM::Value::Binary::GREATEROREQUAL:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateICmpSGE(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFCmpOGE(genValue(value->binary.left), genValue(value->binary.right));
							}
							
						case SEM::Value::Binary::LESSOREQUAL:
							assert(opType == SEM::Value::Op::INTEGER || opType == SEM::Value::Op::FLOAT);
							
							if(opType == SEM::Value::Op::INTEGER) {
								return builder_.CreateICmpSLE(genValue(value->binary.left), genValue(value->binary.right));
							} else {
								return builder_.CreateFCmpOLE(genValue(value->binary.left), genValue(value->binary.right));
							}
							
						default:
							std::cerr << "CodeGen error: Unknown binary operand." << std::endl;
							return ConstantInt::get(getGlobalContext(), APInt(32, 0));
					}
				}
				case SEM::Value::TERNARY: {
					return builder_.CreateSelect(genValue(value->ternary.condition), genValue(value->ternary.ifTrue, genLValue), genValue(value->ternary.ifFalse, genLValue));
				}
				case SEM::Value::CAST: {
					Value* codeValue = genValue(value->cast.value, genLValue);
					SEM::Type* sourceType = value->cast.value->type;
					SEM::Type* destType = value->type;
					
					assert(sourceType->typeEnum == destType->typeEnum || sourceType->typeEnum == SEM::Type::NULLT);
					
					switch(sourceType->typeEnum) {
						case SEM::Type::VOID: {
							return codeValue;
						}
						case SEM::Type::NULLT: {
							assert(destType->typeEnum == SEM::Type::NAMED || destType->typeEnum == SEM::Type::POINTER || destType->typeEnum == SEM::Type::FUNCTION);
							if(destType->typeEnum == SEM::Type::POINTER || destType->typeEnum == SEM::Type::FUNCTION){
								return builder_.CreatePointerCast(codeValue, genType(destType));
							}
							
							std::cerr << "CodeGen error: Unimplemented cast from null to class type." << std::endl;
							return 0;
						}
						case SEM::Type::BASIC: {
							if(sourceType->basicType.typeEnum == destType->basicType.typeEnum) {
								return codeValue;
							}
							
							// Int -> Float.
							if(sourceType->basicType.typeEnum == SEM::Type::BasicType::INTEGER && destType->basicType.typeEnum == SEM::Type::BasicType::FLOAT) {
								return builder_.CreateSIToFP(codeValue, genType(destType));
							}
							
							// Float -> Int.
							if(sourceType->basicType.typeEnum == SEM::Type::BasicType::FLOAT && destType->basicType.typeEnum == SEM::Type::BasicType::INTEGER) {
								return builder_.CreateFPToSI(codeValue, genType(destType));
							}
							
							return codeValue;
						}
						case SEM::Type::NAMED:
						case SEM::Type::POINTER: {
							if(genLValue) {
								return builder_.CreatePointerCast(codeValue, PointerType::getUnqual(genType(destType)));
							} else {
								return builder_.CreatePointerCast(codeValue, genType(destType));
							}
						}
						case SEM::Type::FUNCTION: {
							return codeValue;
						}
						default:
							std::cerr << "CodeGen error: Unknown type in cast." << std::endl;
							return 0;
					}
				}
				case SEM::Value::CONSTRUCT:
					std::cerr << "CodeGen error: Unimplemented constructor call." << std::endl;
					return ConstantInt::get(getGlobalContext(), APInt(32, 42));
				case SEM::Value::MEMBERACCESS:
					std::cerr << "CodeGen error: Unimplemented member access." << std::endl;
					return ConstantInt::get(getGlobalContext(), APInt(32, 42));
				case SEM::Value::FUNCTIONCALL: {
					std::vector<Value*> parameters;
					
					const std::list<SEM::Value *>& list = value->functionCall.parameters;
					std::list<SEM::Value *>::const_iterator it;
					for(it = list.begin(); it != list.end(); ++it) {
						parameters.push_back(genValue(*it));
					}
					
					return builder_.CreateCall(genValue(value->functionCall.functionValue), parameters);
				}
				case SEM::Value::FUNCTIONREF: {
					Function* function = functions_[value->functionRef.functionDecl];
					assert(function != NULL);
					return function;
				}
				default:
					std::cerr << "CodeGen error: Unknown value." << std::endl;
					return ConstantInt::get(getGlobalContext(), APInt(32, 0));
			}
		}
		
};

void* Locic_CodeGenAlloc(const std::string& moduleName) {
	return new CodeGen(moduleName);
}

void Locic_CodeGenFree(void* context) {
	delete reinterpret_cast<CodeGen*>(context);
}
	
void Locic_CodeGen(void* context, SEM::Module* module) {
	reinterpret_cast<CodeGen*>(context)->genFile(module);
}
	
void Locic_CodeGenDump(void* context) {
	reinterpret_cast<CodeGen*>(context)->dump();
}

