/*
	Author : Tobias Stein
	Date   : 2nd July, 2016
	File   : Component.h
	
	Base component class which provides a unique id.

	All Rights Reserved. (c) Copyright 2016.
*/

#pragma once
#ifndef __COMPONENT_H__
#define __COMPONENT_H__

#include "API.h"

#include "IComponent.h"
#include "util/FamilyTypeID.h"

namespace ECS
{
	template<class T>
	class Component : public IComponent
	{

	public:

		static const ComponentTypeId STATIC_COMPONENT_TYPE_ID;

		Component() 
		{}

		virtual ~Component()
		{}		

		inline ComponentTypeId GetStaticComponentTypeID() const
		{
			return STATIC_COMPONENT_TYPE_ID;
		}	

		virtual void OnData(const CustomEvent& data)
		{
			(void)data;
		}
	};

	// This private member only exists to force the compiler to create an instance of Component T,
	// which will set its unique identifier.
	template<class T>
	const ComponentTypeId Component<T>::STATIC_COMPONENT_TYPE_ID = util::Internal::FamilyTypeID<IComponent>::Get<T>();
}

#endif // __COMPONENT_H__