#pragma once
#include <isxdk.h>


class ISXEQAutologin :
    public ISXInterface
{
public:

    virtual bool Initialize(ISInterface *p_ISInterface);
    virtual void Shutdown();

    void LoadSettings();
    void ConnectServices();
    void RegisterCommands();

    void DisconnectServices();
    void UnRegisterCommands();

};

extern ISInterface *pISInterface;
extern HISXSERVICE hPulseService;
extern HISXSERVICE hServicesService;

extern HISXSERVICE hEQGamestateService;

extern ISXEQAutologin *pExtension;
#define printf pISInterface->Printf

extern LSType *pStringType;
extern LSType *pIntType;
extern LSType *pBoolType;
extern LSType *pFloatType;
extern LSType *pTimeType;
extern LSType *pByteType;
