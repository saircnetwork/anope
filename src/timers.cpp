/* Timer stuff.
 *
 * (C) 2003-2013 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 */

#include "services.h"
#include "timers.h"

std::multimap<time_t, Timer *> TimerManager::Timers;

Timer::Timer(long time_from_now, time_t now, bool repeating)
{
	trigger = now + time_from_now;
	secs = time_from_now;
	repeat = repeating;
	settime = now;

	TimerManager::AddTimer(this);
}

Timer::~Timer()
{
	TimerManager::DelTimer(this);
}

void Timer::SetTimer(time_t t)
{
	TimerManager::DelTimer(this);
	trigger = t;
	TimerManager::AddTimer(t);
}

time_t Timer::GetTimer() const
{
	return trigger;
}

bool Timer::GetRepeat() const
{
	return repeat;
}

time_t Timer::GetSetTime() const
{
	return settime;
}

void Timer::SetSecs(time_t t)
{
	TimerManager::DelTimer(this);
	secs = t;
	trigger = Anope::CurTime + t;
	TimerManager::AddTimer(this);
}

long Timer::GetSecs() const
{
	return secs;
}

void TimerManager::AddTimer(Timer *t)
{
	Timers.insert(std::make_pair(t->GetTimer(), t));
}

void TimerManager::DelTimer(Timer *t)
{
	std::pair<std::multimap<time_t, Timer *>::iterator, std::multimap<time_t, Timer *>::iterator> itpair = Timers.equal_range(t->GetTimer());
	for (std::multimap<time_t, Timer *>::iterator i = itpair.first; i != itpair.second; ++i)
	{
		if (i->second == t)
		{
			Timers.erase(i);
			break;
		}
	}
}

void TimerManager::TickTimers(time_t ctime)
{
	while (!Timers.empty())
	{
		std::multimap<time_t, Timer *>::iterator it = Timers.begin();
		Timer *t = it->second;

		if (t->GetTimer() > ctime)
			break;

		t->Tick(ctime);

		if (t->GetRepeat())
			t->SetTimer(ctime + t->GetSecs());
		else
			delete t;
	}
}
