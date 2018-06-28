#include "push_disable_llvm_warnings.hpp"
#include <llvm/IR/InstrTypes.h>
#include "pop_llvm_warnings.hpp"

#include "assert.hpp"
#include "constexpr_function_evaluator.hpp"

namespace U
{

namespace CodeBuilderPrivate
{

ConstexprFunctionEvaluator::ConstexprFunctionEvaluator( const llvm::DataLayout& data_layout )
	: data_layout_(data_layout)
{
}

ConstexprFunctionEvaluator::Result ConstexprFunctionEvaluator::Evaluate(
	const Function& function_type,
	llvm::Function* const llvm_function,
	const std::vector<llvm::Constant*>& args )
{
	U_ASSERT( function_type.args.size() == args.size() );

	// Fill arguments
	size_t i= 0u;
	for( const llvm::Argument& arg : llvm_function->args() )
	{
		if( arg.hasStructRetAttr() )
		{
			++i;
			continue;
		}

		if( arg.getType()->isPointerTy() )
		{
			size_t ptr= MoveConstantToStack( *args[i] );
			llvm::GenericValue val;
			val.IntVal= llvm::APInt( 64u, uint64_t(ptr) );

			instructions_map_[ &arg ]= val;
		}
		else if( arg.getType()->isIntegerTy() )
		{
			llvm::GenericValue val;
			val.IntVal= args[i]->getUniqueInteger();
			instructions_map_[ &arg ]= val;
		}
		else if( arg.getType()->isFloatTy() )
		{
			llvm::GenericValue val;
			val.FloatVal= llvm::dyn_cast<llvm::ConstantFP>(args[i])->getValueAPF().convertToFloat();
			instructions_map_[ &arg ]= val;
		}
		else if( arg.getType()->isDoubleTy() )
		{
			llvm::GenericValue val;
			val.DoubleVal= llvm::dyn_cast<llvm::ConstantFP>(args[i])->getValueAPF().convertToDouble();
			instructions_map_[ &arg ]= val;
		}
		else
			U_ASSERT(false);

		++i;
	}

	llvm::GenericValue res= CallFunction( *llvm_function );
	Result result;

	if( function_type.return_value_is_reference )
	{
		U_ASSERT(false);
	}
	else
	{
		if( const FundamentalType* const fundamental= function_type.return_type.GetFundamentalType() )
		{
			if( IsInteger( fundamental->fundamental_type ) || fundamental->fundamental_type == U_FundamentalType::Bool )
				result.result_constant= llvm::Constant::getIntegerValue( function_type.return_type.GetLLVMType(), res.IntVal );
			else if( IsFloatingPoint( fundamental->fundamental_type ) )
			{
				result.result_constant=
					llvm::ConstantFP::get(
						function_type.return_type.GetLLVMType(),
						fundamental->fundamental_type == U_FundamentalType::f32 ? double(res.FloatVal) : res.DoubleVal );
			}
			else
				U_ASSERT(false);
		}
		else
		{
			U_ASSERT(false);
		}
	}

	instructions_map_.clear();
	stack_.clear();

	return result;
}

llvm::GenericValue ConstexprFunctionEvaluator::CallFunction( const llvm::Function& llvm_function )
{
	const llvm::Instruction* instruction= llvm_function.getBasicBlockList().front().begin();
	while(true)
	{
		switch( instruction->getOpcode() )
		{
		case llvm::Instruction::Alloca:
			ProcessAlloca(instruction);
			instruction= instruction->getNextNode();
			break;

		case llvm::Instruction::Load:
			ProcessLoad(instruction);
			instruction= instruction->getNextNode();
			break;

		case llvm::Instruction::Store:
			ProcessStore(instruction);
			instruction= instruction->getNextNode();
			break;

		case llvm::Instruction::Br:
			{
				if( instruction->getNumOperands() == 1u )
				{
					// Unconditional
					const auto basic_block= llvm::dyn_cast<llvm::BasicBlock>(instruction->getOperand(0u));
					instruction= basic_block->getInstList().begin();
				}
				else
				{
					const llvm::Value* const condition_value= instruction->getOperand(0u);
					U_ASSERT( instructions_map_.find( condition_value ) != instructions_map_.end() );
					const llvm::GenericValue& val= instructions_map_[condition_value];

					if( !val.IntVal.getBoolValue() )
					{
						const auto basic_block= llvm::dyn_cast<llvm::BasicBlock>(instruction->getOperand(1u));
						instruction= basic_block->getInstList().begin();
					}
					else
					{
						const auto basic_block= llvm::dyn_cast<llvm::BasicBlock>(instruction->getOperand(2u));
						instruction= basic_block->getInstList().begin();
					}
				}
			}
			break;

		case llvm::Instruction::Ret:
			{
				if( instruction->getNumOperands() == 0u )
					return llvm::GenericValue();

				return GetVal( instruction->getOperand(0u) );
			}

		case llvm::Instruction::Add:
		case llvm::Instruction::Sub:
		case llvm::Instruction::Mul:
		case llvm::Instruction::SDiv:
		case llvm::Instruction::UDiv:
		case llvm::Instruction::SRem:
		case llvm::Instruction::URem:
		case llvm::Instruction::And:
		case llvm::Instruction::Or:
		case llvm::Instruction::Xor:
		case llvm::Instruction::Shl:
		case llvm::Instruction::AShr:
		case llvm::Instruction::LShr:
		case llvm::Instruction::FAdd:
		case llvm::Instruction::FSub:
		case llvm::Instruction::FMul:
		case llvm::Instruction::FDiv:
		case llvm::Instruction::FRem:
		case llvm::Instruction::ICmp:
			ProcessBinaryArithmeticInstruction(instruction);
			instruction= instruction->getNextNode();
			break;


		default:
			U_ASSERT(false);
			break;
		};
	}
}

size_t ConstexprFunctionEvaluator::MoveConstantToStack( const llvm::Constant& constant )
{
	const size_t stack_offset= stack_.size();
	stack_.resize( stack_.size() + data_layout_.getTypeAllocSize( constant.getType() ) );

	if( const auto struct_type= llvm::dyn_cast<llvm::StructType>( constant.getType() ) )
	{
		const llvm::StructLayout& struct_layout= *data_layout_.getStructLayout( struct_type );

		size_t i= 0u;
		for( llvm::Type* const element_type : struct_type->elements() )
		{
			llvm::Constant* const element= constant.getAggregateElement(i);
			const size_t element_size= data_layout_.getTypeAllocSize( element_type );
			const size_t element_offset= struct_layout.getElementOffset(i);

			if( element_type->isPointerTy() ) // TODO - what if it is function pointer ?
			{
				const size_t element_ptr= MoveConstantToStack( *element );
				std::memcpy( stack_.data() + stack_offset + element_offset, stack_.data() + element_ptr, element_size );
			}
			else if( element_type->isStructTy() )
			{
				U_ASSERT(false); // TODO
			}
			// TODO - checl big/little-endian correctness.
			else if( element_type->isIntegerTy() )
			{
				const uint64_t val= element->getUniqueInteger().getLimitedValue();
				std::memcpy( stack_.data() + stack_offset + element_offset, &val, element_size );
			}
			else if( element_type->isFloatTy() )
			{
				const float val= llvm::dyn_cast<llvm::ConstantFP>( element )->getValueAPF().convertToFloat();
				std::memcpy( stack_.data() + stack_offset + element_offset, &val, element_size );
			}
			else if( element_type->isDoubleTy() )
			{
				const double val= llvm::dyn_cast<llvm::ConstantFP>( element )->getValueAPF().convertToDouble();
				std::memcpy( stack_.data() + stack_offset + element_offset, &val, element_size );
			}
			else if( element_type->isArrayTy() )
			{
				U_ASSERT(false); // TODO
			}
			else
				U_ASSERT(false);

			++i;
		}
	}

	return stack_offset;
}

llvm::GenericValue ConstexprFunctionEvaluator::GetVal( const llvm::Value* const val )
{
	llvm::GenericValue res;
	if( const auto constant_fp= llvm::dyn_cast<llvm::ConstantFP>(val) )
	{
		if( val->getType()->isFloatTy() )
			res.FloatVal= constant_fp->getValueAPF().convertToFloat();
		else if( val->getType()->isDoubleTy() )
			res.DoubleVal= constant_fp->getValueAPF().convertToDouble();
		else U_ASSERT(false);
	}
	else if( const auto constant= llvm::dyn_cast<llvm::Constant>(val) )
		res.IntVal= constant->getUniqueInteger();
	else
	{
		U_ASSERT( instructions_map_.find( val ) != instructions_map_.end() );
		res= instructions_map_[val];
	}
	return res;
}

void ConstexprFunctionEvaluator::ProcessAlloca( const llvm::Instruction* const instruction )
{
	llvm::Type* const element_type= instruction->getOperand(0u)->getType();

	const size_t size= data_layout_.getTypeAllocSize( element_type );

	const size_t stack_offset= stack_.size();
	stack_.resize( stack_.size() + size );

	llvm::GenericValue val;
	val.IntVal= llvm::APInt( 64u, uint64_t(stack_offset) );
	instructions_map_[ instruction ]= val;
}

void ConstexprFunctionEvaluator::ProcessLoad( const llvm::Instruction* const instruction )
{
	const llvm::Value* const address= instruction->getOperand(0u);
	U_ASSERT( instructions_map_.find( address ) != instructions_map_.end() );
	const llvm::GenericValue& address_val= instructions_map_[address];

	const size_t offset= address_val.IntVal.getLimitedValue();
	U_ASSERT( offset < stack_.size() );

	llvm::Type* const element_type= llvm::dyn_cast<llvm::PointerType>(address->getType())->getElementType();
	if( element_type->isIntegerTy() )
	{
		uint64_t buff[4];
		std::memcpy( buff, stack_.data() + offset, data_layout_.getTypeStoreSize( element_type ) );

		llvm::GenericValue val;
		val.IntVal= llvm::APInt( element_type->getIntegerBitWidth() , buff );
		instructions_map_[ instruction ]= val;
	}
	else if( element_type->isFloatTy() )
	{
		llvm::GenericValue val;
		std::memcpy( &val.FloatVal, stack_.data() + offset, sizeof(float) );
		instructions_map_[ instruction ]= val;
	}
	else if( element_type->isDoubleTy() )
	{
		llvm::GenericValue val;
		std::memcpy( &val.DoubleVal, stack_.data() + offset, sizeof(double) );
		instructions_map_[ instruction ]= val;
	}
	else if( element_type->isPointerTy() )
	{
		SizeType ptr;
		std::memcpy( &ptr, stack_.data() + offset, data_layout_.getTypeAllocSize( element_type ) );
		llvm::GenericValue val;
		val.IntVal= llvm::APInt( 64u , ptr );
		instructions_map_[ instruction ]= val;
	}
	else
		U_ASSERT(false);
}

void ConstexprFunctionEvaluator::ProcessStore( const llvm::Instruction* const instruction )
{
	const llvm::Value* const address= instruction->getOperand(1u);
	U_ASSERT( instructions_map_.find( address ) != instructions_map_.end() );
	const llvm::GenericValue& address_val= instructions_map_[address];

	const size_t offset= address_val.IntVal.getLimitedValue();
	U_ASSERT( offset < stack_.size() );

	const llvm::GenericValue val= GetVal( instruction->getOperand(0u) );

	llvm::Type* const element_type= llvm::dyn_cast<llvm::PointerType>(address->getType())->getElementType();
	if( element_type->isIntegerTy() )
	{
		uint64_t limited_value= val.IntVal.getLimitedValue();
		std::memcpy( stack_.data() + offset, &limited_value,  data_layout_.getTypeStoreSize( element_type ) );
	}
	else if( element_type->isFloatTy() )
		std::memcpy( stack_.data() + offset, &val.FloatVal, data_layout_.getTypeAllocSize( element_type ) );
	else if( element_type->isDoubleTy() )
		std::memcpy( stack_.data() + offset, &val.DoubleVal, data_layout_.getTypeAllocSize( element_type ) );
	else if( element_type->isPointerTy() )
	{
		SizeType ptr= val.IntVal.getLimitedValue();
		std::memcpy( stack_.data() + offset, &ptr, data_layout_.getTypeAllocSize( element_type ) );
	}
	else
		U_ASSERT(false);
}

void ConstexprFunctionEvaluator::ProcessBinaryArithmeticInstruction( const llvm::Instruction* const instruction )
{
	const llvm::GenericValue op0= GetVal( instruction->getOperand(0u) );
	const llvm::GenericValue op1= GetVal( instruction->getOperand(1u) );

	llvm::Type* type= instruction->getType();
	llvm::GenericValue val;
	switch(instruction->getOpcode())
	{
	case llvm::Instruction::Add:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal + op1.IntVal;
		break;

	case llvm::Instruction::Sub:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal - op1.IntVal;
		break;

	case llvm::Instruction::Mul:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal * op1.IntVal;
		break;

	case llvm::Instruction::SDiv:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.sdiv(op1.IntVal); // TODO - check division by zero
		break;

	case llvm::Instruction::UDiv:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.udiv(op1.IntVal); // TODO - check division by zero
		break;

	case llvm::Instruction::SRem:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.srem(op1.IntVal);
		break;

	case llvm::Instruction::URem:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.urem(op1.IntVal);
		break;

	case llvm::Instruction::And:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.And(op1.IntVal);
		break;

	case llvm::Instruction::Or:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.Or(op1.IntVal);
		break;

	case llvm::Instruction::Xor:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.Xor(op1.IntVal);
		break;


	case llvm::Instruction::Shl:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.shl(op1.IntVal);
		break;

	case llvm::Instruction::AShr:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.ashr(op1.IntVal);
		break;

	case llvm::Instruction::LShr:
		U_ASSERT(type->isIntegerTy());
		val.IntVal= op0.IntVal.lshr(op1.IntVal);
		break;

	case llvm::Instruction::FAdd:
		if( type->isFloatTy() )
			val.FloatVal= ( llvm::APFloat(op0.FloatVal) + llvm::APFloat(op1.FloatVal) ).convertToFloat();
		else if( type->isDoubleTy() )
			val.DoubleVal= ( llvm::APFloat(op0.DoubleVal) + llvm::APFloat(op1.DoubleVal) ).convertToDouble();
		else U_ASSERT(false);
		break;

	case llvm::Instruction::FSub:
		if( type->isFloatTy() )
			val.FloatVal= ( llvm::APFloat(op0.FloatVal) - llvm::APFloat(op1.FloatVal) ).convertToFloat();
		else if( type->isDoubleTy() )
			val.DoubleVal= ( llvm::APFloat(op0.DoubleVal) - llvm::APFloat(op1.DoubleVal) ).convertToDouble();
		else U_ASSERT(false);
		break;

	case llvm::Instruction::FMul:
		if( type->isFloatTy() )
			val.FloatVal= ( llvm::APFloat(op0.FloatVal) * llvm::APFloat(op1.FloatVal) ).convertToFloat();
		else if( type->isDoubleTy() )
			val.DoubleVal= ( llvm::APFloat(op0.DoubleVal) * llvm::APFloat(op1.DoubleVal) ).convertToDouble();
		else U_ASSERT(false);
		break;

	case llvm::Instruction::FDiv:
		if( type->isFloatTy() )
			val.FloatVal= ( llvm::APFloat(op0.FloatVal) / llvm::APFloat(op1.FloatVal) ).convertToFloat();
		else if( type->isDoubleTy() )
			val.DoubleVal= ( llvm::APFloat(op0.DoubleVal) / llvm::APFloat(op1.DoubleVal) ).convertToDouble();
		else U_ASSERT(false);
		break;

	case llvm::Instruction::FRem:
		U_ASSERT(false); // TODO
		break;


	case llvm::Instruction::ICmp:
		{
			U_ASSERT(type->isIntegerTy());
			switch(llvm::dyn_cast<llvm::CmpInst>(instruction)->getPredicate())
			{
			case llvm::CmpInst::ICMP_EQ : val.IntVal= op0.IntVal.eq (op1.IntVal); break;
			case llvm::CmpInst::ICMP_NE : val.IntVal= op0.IntVal.ne (op1.IntVal); break;
			case llvm::CmpInst::ICMP_UGT: val.IntVal= op0.IntVal.ugt(op1.IntVal); break;
			case llvm::CmpInst::ICMP_UGE: val.IntVal= op0.IntVal.uge(op1.IntVal); break;
			case llvm::CmpInst::ICMP_ULT: val.IntVal= op0.IntVal.ult(op1.IntVal); break;
			case llvm::CmpInst::ICMP_ULE: val.IntVal= op0.IntVal.ule(op1.IntVal); break;
			case llvm::CmpInst::ICMP_SGT: val.IntVal= op0.IntVal.sgt(op1.IntVal); break;
			case llvm::CmpInst::ICMP_SGE: val.IntVal= op0.IntVal.sge(op1.IntVal); break;
			case llvm::CmpInst::ICMP_SLT: val.IntVal= op0.IntVal.slt(op1.IntVal); break;
			case llvm::CmpInst::ICMP_SLE: val.IntVal= op0.IntVal.sle(op1.IntVal); break;
			default: U_ASSERT(false); break;
			};
		}
		break;

	default:
		U_ASSERT(false);
		break;
	};

	instructions_map_[instruction]= val;
}

} // namespace CodeBuilderPrivate

} // namespace U
