/*
	Author : Tobias Stein
	Date   : 3rd July, 2016
	File   : IComponent.h
	
	Interface class for component class

	All Rights Reserved. (c) Copyright 2016.
*/

#pragma once
#ifndef ECS__I_COMPONENT_H__
#define ECS__I_COMPONENT_H__

#include "IEntity.h"

namespace ECS
{
	struct CustomEvent;

	using ComponentId		= ObjectID;
	using ComponentTypeId	= TypeID;

	static const ComponentId INVALID_COMPONENT_ID = INVALID_OBJECT_ID;

	template<class T>
	class Component;

	class ECS_API IComponent
	{
		friend class ComponentManager;

	protected:
		
		ComponentId		m_HashValue;

		ComponentId		m_ComponentID;

		EntityId		m_Owner;

		bool			m_Enabled;

	public:

		IComponent();

		virtual ~IComponent();


		// COMPARE
		inline bool operator==(const IComponent& other) const { return m_HashValue == other.m_HashValue; }
		inline bool operator!=(const IComponent& other) const { return m_HashValue == other.m_HashValue; }
		

		// ACCESSOR

		inline ComponentId GetComponentId() const { return this->m_ComponentID; }

		inline EntityId GetOwner() const { return this->m_Owner; }

		inline void SetActive(bool state) { this->m_Enabled = state; }
		inline bool IsActive() const { return this->m_Enabled; }

		virtual void OnData(const CustomEvent& data) = 0;
	};
}

#endif // ECS__I_COMPONENT_H__