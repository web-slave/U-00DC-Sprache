#include "../lex_synt_lib/assert.hpp"
#include "../lex_synt_lib/keywords.hpp"
#include "mangling.hpp"
#include "error_reporting.hpp"
#include "code_builder.hpp"

namespace U
{

namespace CodeBuilderPrivate
{

void CodeBuilder::ProcessClassParentsVirtualTables( Class& the_class )
{
	U_ASSERT( the_class.completeness != TypeCompleteness::Complete );
	U_ASSERT( the_class.virtual_table.empty() );

	// Copy virtual table of base class.
	if( the_class.base_class != nullptr )
		the_class.virtual_table= the_class.base_class->class_->virtual_table;

	// First, borrow virtual table of parent with 0 offset.
	// Class reuses virtual table pointer of first parent, so, virtual table layout must be equal.
	for( const Class::Parent& parent : the_class.parents )
		if( parent.field_number == 0u )
		{
			the_class.virtual_table= parent.class_->class_->virtual_table;
			break;
		}

	// Then, add virtual functions from other parents.
	// Later, add new virtual functions.
	for( const Class::Parent& parent : the_class.parents )
	{
		if( parent.field_number == 0u )
			continue;

		for( const Class::VirtualTableEntry& parent_vtable_entry : parent.class_->class_->virtual_table )
		{
			bool already_exists_in_vtable= false;
			for( const Class::VirtualTableEntry& this_class_vtable_entry : the_class.virtual_table )
			{
				if( this_class_vtable_entry.name == parent_vtable_entry.name &&
					this_class_vtable_entry.function_variable.VirtuallyEquals( parent_vtable_entry.function_variable ) )
				{
					already_exists_in_vtable= true;
					break;
				}
			}

			if( !already_exists_in_vtable )
				the_class.virtual_table.push_back( parent_vtable_entry );
		} // for parent virtual table
	}
}

void CodeBuilder::TryGenerateDestructorPrototypeForPolymorphClass( Class& the_class, const Type& class_type )
{
	U_ASSERT( the_class.completeness != TypeCompleteness::Complete );
	U_ASSERT( the_class.virtual_table_llvm_type == nullptr );
	U_ASSERT( the_class.this_class_virtual_table == nullptr );

	if( the_class.members.GetThisScopeValue( Keyword( Keywords::destructor_ ) ) != nullptr )
		return;

	// Generate destructor prototype.
	FunctionVariable destructor_function_variable= GenerateDestructorPrototype( the_class, class_type );
	destructor_function_variable.prototype_file_pos= destructor_function_variable.body_file_pos= FilePos(); // TODO - set correct file_pos

	// Add destructor to virtual table.
	Class::VirtualTableEntry* virtual_table_entry= nullptr;
	for( Class::VirtualTableEntry& e : the_class.virtual_table )
	{
		if( e.name == Keywords::destructor_ )
		{
			virtual_table_entry= &e;
			break;
		}
	}
	if( virtual_table_entry == nullptr )
	{
		destructor_function_variable.virtual_table_index= static_cast<unsigned int>(the_class.virtual_table.size());
		Class::VirtualTableEntry new_virtual_table_entry;
		new_virtual_table_entry.function_variable= destructor_function_variable;
		new_virtual_table_entry.name= Keyword( Keywords::destructor_ );
		new_virtual_table_entry.is_pure= false;
		new_virtual_table_entry.is_final= false;
		the_class.virtual_table.push_back( std::move( new_virtual_table_entry ) );
	}
	else
		virtual_table_entry->function_variable= destructor_function_variable;

	// Add destructor to names scope.
	OverloadedFunctionsSet destructors_set;
	destructors_set.functions.push_back(destructor_function_variable);
	the_class.members.AddName( Keyword( Keywords::destructor_ ), destructors_set );
}

void CodeBuilder::ProcessClassVirtualFunction( Class& the_class, FunctionVariable& function )
{
	U_ASSERT( the_class.completeness != TypeCompleteness::Complete );

	const std::string& function_name= function.syntax_element->name_.back();
	const FilePos& file_pos= function.syntax_element->file_pos_;
	CodeBuilderErrorsContainer& errors_container= the_class.members.GetErrors();

	if( function.virtual_function_kind != Synt::VirtualFunctionKind::None &&
		the_class.GetMemberVisibility( function_name ) == ClassMemberVisibility::Private )
	{
		// Private members not visible in child classes. So, virtual private function is 100% error.
		REPORT_ERROR( VirtualForPrivateFunction, errors_container, file_pos, function_name );
	}

	if( !function.is_this_call )
		return; // May be in case of error

	Class::VirtualTableEntry* virtual_table_entry= nullptr;
	for( Class::VirtualTableEntry& e : the_class.virtual_table )
	{
		if( e.name == function_name && e.function_variable.VirtuallyEquals( function ) )
		{
			virtual_table_entry= &e;
			break;
		}
	}
	unsigned int virtual_table_index= ~0u;
	if( virtual_table_entry != nullptr )
		virtual_table_index= static_cast<unsigned int>(virtual_table_entry - the_class.virtual_table.data());

	switch( function.virtual_function_kind )
	{
	case Synt::VirtualFunctionKind::None:
		if( function_name == Keywords::destructor_ )
		{
			// For destructors virtual specifiers are optional.
			// If destructor not marked as virtual, but it placed in polymorph class, make it virtual.
			if( virtual_table_entry != nullptr )
			{
				function.virtual_table_index= virtual_table_index;
				virtual_table_entry->function_variable= function;
			}
			else if( the_class.kind == Class::Kind::PolymorphFinal || the_class.kind == Class::Kind::PolymorphNonFinal ||
					 the_class.kind == Class::Kind::Interface || the_class.kind == Class::Kind::Abstract )
			{
				function.virtual_table_index= static_cast<unsigned int>(the_class.virtual_table.size());

				Class::VirtualTableEntry new_virtual_table_entry;
				new_virtual_table_entry.name= function_name;
				new_virtual_table_entry.function_variable= function;
				new_virtual_table_entry.is_pure= false;
				new_virtual_table_entry.is_final= false;
				the_class.virtual_table.push_back( std::move( new_virtual_table_entry ) );
			}
		}
		else if( virtual_table_entry != nullptr )
			REPORT_ERROR( VirtualRequired, errors_container, file_pos, function_name );
		break;

	case Synt::VirtualFunctionKind::DeclareVirtual:
		if( virtual_table_entry != nullptr )
			REPORT_ERROR( OverrideRequired, errors_container, file_pos, function_name );
		else
		{
			function.virtual_table_index= static_cast<unsigned int>(the_class.virtual_table.size());

			Class::VirtualTableEntry new_virtual_table_entry;
			new_virtual_table_entry.name= function_name;
			new_virtual_table_entry.function_variable= function;
			new_virtual_table_entry.is_pure= false;
			new_virtual_table_entry.is_final= false;
			the_class.virtual_table.push_back( std::move( new_virtual_table_entry ) );
		}
		break;

	case Synt::VirtualFunctionKind::VirtualOverride:
		if( virtual_table_entry == nullptr )
			REPORT_ERROR( FunctionDoesNotOverride, errors_container, file_pos, function_name );
		else if( virtual_table_entry->is_final )
			REPORT_ERROR( OverrideFinalFunction, errors_container, file_pos, function_name );
		else
		{
			function.virtual_table_index= virtual_table_index;
			virtual_table_entry->function_variable= function;
			virtual_table_entry->is_pure= false;
		}
		break;

	case Synt::VirtualFunctionKind::VirtualFinal:
		if( virtual_table_entry == nullptr )
			REPORT_ERROR( FinalForFirstVirtualFunction, errors_container, file_pos, function_name );
		else
		{
			if( virtual_table_entry->is_final )
				REPORT_ERROR( OverrideFinalFunction, errors_container, file_pos, function_name );
			else
			{
				function.virtual_table_index= virtual_table_index;
				virtual_table_entry->function_variable= function;
				virtual_table_entry->is_pure= false;
				virtual_table_entry->is_final= true;
			}
		}
		break;

	case Synt::VirtualFunctionKind::VirtualPure:
		if( virtual_table_entry != nullptr )
			REPORT_ERROR( OverrideRequired, errors_container, file_pos, function_name );
		else
		{
			if( function.syntax_element->block_ != nullptr )
				REPORT_ERROR( BodyForPureVirtualFunction, errors_container, file_pos, function_name );
			if( function_name == Keyword( Keywords::destructor_ ) )
				REPORT_ERROR( PureDestructor, errors_container, file_pos, the_class.members.GetThisNamespaceName() );
			function.have_body= true; // Mark pure function as "with body", because we needs to disable real body creation for pure function.

			function.virtual_table_index= static_cast<unsigned int>(the_class.virtual_table.size());

			Class::VirtualTableEntry new_virtual_table_entry;
			new_virtual_table_entry.name= function_name;
			new_virtual_table_entry.function_variable= function;
			new_virtual_table_entry.is_pure= true;
			new_virtual_table_entry.is_final= false;
			the_class.virtual_table.push_back( std::move( new_virtual_table_entry ) );
		}
		break;
	};
}

void CodeBuilder::PrepareClassVirtualTableType( const ClassProxyPtr& class_type )
{
	Class& the_class= *class_type->class_;
	U_ASSERT( the_class.completeness != TypeCompleteness::Complete );
	U_ASSERT( the_class.virtual_table_llvm_type == nullptr );

	if( the_class.virtual_table.empty() )
		return; // Non-polymorph class.

	// Virtual table layout:
	// offset to allocated object (int_ptr)
	// type id
	// virtual function 0 ptr
	// virtual function 1 ptr
	// ...
	// virtual function n ptr

	the_class.virtual_table_llvm_type= llvm::StructType::create( llvm_context_ );
	std::vector<llvm::Type*> virtual_table_struct_fields;

	virtual_table_struct_fields.push_back( fundamental_llvm_types_.int_ptr ); // Offset field.
	virtual_table_struct_fields.push_back( fundamental_llvm_types_.int_ptr->getPointerTo() ); // type_id field

	for( const Class::VirtualTableEntry& virtual_table_entry : the_class.virtual_table )
	{
		const Function& function_type= *virtual_table_entry.function_variable.type.GetFunctionType();
		virtual_table_struct_fields.push_back( function_type.llvm_function_type->getPointerTo() ); // Function pointer field.
	}

	the_class.virtual_table_llvm_type->setBody( virtual_table_struct_fields );
	the_class.virtual_table_llvm_type->setName( "_vtable_type_" + MangleType(class_type) );
}

void CodeBuilder::BuildClassVirtualTables_r( Class& the_class, const Type& class_type, const std::vector< ClassProxyPtr >& dst_class_path, llvm::Value* dst_class_ptr_null_based )
{
	const Class& dst_class= *dst_class_path.back()->class_;
	std::vector<llvm::Constant*> initializer_values;
	initializer_values.reserve( dst_class.virtual_table_llvm_type->elements().size() );

	// Calculate offset from this class pointer to ancestor class pointer.
	// Such offset will be used for all virtual calls.
	// It's not necessary to keep different offsets for different virtual functions,
	// because all virtual functions implemented in non-interface classes and
	// classes may have only one non-interface parent class, whose offset in class is 0.
	initializer_values.push_back(
		llvm::dyn_cast<llvm::Constant>(
			global_function_context_->llvm_ir_builder.CreatePtrToInt( dst_class_ptr_null_based, fundamental_llvm_types_.int_ptr ) ) );

	initializer_values.push_back( the_class.polymorph_type_id );

	for( const Class::VirtualTableEntry& ancestor_virtual_table_entry : dst_class.virtual_table )
	{
		const Class::VirtualTableEntry* overriden_in_this_class= nullptr;
		for( const Class::VirtualTableEntry& this_class_virtual_table_entry : the_class.virtual_table )
		{
			if( this_class_virtual_table_entry.name == ancestor_virtual_table_entry.name &&
				this_class_virtual_table_entry.function_variable.VirtuallyEquals( ancestor_virtual_table_entry.function_variable ) )
			{
				overriden_in_this_class= &this_class_virtual_table_entry;
				break;
			}
		}
		U_ASSERT( overriden_in_this_class != nullptr ); // We must override, or inherit function.

		llvm::Value* const function_pointer_casted=
			global_function_context_->llvm_ir_builder.CreateBitOrPointerCast(
				overriden_in_this_class->function_variable.llvm_function,
				ancestor_virtual_table_entry.function_variable.type.GetFunctionType()->llvm_function_type->getPointerTo() );

		initializer_values.push_back( llvm::dyn_cast<llvm::Constant>(function_pointer_casted) );
	} // for ancestor virtual table

	std::string vtable_name= "_vtable_of_" + MangleType( class_type ) + "_for";
	for( const ClassProxyPtr& path_component : dst_class_path )
		vtable_name+= "_" + MangleType( path_component );

	llvm::GlobalVariable* const ancestor_vtable=
		new llvm::GlobalVariable(
			*module_,
			dst_class.virtual_table_llvm_type,
			true, // is constant
			llvm::GlobalValue::InternalLinkage,
			 llvm::ConstantStruct::get( dst_class.virtual_table_llvm_type, initializer_values ),
			vtable_name);
	ancestor_vtable->setUnnamedAddr( llvm::GlobalValue::UnnamedAddr::Global );

	U_ASSERT( the_class.ancestors_virtual_tables.find( dst_class_path ) == the_class.ancestors_virtual_tables.end() );
	the_class.ancestors_virtual_tables[dst_class_path]= ancestor_vtable;

	if( !dst_class.parents.empty() )
	{
		auto parent_path= dst_class_path;
		parent_path.emplace_back();
		for( size_t i= 0u; i < dst_class.parents.size(); ++i )
		{
			parent_path.back()= dst_class.parents[i].class_;

			llvm::Value* index_list[2];
			index_list[0]= GetZeroGEPIndex();
			index_list[1]= GetFieldGEPIndex( dst_class.parents[i].field_number );
			llvm::Value* const offset_ptr= global_function_context_->llvm_ir_builder.CreateGEP( dst_class_ptr_null_based, index_list );

			BuildClassVirtualTables_r( the_class, class_type, parent_path, offset_ptr );
		}
	}
}

void CodeBuilder::BuildClassVirtualTables( Class& the_class, const Type& class_type )
{
	U_ASSERT( the_class.completeness != TypeCompleteness::Complete );
	U_ASSERT( the_class.this_class_virtual_table == nullptr );
	U_ASSERT( the_class.ancestors_virtual_tables.empty() );

	if( the_class.virtual_table.empty() )
		return; // Non-polymorph class.

	U_ASSERT( the_class.virtual_table_llvm_type != nullptr );

	llvm::Type* const type_id_type= the_class.virtual_table_llvm_type->getStructElementType(1u)->getPointerElementType();
	the_class.polymorph_type_id=
		new llvm::GlobalVariable(
			*module_,
			type_id_type,
			true, // is_constant
			llvm::GlobalValue::ExternalLinkage,
			llvm::ConstantInt::get( type_id_type, llvm::APInt( type_id_type->getIntegerBitWidth(), 0u ) ),
			"_type_id_for_" + MangleType( class_type ) );
	llvm::Comdat* const type_id_comdat= module_->getOrInsertComdat( the_class.polymorph_type_id->getName() );
	type_id_comdat->setSelectionKind( llvm::Comdat::Any );
	the_class.polymorph_type_id->setComdat( type_id_comdat );

	std::vector<llvm::Constant*> initializer_values;
	initializer_values.reserve( the_class.virtual_table_llvm_type->elements().size() );
	initializer_values.push_back(
		llvm::Constant::getIntegerValue(
			fundamental_llvm_types_.int_ptr,
			llvm::APInt( fundamental_llvm_types_.int_ptr->getIntegerBitWidth(), 0u ) ) ); // For this class virtual table we have zero offset to real this.

	initializer_values.push_back( the_class.polymorph_type_id );

	for( const Class::VirtualTableEntry& virtual_table_entry : the_class.virtual_table )
	{
		if( virtual_table_entry.is_pure )
			return;  // Class is interface or abstract.

		initializer_values.push_back( virtual_table_entry.function_variable.llvm_function );
	}

	the_class.this_class_virtual_table=
		new llvm::GlobalVariable(
			*module_,
			the_class.virtual_table_llvm_type,
			true, // is constant
			llvm::GlobalValue::InternalLinkage,
			 llvm::ConstantStruct::get( the_class.virtual_table_llvm_type, initializer_values ),
			"_vtable_main_" + MangleType(class_type) );
	the_class.this_class_virtual_table->setUnnamedAddr( llvm::GlobalValue::UnnamedAddr::Global );

	// Recursive build virtual tables for all instances of all ancestors.
	llvm::Value* const this_nullptr= llvm::Constant::getNullValue( the_class.llvm_type->getPointerTo() );
	for( size_t i= 0u; i < the_class.parents.size(); ++i )
	{
		llvm::Value* index_list[2];
		index_list[0]= GetZeroGEPIndex();
		index_list[1]= GetFieldGEPIndex( the_class.parents[i].field_number );
		llvm::Value* const offset_ptr= global_function_context_->llvm_ir_builder.CreateGEP( this_nullptr, index_list );
		BuildClassVirtualTables_r( the_class, class_type, {the_class.parents[i].class_}, offset_ptr );
	}
}

std::pair<Variable, llvm::Value*> CodeBuilder::TryFetchVirtualFunction(
	const Variable& this_,
	const FunctionVariable& function,
	FunctionContext& function_context,
	CodeBuilderErrorsContainer& errors_container,
	const FilePos& file_pos )
{
	const Function& function_type= *function.type.GetFunctionType();

	if( !ReferenceIsConvertible( this_.type, function_type.args.front().type, errors_container, file_pos ) )
		return std::make_pair( this_, function.llvm_function );

	Variable this_casted;
	this_casted= this_;
	if( this_.type != function_type.args.front().type )
	{
		this_casted.type= function_type.args.front().type;
		this_casted.llvm_value= CreateReferenceCast( this_.llvm_value, this_.type, this_casted.type, function_context );
	}

	llvm::Value* llvm_function_ptr= function.llvm_function;
	if( function.virtual_table_index != ~0u )
	{
		const Class* const class_type= this_casted.type.GetClassType();
		U_ASSERT( class_type != nullptr );
		U_ASSERT( function.virtual_table_index < class_type->virtual_table.size() );

		const unsigned int offset_field_number= 0u;
		const unsigned int type_id_field_number= 1u;
		const unsigned int func_ptr_field_number= type_id_field_number + 1u + function.virtual_table_index;

		// Fetch vtable pointer.
		// Virtual table pointer is always first field.
		llvm::Value* const ptr_to_virtual_table_ptr= function_context.llvm_ir_builder.CreatePointerCast( this_casted.llvm_value, class_type->virtual_table_llvm_type->getPointerTo()->getPointerTo() );
		llvm::Value* const virtual_table_ptr= function_context.llvm_ir_builder.CreateLoad( ptr_to_virtual_table_ptr );
		// Fetch function.
		llvm::Value* index_list[2];
		index_list[0]= GetZeroGEPIndex();
		index_list[1]= GetFieldGEPIndex( func_ptr_field_number );
		llvm::Value* const ptr_to_function_ptr= function_context.llvm_ir_builder.CreateGEP( virtual_table_ptr, index_list );
		llvm_function_ptr= function_context.llvm_ir_builder.CreateLoad( ptr_to_function_ptr );
		// Fetch "this" pointer offset.
		index_list[1]= GetFieldGEPIndex( offset_field_number );
		llvm::Value* const offset_ptr= function_context.llvm_ir_builder.CreateGEP( virtual_table_ptr, index_list );
		llvm::Value* const offset= function_context.llvm_ir_builder.CreateLoad( offset_ptr );
		// Correct "this" pointer.
		llvm::Value* const this_ptr_as_int= function_context.llvm_ir_builder.CreatePtrToInt( this_casted.llvm_value, fundamental_llvm_types_.int_ptr );
		llvm::Value* this_sub_offset= function_context.llvm_ir_builder.CreateSub( this_ptr_as_int, offset );
		this_casted.llvm_value= function_context.llvm_ir_builder.CreateIntToPtr( this_sub_offset, this_casted.type.GetLLVMType()->getPointerTo() );
	}

	return std::make_pair( std::move(this_casted), llvm_function_ptr );
}


void CodeBuilder::SetupVirtualTablePointers_r(
	llvm::Value* this_,
	const std::vector< ClassProxyPtr >& class_path,
	const std::map< std::vector< ClassProxyPtr >, llvm::GlobalVariable* > virtual_tables,
	FunctionContext& function_context )
{
	const Class& the_class= *class_path.back()->class_;
	U_ASSERT( virtual_tables.find( class_path ) != virtual_tables.end() );

	if( !the_class.parents.empty() )
	{
		auto parent_path= class_path;
		parent_path.emplace_back();
		for( const Class::Parent& parent : the_class.parents )
		{
			parent_path.back()= parent.class_;

			llvm::Value* index_list[2];
			index_list[0]= GetZeroGEPIndex();
			index_list[1]= GetFieldGEPIndex( parent.field_number );
			llvm::Value* const parent_ptr= function_context.llvm_ir_builder.CreateGEP( this_, index_list );
			SetupVirtualTablePointers_r( parent_ptr, parent_path, virtual_tables, function_context );
		}
	}

	// Overwrite, if needed, first parent virtual table.
	llvm::Value* const ptr_to_vtable_ptr= function_context.llvm_ir_builder.CreatePointerCast( this_, the_class.virtual_table_llvm_type->getPointerTo()->getPointerTo() );
	function_context.llvm_ir_builder.CreateStore( virtual_tables.find(class_path)->second, ptr_to_vtable_ptr );
}

void CodeBuilder::SetupVirtualTablePointers(
	llvm::Value* this_,
	const Class& the_class,
	FunctionContext& function_context )
{
	if( the_class.virtual_table.empty() )
	{
		U_ASSERT( the_class.virtual_table_llvm_type == nullptr );
		U_ASSERT( the_class.this_class_virtual_table == nullptr );
		return;
	}

	if( the_class.kind == Class::Kind::Interface || the_class.kind == Class::Kind::Abstract )
		return; // Such kinds of classes have no virtual tables. SPRACHE_TODO - maybe generate for such classes some virtual tables?

	if( the_class.this_class_virtual_table == nullptr )
		return; // May be in case of errors.

	for( const Class::Parent& parent : the_class.parents )
	{
		llvm::Value* index_list[2];
		index_list[0]= GetZeroGEPIndex();
		index_list[1]= GetFieldGEPIndex( parent.field_number );
		llvm::Value* const parent_ptr= function_context.llvm_ir_builder.CreateGEP( this_, index_list );
		SetupVirtualTablePointers_r( parent_ptr, { parent.class_ }, the_class.ancestors_virtual_tables, function_context );
	}

	// Overwrite, if needed, first parent virtual table.
	llvm::Value* const ptr_to_vtable_ptr= function_context.llvm_ir_builder.CreatePointerCast( this_,  the_class.virtual_table_llvm_type->getPointerTo()->getPointerTo() );
	function_context.llvm_ir_builder.CreateStore( the_class.this_class_virtual_table, ptr_to_vtable_ptr );
}

} // namespace CodeBuilderPrivate

} // namespace U
