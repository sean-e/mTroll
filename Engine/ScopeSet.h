#ifndef ScopeSet_h__
#define ScopeSet_h__


template<typename T>
class ScopeSet
{
	T * mVar;
	T	mOldValue;
	T	mNewValue;
	bool mCondition;

public:
	ScopeSet(T * theVar, T newValue = 0, bool condition = true) : 
		mVar(theVar),
		mNewValue(newValue),
		mOldValue(*theVar),
		mCondition(condition)
	{ 
		if (mCondition) 
			*mVar = mNewValue;
	}

	~ScopeSet() { Restore(); }

	void Restore()
	{
		if (mCondition)
		{
			*mVar = mOldValue;
			mCondition = false;
		}
	}
};

#endif // ScopeSet_h__
