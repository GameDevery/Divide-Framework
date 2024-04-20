///-------------------------------------------------------------------------------------------------
/// File:	include\Engine.h.
///
/// Summary:	Declares the engine class.
///-------------------------------------------------------------------------------------------------

#pragma once
#ifndef ECS__ECS_ENGINE_H__
#define ECS__ECS_ENGINE_H__

#include "API.h"

#include "Event/EventHandler.h"
#include "Event/EventDelegate.h"

namespace ECS
{
	namespace util
	{
		class Timer;
	}

	namespace Event
	{
		class IEvent;
		class IEventListener;
		class EventHandler;
	}

	class EntityManager;
	class SystemManager;
	class ComponentManager;
	struct Data;


	class ECS_API ECSEngine
	{
		friend class IEntity;
		friend class IComponent;
		friend class ISystem;

		friend class Event::IEvent;

		friend class Event::IEventListener;

		friend class EntityManager;

	private:

		util::Timer*				ECS_EngineTime;

		EntityManager*				ECS_EntityManager;

		ComponentManager*			ECS_ComponentManager;

		SystemManager*				ECS_SystemManager;

		Event::EventHandler*		ECS_EventHandler;


		ECSEngine(const ECSEngine&) = delete;
		ECSEngine& operator=(ECSEngine&) = delete;

		// Add event callback
		template<class E>
		inline void SubscribeEvent(Event::Internal::IEventDelegate* const eventDelegate)
		{
			ECS_EventHandler->AddEventCallback<E>(eventDelegate);
		}

		// Remove event callback
		void UnsubscribeEvent(Event::Internal::IEventDelegate* eventDelegate);

	public:

		ECSEngine();

		~ECSEngine();


        inline EntityManager* GetEntityManager() const { return ECS_EntityManager; }

		inline ComponentManager* GetComponentManager() { return ECS_ComponentManager; }
        inline ComponentManager* GetComponentManager() const { return ECS_ComponentManager; }

		inline SystemManager* GetSystemManager() { return ECS_SystemManager; }
        inline SystemManager* GetSystemManager() const { return ECS_SystemManager; }

		///-------------------------------------------------------------------------------------------------
		/// Fn:	template<class E, class... ARGS> void ECSEngine::SendEvent(ARGS&&... eventArgs)
		///
		/// Summary:	Broadcasts an event.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	3/10/2017
		///
		/// Typeparams:
		/// E - 	   	Type of the e.
		/// ARGS - 	   	Type of the arguments.
		/// Parameters:
		/// eventArgs - 	Variable arguments providing [in,out] The event arguments.
		///-------------------------------------------------------------------------------------------------

		template<class E, class... ARGS>
		void SendEvent(ARGS&&... eventArgs) const
		{
			ECS_EventHandler->Send<E>(this, FWD(eventArgs)...);
		}

        template<class E, class... ARGS>
        void SendEventAndDispatch(ARGS&&... eventArgs) const
        {
            ECS_EventHandler->SendAndDispatchEvent<E>(this, FWD(eventArgs)...);
        }
		///-------------------------------------------------------------------------------------------------
		/// Fn:	void ECSEngine::Update(f32 tick_ms);
		///
		/// Summary:	Updates the entire ECS with a given delta time in milliseconds.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	3/10/2017
		///
		/// Parameters:
		/// tick_ms - 	The tick in milliseconds.
		///-------------------------------------------------------------------------------------------------
        void PreUpdate(f32 tick_ms);
		void Update(f32 tick_ms);
        void PostUpdate(f32 tick_ms);
		void OnFrameStart();
		void OnFrameEnd();
	};

} // namespace ECS

#endif // ECS__ECS_ENGINE_H__
