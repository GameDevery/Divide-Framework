///-------------------------------------------------------------------------------------------------
/// File:	include\util\Timer.h.
///
/// Summary:	Declares the Timer class.
///-------------------------------------------------------------------------------------------------

#pragma once
#ifndef ECS__TIMER_H___
#define ECS__TIMER_H___

#include "API.h"

namespace ECS { namespace util {

	class ECS_API Timer
	{
		using Elapsed = std::chrono::duration<f32, std::milli>;

	private:

		Elapsed m_Elapsed;

	public:

		Timer();
		~Timer();

		///-------------------------------------------------------------------------------------------------
		/// Fn:	void Timer::Tick(DurationRep ms);
		///
		/// Summary:	Advances timer by adding elapsed milliseconds.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	3/10/2017
		///
		/// Parameters:
		/// ms - 	milliseconds (can be fractions).
		///-------------------------------------------------------------------------------------------------

		void Tick(f32 ms);

		///-------------------------------------------------------------------------------------------------
		/// Fn:	void Timer::Reset();
		///
		/// Summary:	Resets this timer to zero.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	3/10/2017
		///-------------------------------------------------------------------------------------------------

		void Reset();

		///-------------------------------------------------------------------------------------------------
		/// Fn:	inline TimeStamp Timer::GetTimeStamp() const
		///
		/// Summary:	Gets a TimeStamp from current timer value.
		///
		/// Author:	Tobias Stein
		///
		/// Date:	3/10/2017
		///
		/// Returns:	The time stamp.
		///-------------------------------------------------------------------------------------------------

		inline TimeStamp GetTimeStamp() const
		{
			return TimeStamp(this->m_Elapsed.count());
		}

	}; // class Timer

}} // namespace ECS::util

#endif // ECS__TIMER_H___

