#include "func_decl.h"

//////////////////////////
// Function Declaration //
//////////////////////////
void FuncDeclNode::proto( DeclSeq *d,Environ *e ){
	Type *t=tagType( tag,e );if( !t ) t=Type::int_type;
	a_ptr<DeclSeq> decls( d_new DeclSeq() );
	params->proto( decls,e );
	sem_type=d_new FuncType( t,decls.release(),false,false );
	if( !d->insertDecl( ident,sem_type,DECL_FUNC ) ){
		delete sem_type;ex( "duplicate identifier" );
	}
	e->types.push_back( sem_type );
}

void FuncDeclNode::semant( Environ *e ){

	sem_env=d_new Environ( genLabel(),sem_type->returnType,1,e );
	DeclSeq *decls=sem_env->decls;

	int k;
	for( k=0;k<sem_type->params->size();++k ){
		Decl *d=sem_type->params->decls[k];
		if( !decls->insertDecl( d->name,d->type,d->kind ) ) ex( "duplicate identifier" );
	}

	stmts->semant( sem_env );
}

void FuncDeclNode::translate( Codegen *g ){

	//var offsets
	int size=enumVars( sem_env );

	//enter function
	g->enter( "_f"+ident,size );

	//initialize locals
	TNode *t=createVars( sem_env );
	if( t ) g->code( t );
	if( g->debug ){
		std::string t=genLabel();
		g->s_data( ident,t );
		g->code( call( "__bbDebugEnter",local(0),iconst((bb_int_t)sem_env),global(t) ) );
	}

	//translate statements
	stmts->translate( g );

	for( int k=0;k<sem_env->labels.size();++k ){
		if( sem_env->labels[k]->def<0 )	ex( "Undefined label",sem_env->labels[k]->ref );
	}

	//leave the function
	g->label( sem_env->funcLabel+"_leave" );
	t=deleteVars( sem_env );
	if( g->debug ) t=d_new TNode( IR_SEQ,call( "__bbDebugLeave" ),t );
	g->leave( t,sem_type->params->size()*4 );
}

#ifdef USE_LLVM
#include <llvm/IR/Verifier.h>
#include <llvm/IR/ValueSymbolTable.h>

void FuncDeclNode::translate2( Codegen_LLVM *g ){
	auto func=sem_type->llvmFunction( ident,g );
	auto block = llvm::BasicBlock::Create( *g->context,"entry",func );
	g->builder->SetInsertPoint( block );
	g->builder->SetCurrentDebugLocation( llvm::DebugLoc() );
	if( g->debug ){
		size_t slash=file.find_last_of("/\\");
		auto unit=g->dbgBuilder->createFile( file.substr(slash+1),slash==std::string::npos ? "." : file.substr(0,slash) );
		auto signature=g->dbgBuilder->createSubroutineType( g->dbgBuilder->getOrCreateTypeArray({}) );
		unsigned line=(pos>>16)&0xffff;
		auto scope=g->dbgBuilder->createFunction( unit,ident,func->getName(),unit,line,signature,line,llvm::DINode::FlagPrototyped,llvm::DISubprogram::SPFlagDefinition );
		func->setSubprogram( scope );
		g->dbgBlocks.push_back( scope );
	}

	for( int k=0;k<sem_type->params->size();k++ ){
		Decl *d=sem_env->decls->decls[k];
		d->ptr=g->builder->CreateAlloca( d->type->llvmType( g->context.get() ),nullptr,d->name );
		g->builder->CreateStore( g->builder->GetInsertBlock()->getParent()->getArg( k ),d->ptr );
	}

	createVars2( sem_env,g );

	stmts->translate2( g );

	auto final_block=g->builder->GetInsertBlock();
	if( final_block->size()==0 ){
		final_block->eraseFromParent();
	}
	if( g->debug ) g->dbgBlocks.pop_back();
	g->builder->SetCurrentDebugLocation( llvm::DebugLoc() );
}
#endif

json FuncDeclNode::toJSON( Environ *e ){
	json tree;tree["@class"]="FuncDeclNode";
	tree["pos"]=pos;
	tree["file"]=file;
	tree["ident"]=ident;
	tree["tag"]=tag;
	tree["types"]=json::array();
	for( int k=0;k<sem_env->types.size();++k ){
		tree["types"].push_back( sem_env->types[k]->toFullJSON() );
	}
	tree["locals"]=json::array();
	for( int k=0;k<sem_env->decls->size();++k ){
		Decl *d=sem_env->decls->decls[k];
		if( d->kind!=DECL_LOCAL ) continue;
		tree["locals"].push_back( d->toJSON() );
	}
	tree["stmts"]=stmts->toJSON( sem_env );
	tree["sem_type"]=sem_type->toJSON( );
	return tree;
}
