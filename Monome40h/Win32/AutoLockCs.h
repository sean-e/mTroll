#ifndef AutoLockCs_h__
#define AutoLockCs_h__


class AutoLockCs
{
	CRITICAL_SECTION & mLock;

public:
	AutoLockCs(CRITICAL_SECTION & lock) : mLock(lock) {::EnterCriticalSection(&mLock);}
	~AutoLockCs() {::LeaveCriticalSection(&mLock);}
};

#endif // AutoLockCs_h__
