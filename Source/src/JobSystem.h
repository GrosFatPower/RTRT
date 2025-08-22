#ifndef _JobSystem_
#define _JobSystem_

/*
 * Job system 
 * Derived from: https://github.com/turanszkij/JobSystem
 */

#include <functional>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>

namespace RTRT
{

class JobSystem
{
public:

  JobSystem() {}
  virtual ~JobSystem() {}

  static JobSystem & Get() { return _S_JobSystem; }

  void Initialize( unsigned int iNbThreads );

  void Execute( const std::function<void()> & iJob );

	bool IsBusy();

	void Wait();

protected:

  void Push( const std::function<void()> & iJob );
  bool Pop( std::function<void()> & oJob );
  void Poll();

  void Reset();

protected:

  static JobSystem                  _S_JobSystem; // Singleton

  unsigned int                      _NbThreads = 1;

  std::queue<std::function<void()>> _Jobs;
  std::mutex                        _JobsMutex;

  std::condition_variable           _WakeCondition;
  std::mutex                        _WakeMutex;
  bool                              _Stop = false;

  unsigned long long                _NbJobs;
  std::atomic<unsigned long long>   _NbFinishedJobs;
};

inline bool JobSystem::IsBusy() {
  return ( _NbFinishedJobs.load() < _NbJobs ); }

inline void JobSystem::Wait() {
  while ( IsBusy() ) Poll(); }

inline void JobSystem::Push( const std::function<void()> & iJob )
{
  std::unique_lock<std::mutex> lock(_JobsMutex);
  _Jobs.push(iJob);
  lock.unlock();
}

inline bool JobSystem::Pop( std::function<void()> & oJob )
{
  std::unique_lock<std::mutex> lock(_JobsMutex);

  if ( !_Jobs.empty() )
  {
    oJob = _Jobs.front();
    _Jobs.pop();
    return true;
  }
  return false;
}

inline void JobSystem::Poll()
{
  _WakeCondition.notify_one();
  std::this_thread::yield();
}

}

#endif /* _JobSystem_ */
