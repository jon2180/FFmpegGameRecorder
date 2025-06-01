#pragma once

class FFMPEGGAMERECORDER_API IAVRecorderBase
{
public:
    virtual ~IAVRecorderBase()
    {
    }

    /** 主线程调用 */
    virtual void Register(UWorld* World) = 0;

    /** 主线程调用 */
    virtual void Unregister() = 0;

    virtual void Setup()
    {
    }

    /** 主线程调用 */
    virtual void Teardown()
    {
    };
};
