//
//  ReflectionData.cpp
//  Qi Game Engine
//
//  Created by Cody White on 3/23/15.
//  Copyright (c) 2015 Cody White. All rights reserved.
//

#include "ReflectionData.h"
#include "ReflectedVariable.h"
#include "PointerTable.h"
#include "ReflectionUtilities.h"
#include <iostream>

namespace Qi
{

// ReflectedMember implementation begin ------------------------------------------------------

	ReflectedMember::ReflectedMember(const std::string &name, size_t offset, size_t size, bool isPointer, const ReflectionData *reflectionData) :
    m_name(name),
    m_offset(offset),
	m_size(size),
	m_isPointer(isPointer),
    m_data(reflectionData)
{
}

ReflectedMember::~ReflectedMember()
{
}

const std::string &ReflectedMember::GetName() const
{
    return m_name;
}

size_t ReflectedMember::GetOffset() const
{
    return m_offset;
}

const ReflectionData *ReflectedMember::GetReflectionData() const
{
    return m_data;
}

size_t ReflectedMember::GetSize() const
{
	return m_size;
}

bool ReflectedMember::IsArray() const
{
	// If this is an array, m_size will contain the size of of the entire array
	// whereas the reflection data will contain the size of just one element of
	// the array.
	return (m_size > m_data->GetSize());
}

bool ReflectedMember::IsPointer() const
{
	return m_isPointer;
}

// ReflectedMember implementation end --------------------------------------------------------

// ReflectionData implementation begin -------------------------------------------------------

ReflectionData::ReflectionData() :
    m_name(""),
    m_size(0),
    m_serializeFunction(nullptr),
	m_parent(nullptr)
{
}
    
ReflectionData::~ReflectionData()
{
	for (auto iter = m_members.begin(); iter != m_members.end(); ++iter)
	{
		delete *iter;
	}
}
    
void ReflectionData::Init(ReflectionDataCInfo &info)
{
	QI_ASSERT(info.size > 0 && "Invalid size specified as a reflected type");
	QI_ASSERT(info.name.length() && "Type has no name");
	QI_ASSERT(info.allocateFunction != nullptr && "No allocation function specified for type");

	m_name = info.name;
	m_size = info.size;
	m_allocateInstanceFunction = info.allocateFunction;
}
    
const std::string &ReflectionData::GetName() const
{
    return m_name;
}
    
size_t ReflectionData::GetSize() const
{
    return m_size;
}

void ReflectionData::DeclareParent(const ReflectionData *parent)
{
	m_parent = parent;
}

bool ReflectionData::HasParent() const
{
	return (m_parent != nullptr);
}
    
void ReflectionData::AddMember(const ReflectedMember *member)
{
	m_members.push_back(const_cast<ReflectedMember *>(member));
}
    
bool ReflectionData::HasDataMembers() const
{
    return (!m_members.empty());
}

const ReflectedMember *ReflectionData::GetMember(const std::string &name) const
{
	for (auto iter = m_members.begin(); iter != m_members.end(); ++iter)
	{
		if ((*iter)->GetName() == name)
		{
			return *iter;
		}
	}

	return nullptr;
}

const ReflectionData::Members &ReflectionData::GetMembers() const
{
    return m_members;
}
    
void *ReflectionData::AllocateInstance() const
{
	return m_allocateInstanceFunction();
}

void Pad(std::ostream &stream, uint32 pad)
{
    for (uint32 ii = 0; ii < pad; ++ii)
    {
        stream << "\t";
    }
}

void ReflectionData::Serialize(const ReflectedVariable *variable, std::ostream &stream, PointerTable &pointerTable, uint32 padding, bool isArray) const
{
	// If this object has a parent, serialize its data first.
	if (m_parent)
	{
		m_parent->Serialize(variable, stream, pointerTable, padding);
	}

    // If this type has a valid serialization function then it knows how to serialize itself, let it.
    if (m_serializeFunction)
    {
        m_serializeFunction(variable, stream);
        return;
    }
    
    // For each member of this type, ask it to serialize itself.

	// Write out the name of this type.
	if (!isArray)
	{
		stream << pointerTable.GetIndex(*variable) << " ";
	}

	stream << m_name << std::endl;

	// Make sure the instance data for this object is valid (could be a null pointer).
	if (variable->GetInstanceData() == nullptr)
	{
		Pad(stream, padding);
		stream << "[" << std::endl;
		++padding;
		Pad(stream, padding);
		stream << "null" << std::endl;
		--padding;
		Pad(stream, padding);
		stream << "]" << std::endl;
		return;
	}

	Pad(stream, padding);
    stream << "[" << std::endl;
    ++padding;
	for (auto iter = m_members.begin(); iter != m_members.end(); ++iter)
    {
		const ReflectedMember *member = *iter;
		Pad(stream, padding);

		// If this is a pointer type, serialize its index in the pointer table.
		if (member->IsPointer())
		{
			void *offsetData = PointerOffset(variable->GetInstanceData(), member->GetOffset());
			ReflectedVariable memberVariable(member->GetReflectionData(), offsetData);

			void *pointerData = &(*(memberVariable.GetValue<char *>()));
			ReflectedVariable resolvedPointer(member->GetReflectionData(), pointerData);

			stream << member->GetName() << " " << pointerTable.GetIndex(resolvedPointer) << std::endl;
		}

		// If this type is an array, we have to serialize each element of the array before moving on 
		// to the next member variable.
		else if (member->IsArray())
		{
			stream << member->GetName() << std::endl;
			++padding;
			const ReflectionData *data = member->GetReflectionData();
			size_t baseTypeSize = data->GetSize();
			QI_ASSERT(baseTypeSize > 0 && "Attempted to serialize unknown type");
			for (size_t ii = 0; ii < member->GetSize(); ii += baseTypeSize)
			{
				Pad(stream, padding);

				// Get the next element to serialize.
				void *offsetData = PointerOffset(variable->GetInstanceData(), member->GetOffset() + ii);
				ReflectedVariable arrayElement(data, offsetData);
				data->Serialize(&arrayElement, stream, pointerTable, padding, true);
			}
			--padding;
		}
		else // non-array/pointer type.
		{
			stream << member->GetName() << " ";
			void *offsetData = PointerOffset(variable->GetInstanceData(), member->GetOffset());
			ReflectedVariable memberVariable(member->GetReflectionData(), offsetData);
			member->GetReflectionData()->Serialize(&memberVariable, stream, pointerTable, padding, false);
		}
    }

    --padding;
    Pad(stream, padding);
	stream << "]" << std::endl;
}

void ReflectionData::Deserialize(ReflectedVariable *variable, std::istream &stream, PointerTable &pointerTable, bool isArray) const
{
	// If this object has a parent, deserialize its data first.
	if (m_parent)
	{
		m_parent->Deserialize(variable, stream, pointerTable, isArray);
	}

	// If this type has a valid deserialization function then it knows how to deserialize itself, let it.
	if (m_deserializeFunction)
	{
		m_deserializeFunction(variable, stream);
		return;
	}

	// For each member read from this object, ask it to deserialize itself if we have a definition for it.

	std::string streamInput;

	// Read the pointer table index and typename first if we're not deserializing an array.
	PointerTable::TableIndex tableIndex = 0;
	if (!isArray)
	{
		stream >> tableIndex;
		QI_ASSERT(tableIndex >= 0);
	}

	stream >> streamInput;
	QI_ASSERT(streamInput == m_name);

	// Read the starting bracket denoting the start of member variables for this type.
	{
		stream >> streamInput;
		QI_ASSERT(streamInput == "[");
	}

	while (streamInput != "]")
	{
		// Read in the type.
		stream >> streamInput;
        QI_ASSERT(stream);

		// Handle deserializing a NULL pointer. In this case, there will be no other
		// members to deserialize as this instance has no data.
		if (streamInput == "null")
		{
			variable->SetInstanceData(nullptr);
			continue;
		}
	
		const ReflectedMember *member = GetMember(streamInput);
		if (member)
		{
			if (member->IsPointer())
			{
				// Read in the index for this pointer that corresponds to the pointer table.
				PointerTable::TableIndex pointerIndex = 0;
				stream >> pointerIndex;
				QI_ASSERT(pointerIndex >= 0);

				void *offsetData = PointerOffset(variable->GetInstanceData(), member->GetOffset());
				ReflectedVariable memberVariable(member->GetReflectionData(), offsetData);

				// Add this pointer to the patch table to deffer resolving it until the pointer table
				// has been entirely deserialized.
				pointerTable.AddPatchPointer(pointerIndex, memberVariable);
 			}
			else if (member->IsArray()) // If this member is an array type, read in each element of the array individually.
			{
				const ReflectionData *data = member->GetReflectionData();
				size_t baseTypeSize = data->GetSize();
				for (size_t ii = 0; ii < member->GetSize(); ii += baseTypeSize)
				{
					// Get the next element to serialize.
					void *offsetData = PointerOffset(variable->GetInstanceData(), member->GetOffset() + ii);
					ReflectedVariable arrayElement(data, offsetData);
					data->Deserialize(&arrayElement, stream, pointerTable, true);
				}
			}
			else // non-array/pointer type type.
			{
				void *offsetData = PointerOffset(variable->GetInstanceData(), member->GetOffset());
				ReflectedVariable memberVariable(member->GetReflectionData(), offsetData);
				member->GetReflectionData()->Deserialize(&memberVariable, stream, pointerTable, false);
			}
		}
	}

	// Add this variable to the pointer table if it hasn't already been added by a base type.
	if (!m_parent)
	{
		ReflectedVariable &tableVariable = pointerTable.GetPointer(tableIndex);
		tableVariable = *variable;
	}
}

void ReflectionData::SetSerializeFunction(SerializeFunction function)
{
    m_serializeFunction = function;
}

void ReflectionData::SetDeserializeFunction(DeserializeFunction function)
{
	m_deserializeFunction = function;
}

    
// ReflectionData implementation end ---------------------------------------------------------

} // namespace Qi

