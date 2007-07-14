#ifndef SEHexception_h__
#define SEHexception_h__


struct SEHexception
{
    SEHexception() { }
    SEHexception(unsigned int n) : mSEnum(n) { }
    unsigned int mSEnum;
};


void trans_func(unsigned int u, EXCEPTION_POINTERS* pExp);

#endif // SEHexception_h__
