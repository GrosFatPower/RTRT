#include "JobSystem.h"


namespace RTRT
{

JobSystem JobSystem::_S_JobSystem;

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
void JobSystem::Initialize( unsigned int iNbThreads )
{
  Reset();

  _NbThreads = iNbThreads;
  _Stop = false;

  _NbFinishedJobs.store(0);

  for ( unsigned int threadID = 0; threadID < _NbThreads; ++threadID )
  {
    std::thread worker([this] {

      std::function<void()> job;

      while ( true )
      {
        if ( Pop(job) )
        {
          job();
          _NbFinishedJobs.fetch_add(1);
        }
        else
        {
          // No job, put thread to sleep
          std::unique_lock<std::mutex> lock(_WakeMutex);
          _WakeCondition.wait(lock);
        }

        if ( _Stop )
          return;
      }

    });

    worker.detach();
  }
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
void JobSystem::Reset()
{
  std::unique_lock<std::mutex> lock(_JobsMutex);
  std::queue<std::function<void()>>().swap(_Jobs);
  _NbJobs = 0;
  _Stop = true;
  lock.unlock();

  _WakeCondition.notify_all();

  Wait();
}

// ----------------------------------------------------------------------------
// Execute
// ----------------------------------------------------------------------------
void JobSystem::Execute( const std::function<void()> & iJob )
{
  _NbJobs++;

  Push(iJob);

  _WakeCondition.notify_one();
}

}
