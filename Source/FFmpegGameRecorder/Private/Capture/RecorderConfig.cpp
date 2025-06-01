
#include "Capture/RecorderConfig.h"

FEncodeData::FEncodeData(): StartSec(0), Duration(0)
{
}

FEncodeData::~FEncodeData()
{
	
}

void FEncodeData::Initialize(uint32 Size)
{
	Data.SetNumUninitialized(Size);
}



