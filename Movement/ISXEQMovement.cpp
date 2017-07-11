//
// ISXEQMovement
//
#pragma warning(disable:4996)
#include "../ISXEQClient.h"
#include "ISXEQMovement.h"

// The mandatory pre-setup function.  Our name is "ISXEQMovement", and the class is ISXEQMovement.
// This sets up a "ModulePath" variable which contains the path to this module in case we want it,
// and a "PluginLog" variable, which contains the path and filename of what we should use for our
// debug logging if we need it.  It also sets up a variable "pExtension" which is the pointer to
// our instanced class.
ISXPreSetup("ISXEQMovement",ISXEQMovement);

// Basic LavishScript datatypes, these get retrieved on startup by our initialize function, so we can
// use them in our Top-Level Objects or custom datatypes
LSType *pStringType=0;
LSType *pIntType=0;
LSType *pBoolType=0;
LSType *pFloatType=0;
LSType *pTimeType=0;
LSType *pByteType=0;

LSType *pMakeCampType = 0;
LSType *pStickType = 0;
LSType *pMoveToType = 0;
LSType *pCircleType = 0;
LSType *pMovementType = 0;

ISInterface *pISInterface=0;
HISXSERVICE hPulseService=0;
HISXSERVICE hMemoryService=0;
HISXSERVICE hServicesService=0;
HISXSERVICE hTriggerService=0;

HISXSERVICE hEQChatService=0;
HISXSERVICE hEQUIService=0;
HISXSERVICE hEQGamestateService=0;
HISXSERVICE hEQSpawnService=0;
HISXSERVICE hEQZoneService=0;

unsigned int ISXEQMovementXML=0;

// Forward declarations of callbacks
void __cdecl PulseService(bool Broadcast, unsigned int MSG, void *lpData);
void __cdecl MemoryService(bool Broadcast, unsigned int MSG, void *lpData);
void __cdecl ServicesService(bool Broadcast, unsigned int MSG, void *lpData);

inline unsigned char FindPointers();

// Initialize is called by Inner Space when the extension should initialize.
bool ISXEQMovement::Initialize(ISInterface *p_ISInterface)
{    
	pISInterface=p_ISInterface;

	// retrieve basic ISData types
	pStringType=pISInterface->FindLSType("string");
	pIntType=pISInterface->FindLSType("int");
	pBoolType=pISInterface->FindLSType("bool");
	pFloatType=pISInterface->FindLSType("float");
	pTimeType=pISInterface->FindLSType("time");
	pByteType=pISInterface->FindLSType("byte");

	// instance classes
	ME     = new CMUCharacter();
	SET    = new CMUSettings();
	MOVE   = new CMUMovement();
	pMU    = new CMUActive();

	pISInterface->OpenSettings(XMLFileName);
	LoadSettings();

	// setup global vars
	sprintf(szDebugName, "%s\\ISXEQMovement-debug.ini", gszINIPath);

	ConnectServices();

	RegisterCommands();
	RegisterAliases();
	RegisterDataTypes();
	RegisterTopLevelObjects();
	RegisterServices();

	// offset-driven movement
	unsigned char ucFailedLoad = FindPointers();
	if (ucFailedLoad)
	{
		char szFailOffset[500] = {0};
		sprintf(szFailOffset, "\ay%s\aw:: Couldn't find movement pointer: \ar%s\ax.", MODULE_NAME, szFailedLoad[ucFailedLoad]);
		WriteChatf(szFailOffset);
		MessageBox(NULL, szFailOffset, "ISXEQMovement v1.x", MB_OK);
		bOffsetOverride = true;
	}

	srand((unsigned int)time(0));

	WriteChatf("ISXEQMovement v%1.2f Loaded", MODULE_VERSION);
	return true;
}

// shutdown sequence
void ISXEQMovement::Shutdown()
{
	pISInterface->UnloadSet(ISXEQMovementXML);

	// destroy mq2 linkage
	UndoKeybinds();
	SetupEvents(false, true);

	// do not leave character walking
	MOVE->SetWalk(false);

	// destroy classes
	delete ME;
	delete MOVE;
	delete pMU;
	delete SET;

	DisconnectServices();

	UnRegisterServices();
	UnRegisterTopLevelObjects();
	UnRegisterDataTypes();
	UnRegisterAliases();
	UnRegisterCommands();
}


void ISXEQMovement::ConnectServices()
{
	// connect to any services.  Here we connect to "Pulse" which receives a
	// message every frame (after the frame is displayed) and "Memory" which
	// wraps "detours" and memory modifications
	hPulseService=pISInterface->ConnectService(this,"Pulse",PulseService);
	hMemoryService=pISInterface->ConnectService(this,"Memory",MemoryService);
	hServicesService=pISInterface->ConnectService(this,"Services",ServicesService);
}

void ISXEQMovement::RegisterCommands()
{
	pISInterface->AddCommand("makecamp", HandleCommand);
	pISInterface->AddCommand("moveto", HandleCommand);
	pISInterface->AddCommand("stick", HandleCommand);
	pISInterface->AddCommand("circle", HandleCommand);
	pISInterface->AddCommand("calcangle", CalcOurAngle);
	pISInterface->AddCommand("rootme", RootCmd);
}

void ISXEQMovement::RegisterAliases()
{
	// add any aliases
}

void ISXEQMovement::RegisterDataTypes()
{
	pMakeCampType = new IS_MakeCampType;
	pStickType = new IS_StickType;
	pMoveToType = new IS_MoveToType;
	pCircleType = new IS_CircleType;
	pMovementType = new IS_MovementType;

	pISInterface->AddLSType(*pMakeCampType);
	pISInterface->AddLSType(*pStickType);
	pISInterface->AddLSType(*pMoveToType);
	pISInterface->AddLSType(*pCircleType);
	pISInterface->AddLSType(*pMovementType);
}

void ISXEQMovement::RegisterServices()
{
	// register any services.  Here we demonstrate a service that does not use a
	// callback
	// set up a 1-way service (broadcast only)
	//    hISXEQMovementService=pISInterface->RegisterService(this,"ISXEQMovement Service",0);
	// broadcast a message, which is worthless at this point because nobody will receive it
	// (nobody has had a chance to connect)
	//    pISInterface->ServiceBroadcast(this,hISXEQMovementService,ISXSERVICE_MSG+1,0);

}

void ISXEQMovement::DisconnectServices()
{
	// gracefully disconnect from services
	if (hPulseService)
		pISInterface->DisconnectService(this,hPulseService);
	if (hMemoryService)
	{
		pISInterface->DisconnectService(this,hMemoryService);
		// memory modifications are automatically undone when disconnecting
		// also, since this service accepts messages from clients we should reset our handle to
		// 0 to make sure we dont try to continue using it
		hMemoryService=0; 
	}
	if (hServicesService)
		pISInterface->DisconnectService(this,hServicesService);
}

void ISXEQMovement::UnRegisterCommands()
{
	pISInterface->RemoveCommand("makecamp");
	pISInterface->RemoveCommand("moveto");
	pISInterface->RemoveCommand("stick");
	pISInterface->RemoveCommand("circle");
	pISInterface->RemoveCommand("calcangle");
	pISInterface->RemoveCommand("rootme");
}

void ISXEQMovement::UnRegisterAliases()
{
	// remove aliases
}

void ISXEQMovement::UnRegisterDataTypes()
{
	if (pMakeCampType) {
		pISInterface->RemoveLSType(*pMakeCampType);
		delete pMakeCampType;
	}
	if (pStickType) {
		pISInterface->RemoveLSType(*pStickType);
		delete pStickType;
	}
	if (pMoveToType) {
		pISInterface->RemoveLSType(*pMoveToType);
		delete pMoveToType;
	}
	if (pCircleType) {
		pISInterface->RemoveLSType(*pCircleType);
		delete pCircleType;
	}
	if (pMovementType) {
		pISInterface->RemoveLSType(*pMovementType);
		delete pMovementType;
	}
}

void ISXEQMovement::UnRegisterTopLevelObjects()
{
	pISInterface->RemoveTopLevelObject("Stick");
	pISInterface->RemoveTopLevelObject("MakeCamp");
	pISInterface->RemoveTopLevelObject("MoveTo");
	pISInterface->RemoveTopLevelObject("Circle");
	pISInterface->RemoveTopLevelObject("Movement");
}

void ISXEQMovement::UnRegisterServices()
{
	// shutdown our own services
	//    if (hISXEQMovementService)
	//        pISInterface->ShutdownService(this,hISXEQMovementService);
}

void ISXEQMovement::LoadSettings()
{
	ISXEQMovementXML=pISInterface->OpenSettings(XMLFileName);
	LoadConfig();
}

void __cdecl MemoryService(bool Broadcast, unsigned int MSG, void *lpData)
{
	// no messages are currently associated with this service (other than
	// system messages such as client disconnect), so do nothing.
}

void __cdecl EQChatService(bool Broadcast, unsigned int MSG, void *lpData)
{
#define pChat ((_EQChat*)lpData)
	switch(MSG)
	{
	case CHATSERVICE_OUTGOING:
		// same as OnWriteChatColor
		break;
	case CHATSERVICE_INCOMING:
		// same as OnIncomingChat
		break;
	}
#undef pChat
}

// -----------------------
// import / export

//PLUGIN_API bool bStickOn = false; // stick active or off, exported for other plugins to have a sure way of always knowing
//PLUGIN_API void StickCommand(PSPAWNINFO pLPlayer, char* szLine); // exported wrapper for MQ2Melee support
// note to any developers: if you intend to use these exports and want to shut off stick, do not flip STICK->On directly,
// instead, call StickCommand(pLPlayer, "off")
//bool* pbMULoaded = NULL; // imported from mq2melee in InitializePlugin()

// ----------------------------------------
// function prototypes

void SpewMUError(unsigned char ucErrorNum);
void SpewDebug(unsigned char ucDbgType, char* szOuput, ...);
void OutputHelp(unsigned char ucCmdUsed, bool bOnlyCmdHelp = false);
void WriteLine(char szOutput[MAX_STRING], VERBLEVEL V_COMPARE);
void EndPreviousCmd(bool bKillMovement, unsigned char ucCmdUsed = APPLY_TO_ALL, bool bPreserveSelf = false);
int ChangeSetting(int argc, char *argv[]);
void DebugToWnd(unsigned char ucCmdUsed);
void DebugToINI(unsigned char ucCmdUsed);
void DebugToDebugger(char* szFormat, ...);
void SetupEvents(bool bAddEvent, bool bForceRemove = false);
inline bool ValidIngame(bool bCheckDead = true);
dtNavMesh* LoadMesh();

// CMUDelay methods
void CMUDelay::TimeStop()
{
	Resume = T_INACTIVE;
}

void CMUDelay::TimeStart()
{
	GetSystemTime(&Began);
	Resume = rand() % (Max - Min + 1) + Min;
}

char CMUDelay::TimeStatus()
{
	if (Resume == T_INACTIVE)
	{
		return T_INACTIVE;
	}
	if (ElapsedMS() >= Resume)
	{
		return T_READY;
	}
	return T_WAITING;
}

void CMUDelay::Validate()
{
	MinDelay(Min);
	MaxDelay(Max);
}

void CMUDelay::MinDelay(int iNew)
{
	Min = iNew;
	if (Min < 125) Min = 125;
}

void CMUDelay::MaxDelay(int iNew)
{
	Max = iNew;
	if (Max < Min + 125) Max = Min + 125;
}

int CMUDelay::ElapsedMS()
{
	SYSTEMTIME     stCurr, stResult;
	FILETIME       ftPrev, ftCurr, ftResult;
	ULARGE_INTEGER prev,   curr,   result;

	GetSystemTime(&stCurr);
	SystemTimeToFileTime(&Began, &ftPrev);
	SystemTimeToFileTime(&stCurr, &ftCurr);
	prev.HighPart           = ftPrev.dwHighDateTime;
	prev.LowPart            = ftPrev.dwLowDateTime;
	curr.HighPart           = ftCurr.dwHighDateTime;
	curr.LowPart            = ftCurr.dwLowDateTime;
	result.QuadPart         = curr.QuadPart - prev.QuadPart;
	ftResult.dwHighDateTime = result.HighPart;
	ftResult.dwLowDateTime  = result.LowPart;
	FileTimeToSystemTime(&ftResult, &stResult);
	return ((int)(stResult.wSecond * 1000 + stResult.wMilliseconds));
}

// CMUCharacter methods
bool CMUCharacter::IsBard()
{
	PCHARINFO2 pChar = GetCharInfo2();
	if (pChar && pChar->Class == Bard)
	{
		return true;
	}
	return false;
}

bool CMUCharacter::InCombat()
{
	if (ValidIngame() && ((PCPLAYERWND)pPlayerWnd)->CombatState == 0 && ((CXWnd*)pPlayerWnd)->GetChildItem("PW_CombatStateAnim"))
	{
		return true;
	}
	return false;
}

bool CMUCharacter::IsMe(PSPAWNINFO pCheck)
{
	if (!pCheck || !pLocalPlayer) return false;
	if (pCheck->SpawnID == ((PSPAWNINFO)pCharSpawn)->SpawnID || pCheck->SpawnID == ((PSPAWNINFO)pLocalPlayer)->SpawnID)
	{
		return true;
	}
	return false;
}

// CStuckLogic methods
void CStuckLogic::Reset()
{
	Y = X = Z = 0.0f;
	DifDist   = 0.0f;
	CurDist   = 1.0f;
	StuckInc  = 0;
	StuckDec  = 0;
}

// CCircleSettings methods
void CCircleSettings::SetRadius(float fNew)
{
	// enforce min radius size 5.0f
	Radius = fNew;
	if (Radius < 5.0f) Radius = 5.0f;
}

// CCampSettings methods
void CCampSettings::SetRadius(float fNew)
{
	Radius = fNew;
	ValidateSizes();
}

void CCampSettings::SetLeash(float fNew)
{
	Length = fNew;
	ValidateSizes();
}

void CCampSettings::ValidateSizes()
{
	if (Radius < 5.0f)  Radius = 5.0f;  // enforce min Radius size 5.0f
	float fTemp = Radius + 5.0f;
	if (Length < fTemp) Length = fTemp; // enforce min leash size 5.0f >= Radius
}

// CMoveToCmd methods
void CMoveToCmd::Activate(float fY, float fX, float fZ)
{
	// we load the mesh on zoning, but lets check and try again just in case
	if (pMU->navMesh == NULL) {
		pMU->navMesh = LoadMesh();
	}
	if (pMU->navMesh != NULL) {
		int numPoints = FindPath(fX, fY, fZ, currentPath);
		if (numPoints > 0) {
			currentPathSize = numPoints - 1;
			currentPathCursor = 1;
			fX = currentPath[currentPathCursor*3];
			fY = currentPath[currentPathCursor*3+2];
			fZ = 0.0f;
			UsingPath = true;
			// since we're using pathing, we don't want to walk when we get near each waypoint
			// also we want to be much closer to the waypoint before turning
			Walk = false;
			Dist = 1.5f;
		}
	}
	Y  = fY;
	X  = fX;
	Z  = fZ;
	On = true;
}

int CMoveToCmd::FindPath(float X, float Y, float Z, float* pPath) {
	if (pMU->navMesh == NULL)
		return 0;

	PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
	int numPoints = 0;
	static dtQueryFilter filter;
	static float extents[3] = { 50, 400, 50 }; // Note: X, Z, Y
	float endOffset[3] = {X, Z, Y};
	float startOffset[3] = { pChSpawn->X, pChSpawn->Z, pChSpawn->Y };
	float spos[3];
	float epos[3];
	filter.includeFlags = 0x01; // walkable surfaces
	if (dtPolyRef startRef = pMU->navMesh->findNearestPoly(startOffset, extents, &filter, spos)) {
		if (dtPolyRef endRef = pMU->navMesh->findNearestPoly(endOffset, extents, &filter, epos)) {
			dtPolyRef polys[MAX_POLYS];
			int numPolys = pMU->navMesh->findPath(startRef, endRef,spos, epos, &filter, polys, MAX_POLYS);
			if (numPolys > 0)
				numPoints = pMU->navMesh->findStraightPath(spos, epos, polys, numPolys, pPath, NULL, NULL, MAX_POLYS);
		} else {
			sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) No end reference, not using meshNav", MODULE_NAME);
			WriteLine(szMsg, V_ERRORS);
		}
	} else {
		sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) No start reference, not using meshNav.", MODULE_NAME);
		WriteLine(szMsg, V_ERRORS);
	}
	return numPoints;
}

void CMoveToCmd::UserDefaults()
{
	BreakAggro = SET_M->BreakAggro;
	BreakHit   = SET_M->BreakHit;
	UseBack    = SET_M->UseBack;
	UW         = SET_M->UW;
	Walk       = SET_M->Walk;
	Dist       = SET_M->Dist;
	DistBack   = SET_M->DistBack;
	DistY      = SET_M->DistY;
	DistX      = SET_M->DistX;
	Mod        = SET_M->Mod;
}

// CCampHandler methods
void CCampHandler::ResetBoth()
{
	delete ALTCAMP;
	ALTCAMP = new CAltCamp();
	delete CURCAMP;
	CURCAMP = new CCampCmd();
}

void CCampHandler::ResetCamp(bool bOutput)
{
	ALTCAMP->Update(CURCAMP);
	NewCamp(bOutput);
}

void CCampHandler::ResetPlayer(bool bOutput)
{
	NewCamp(false);
	if (bOutput) OutputPC();
}

void CCampHandler::NewCamp(bool bOutput)
{
	if (ValidIngame() && MOVETO->On && Returning)
	{
		// kill active camp return
		EndPreviousCmd(true);
	}
	delete CURCAMP;
	CURCAMP   = new CCampCmd();
	VarReset();
	if (bOutput) Output();
}

void CCampHandler::Activate(float fY, float fX)
{
	if (CURCAMP->On && !CURCAMP->Pc)
	{
		ResetCamp(false);
	}
	else
	{
		NewCamp(false);
	}
	CURCAMP->On = true;
	CURCAMP->Y  = fY;
	CURCAMP->X  = fX;
	Validate(); // CMUDelay
}

void CCampHandler::ActivatePC(PSPAWNINFO pCPlayer)
{
	Activate(pCPlayer->Y, pCPlayer->X);
	sprintf(CURCAMP->PcName, "%s", pCPlayer->DisplayedName);
	CURCAMP->Pc     = true;
	CURCAMP->PcID   = pCPlayer->SpawnID;
	CURCAMP->PcType = GetSpawnType(pCPlayer);
}

void CCampHandler::VarReset()
{
	Auto      = false;
	DoAlt     = false;
	DoReturn  = false;
	Returning = false;
}

void CCampHandler::Output()
{
	sprintf(szMsg, "\ay%s\aw:: MakeCamp off.", MODULE_NAME);
	WriteLine(szMsg, V_MAKECAMPV);
}

void CCampHandler::OutputPC()
{
	sprintf(szMsg, "\ay%s\aw:: MakeCamp player off.", MODULE_NAME);
	WriteLine(szMsg, V_MAKECAMPV);
}

//CAltCamp methods
void CAltCamp::Update(CCampCmd* Cur)
{
	Y      = Cur->Y;
	X      = Cur->X;
	Radius = Cur->Radius;
	On     = true;
}

// CCircleCmd methods
bool CCircleCmd::Wait()
{
	// drunken circling uses this formula
	if (ElapsedMS() > Max + GetDrunk(Min))
	{
		TimeStart();
		return false;
	}
	return true;
}

void CCircleCmd::AtMe()
{
	// HandleOurCmd calls this to establish '/circle on' without loc supplied
	PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
	Y = pChSpawn->Y + Radius * sin(pChSpawn->Heading * (float)PI / HEADING_HALF);
	X = pChSpawn->X + Radius * cos(pChSpawn->Heading * (float)PI / HEADING_HALF);
	On = true;
}

void CCircleCmd::AtLoc(float fY, float fX)
{
	// HandleOurCmd calls this with desired Y X supplied
	Y  = fY;
	X  = fX;
	On = true;
}

void CCircleCmd::UserDefaults()
{
	Backward = SET_C->Backward;
	CCW      = SET_C->CCW;
	Drunk    = SET_C->Drunk;
	CMod     = SET_C->CMod;
	Radius   = SET_C->Radius;
}

int CCircleCmd::GetDrunk(int iNum)
{
	return (iNum * rand() / (RAND_MAX + 1));
}

// CStickCmd methods
void CStickCmd::TurnOn()
{
	On       = true;
}

void CStickCmd::StopHold()
{
	HoldID   = NULL;
	HoldType = NONE;
	Hold     = false;
}

void CStickCmd::FirstAlways()
{
	// reset hold values, dont allow 'hold' or 'id' with 'always'
	StopHold();
	Always = true;
	TurnOn();
	if (pTarget)
	{
		HaveTarget = true;
		return;
	}
	HaveTarget = false;
}

void CStickCmd::NewSnaproll()
{
	delete Snap;
	Snap = new CSnaproll();
}

void CStickCmd::ResetLoc()
{
	Y = X = Z = 0.0f;
	CurDist = DifDist = 0.0f;
}

bool CStickCmd::Ready()
{
	if (Always)
	{
		return AlwaysStatus();
	}
	return On;
}

void CStickCmd::DoRandomize()
{
	if (!Randomize) return;
	if (NotFront)
	{
		SetRandArc(NOT_FRONT_ARC);
	}
	else if (Behind)
	{
		SetRandArc(BEHIND_ARC);
	}
	else if (Pin)
	{
		SetRandArc(PIN_ARC_MIN);
	}
}

void CStickCmd::UserDefaults()
{
	Min         = SET_S->Min;
	Max         = SET_S->Max;
	BreakGate   = SET_S->BreakGate;
	BreakTarget = SET_S->BreakTarget;
	BreakWarp   = SET_S->BreakWarp;
	PauseWarp   = SET_S->PauseWarp;
	Randomize   = SET_S->Randomize;
	DelayStrafe = SET_S->DelayStrafe;
	UseBack     = SET_S->UseBack;
	UseFleeing  = SET_S->UseFleeing;
	UW          = SET_S->UW;
	Walk        = SET_S->Walk;
	ArcBehind   = SET_S->ArcBehind;
	ArcNotFront = SET_S->ArcNotFront;
	DistBack    = SET_S->DistBack;
	DistBreak   = SET_S->DistBreak;
	DistMod     = SET_S->DistMod;
	DistModP    = SET_S->DistModP;
	DistSnap    = SET_S->DistSnap;
}

void CStickCmd::SetRandArc(int iArcType)
{
	float  fTempArc    = 0.0f;
	float  fArcSize    = 0.0f;
	float* pfRandomArc = NULL;
	float* pfStableArc = NULL;

	RandFlag = !RandFlag;
	if (RandFlag)
	{
		pfRandomArc = &RandMin;
		pfStableArc = &RandMax;
	}
	else
	{
		pfRandomArc = &RandMax;
		pfStableArc = &RandMin;
	}

	switch (iArcType)
	{
	case PIN_ARC_MIN: // 112 to 144  ---  144 - 112 = 32 total size
		fTempArc = (float)(rand() % 32 + 5);
		fArcSize = (float)(rand() % 32 + 16);
		break;
	case BEHIND_ARC:
		fTempArc = (float)(rand() % 45 + 5);
		fArcSize = (float)(rand() % 90 + 40);
		break;
	case NOT_FRONT_ARC:
		fTempArc = (float)(rand() % 135 + 5);
		fArcSize = (float)(rand() % 270 + 80);
		break;
	}

	*pfRandomArc = fTempArc;
	*pfStableArc = fArcSize;

	sprintf(szMsg, "\ay%s\aw:: Arcs Randomized! Max: %.2f  Min: %.2f", MODULE_NAME, RandMax, RandMin);
	WriteLine(szMsg, V_RANDOMIZE);
}

// CPauseHandler methods

bool CPauseHandler::Waiting()
{
	if (PausedCmd || PausedMU) return true;
	return false;
}

void CPauseHandler::HandlePause()
{
	if (PausedCmd) return;
	if (MouseCheck()) return;
	MouseFree();
	PauseTimers();
}

void CPauseHandler::PauseTimers()
{
	if (PausedMU && !UserKB)
	{
		// verify we didnt ditch our stick target
		if (STICK->On && (!STICK->Always || (STICK->Always && STICK->HaveTarget)))
		{
			PSPAWNINFO psTarget = (PSPAWNINFO)(STICK->Hold ? GetSpawnByID(STICK->HoldID) : pTarget);
			if (!psTarget || (STICK->Hold && STICK->HoldType != GetSpawnType(psTarget)))
			{
				Reset();
				EndPreviousCmd(true);
				sprintf(szMsg, "\ay%s\aw:: You are no longer sticking to anything.", MODULE_NAME);
				WriteLine(szMsg, V_STICKV);
				return;
			}
		}
		switch(TimeStatus())
		{
		case T_INACTIVE:
		case T_WAITING:
			break;
		case T_READY:
		default:
			PausedMU = false;
			if (!CAMP->Auto)
			{
				sprintf(szMsg, "\ay%s\aw:: Resuming previous command from movement pause.", MODULE_NAME);
				WriteLine(szMsg, V_MOVEPAUSE);
			}
			Reset();
			break;
		}
	}
}

void CPauseHandler::MouseFree()
{
	if (HandleMouse)
	{
		HandleMouse = false;
		if (PauseNeeded())
		{
			TimeStart();
		}
	}
}

bool CPauseHandler::PauseNeeded()
{
	if (CIRCLE->On || STICK->On || MOVETO->On || CAMP->Auto)
	{
		if (STICK->On && (!STICK->Always || (STICK->Always && STICK->HaveTarget)))
		{
			PSPAWNINFO psTarget = (PSPAWNINFO)(STICK->Hold ? GetSpawnByID(STICK->HoldID) : pTarget);
			if (!psTarget || (STICK->Hold && STICK->HoldType != GetSpawnType(psTarget)))
			{
				Reset();
				if (!UserKB) PausedMU = false;
				EndPreviousCmd(true);
				sprintf(szMsg, "\ay%s\aw:: You are no longer sticking to anything.", MODULE_NAME);
				WriteLine(szMsg, V_STICKV);
				return false;
			}
		}
		return true;
	}
	return false;
}


// CMUMovement methods

void CMUMovement::AutoHead()   // called OnPulse to adjust heading for loose/true
{
	if (ChangeHead == H_INACTIVE) return;
	TurnHead(ChangeHead);
}

void CMUMovement::NewHead(float fNewHead)
{
	// this is called to apply new heading changes
	// if loose/true, set ChangeHead to be adjusted appropriately OnPulse
	// if fast, apply change immediately
	if (!ValidIngame()) return;
	switch (pMU->Head)
	{
	case H_LOOSE:
	case H_TRUE:
		ChangeHead = fNewHead;
		break;
	case H_FAST:
	default:
		FastTurn(fNewHead);
		break;
	}
}

void CMUMovement::NewFace(double dNewFace)
{
	// set look angle adjustments (uw param & autouw)
	// loose/true: let mq2's gLookAngle auto-adjust for us
	// fast: apply change immediately
	if (!ValidIngame()) return;
	switch (pMU->Head)
	{
	case H_LOOSE:
	case H_TRUE:
		gLookAngle = dNewFace;
		break;
	case H_FAST:
	default:
		((PSPAWNINFO)pCharSpawn)->CameraAngle = (float)dNewFace;
		break;
	}
}

void CMUMovement::StopHeading()
{
	// if loose/true is currently trying to adjust head
	// set adjustment to current heading to halt it
	// we do it this way because true heading holds down the turn keys
	if (ChangeHead != H_INACTIVE)
	{
		TurnHead(((PSPAWNINFO)pCharSpawn)->Heading);
	}
}

float CMUMovement::SaneHead(float fHeading)
{
	// places new heading adjustments within bounds
	if (fHeading >= HEADING_MAX) fHeading -= HEADING_MAX;
	if (fHeading < 0.0f)         fHeading += HEADING_MAX;
	return fHeading;
}

void CMUMovement::DoRoot()
{
	// turns '/rootme' on
	if (!pMU->Rooted || !ValidIngame()) return;

	if (SET->WinEQ || bOffsetOverride)
	{
		StopRoot();
		return;
	}
	ChangeHead = H_INACTIVE;
	((PSPAWNINFO)pCharSpawn)->Heading          = RootHead;
	((PSPAWNINFO)pCharSpawn)->SpeedHeading     = 0.0f;
	pKeypressHandler->CommandState[iTurnLeft]  = 0;
	*pulTurnLeft                               = 0;
	pKeypressHandler->CommandState[iTurnRight] = 0;
	*pulTurnRight                              = 0;
	TrueMoveOff(APPLY_TO_ALL);
}

void CMUMovement::StopRoot()
{
	if (!pMU->Rooted) return;
	// turns '/rootme' off
	pMU->Rooted = false;
	RootHead    = 0.0f;
	char szTempOut[MAX_STRING] = {0};
	sprintf(szTempOut, "\ay%s\aw:: You are no longer rooted.", MODULE_NAME);
	WriteLine(szTempOut, V_SILENCE);
}

float CMUMovement::AngDist(float fH1, float fH2)
{
	// calculates current angular heading distance between two headings
	// used primarily by strafing stick cmds
	if(fabs(fH1 - fH2) > HEADING_HALF)
	{
		(fH1 < fH2 ? fH1 : fH2) += HEADING_MAX;
	}
	return (fabs(fH1 - fH2) > HEADING_HALF) ? (fH2 - fH1) : (fH1 - fH2);
}

bool CMUMovement::CanMove(float fHead, float fY, float fX)
{
	// anti-orbit formula credit: deadchicken
	// should always return true for fast heading
	// loose/true heading will not allow movement until
	// within a close enough turn to prevent orbiting around
	// a destination or running away from a close destination
	if (!ValidIngame()) return false;
	PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
	float fHeadDiff = fabs(pChSpawn->Heading - fHead);

	if (fHeadDiff > SET->AllowMove)
	{
		// if we are more than an 1/8th turn
		return false;
	}

	if ((fHeadDiff / 2.0f) > fabs(GetDistance(pChSpawn->Y, pChSpawn->X, fY, fX)))
	{
		// if half our needed adjustment is > distance between us and destination
		return false;
	}
	// else safe to move
	return true;
}

void CMUMovement::SetWalk(bool bWalkOn)
{
	// turns walking on or off when desired (if appropriate)
	if (!ValidIngame()) return; // ExecuteCmd in any other state = CTD
	PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;

	bool bWalking = (*EQADDR_RUNWALKSTATE) ? false : true;
	if (pChSpawn->SpeedMultiplier < 0.0f || pChSpawn->RunSpeed < 0.0f)
	{
		//if negative speed, we are snared, and do not want walk on
		bWalkOn = false;
	}
	if (bWalkOn != bWalking)
	{
		MQ2Globals::ExecuteCmd(iRunWalk, 1, 0);
		MQ2Globals::ExecuteCmd(iRunWalk, 0, 0);
	}
}

void CMUMovement::DoStand()
{
	// stand up when desired (if appropriate)
	// feignsupport is handled *here only*
	if (!ValidIngame()) return;
	PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;

	switch (pChSpawn->StandState)
	{
	case STANDSTATE_SIT:
		EzCommand("/stand"); // fix for preventing sit/stand bug
		break;
	case STANDSTATE_FEIGN:
		if (SET->Feign)
		{
			sprintf(szMsg, "\ay%s\aw:: Not standing as you are currently Feign Death", MODULE_NAME);
			WriteLine(szMsg, V_FEIGN);
			break;
		}
		EzCommand("/stand");
		break;
	case STANDSTATE_DUCK:
		/*MQ2Globals::ExecuteCmd(iDuckKey, 1, 0);
		MQ2Globals::ExecuteCmd(iDuckKey, 0, 0);*/ // rare server desync can happen from doing it this way
		EzCommand("/stand");
		break;
	case STANDSTATE_STAND:
	case STANDSTATE_DEAD:
	case STANDSTATE_BIND:
	case STANDSTATE_CASTING:
		break;
	default:
		SpewDebug(DBG_MAIN, "StandIfNeeded():: no StandState matches for %d", pChSpawn->StandState);
		break;
	}
}

void CMUMovement::Walk(unsigned char ucDirection)
{
	// wrapper for calling movement with walking on
	DoMove(ucDirection, true, MU_WALKON);
}

void CMUMovement::TryMove(unsigned char ucDirection, unsigned char ucWalk, float fHead, float fY, float fX)
{
	// wrapper for calling movement with anti-orbit formula calculated
	DoMove(ucDirection, CanMove(fHead, fY, fX), ucWalk);
}

void CMUMovement::StopMove(unsigned char ucDirection)
{
	// wrapper for stopping movement
	// old or new style support handled by the class instead of function
	if (SET->WinEQ || bOffsetOverride)
	{
		SimMoveOff(ucDirection);
		return;
	}
	TrueMoveOff(ucDirection);
}

void CMUMovement::StickStrafe(unsigned char ucDirection)
{
	if (STICK->DelayStrafe)
	{
		TimedStrafe(ucDirection);
		return;
	}
	DoMove(ucDirection, true, STICK->Walk ? MU_WALKON : MU_WALKIGNORE);
}

void CMUMovement::TimedStrafe(unsigned char ucDirection)
{
	// function to determine if we can begin strafing based on
	// called if delaystrafe is on, calculates values via
	// CMUDelay inherit functions using Min/Max
	unsigned char ucResult = STICK->TimeStatus();
	if (!pMU->CmdStrafe && ucResult == T_INACTIVE)
	{
		StopMove(KILL_STRAFE);
		STICK->TimeStart();
		return; // return if we are start moving, not continue moving
	}
	if (ucResult == T_READY)
	{
		DoMove(ucDirection, true, STICK->Walk ? MU_WALKON : MU_WALKIGNORE);
	}
}

void CMUMovement::TurnHead(float fHeading)
{
	// this is called by AutoHead if ChangeHead has a value
	// do a loose or true heading adjustment determined here
	if (!ValidIngame()) return;
	switch(pMU->Head)
	{
	case H_LOOSE:
		LooseTurn(fHeading);
		break;
	case H_TRUE:
		if (bOffsetOverride || SET->WinEQ)
		{
			LooseTurn(fHeading);
			break;
		}
		TrueTurn(fHeading);
		break;
	default:
		break;
	}
}

void CMUMovement::FastTurn(float fNewHead)
{
	if (!ValidIngame()) return;
	gFaceAngle = H_INACTIVE;
	((PSPAWNINFO)pCharSpawn)->Heading = fNewHead;
}

void CMUMovement::LooseTurn(float fNewHead)
{
	if (!ValidIngame()) return;
	gFaceAngle = H_INACTIVE;
	PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
	if (fabs(pChSpawn->Heading - fNewHead) < SET->TurnRate)
	{
		// if we are within one turn away, set heading to desired heading
		pChSpawn->Heading      = fNewHead;
		pChSpawn->SpeedHeading = 0.0f;
		ChangeHead             = H_INACTIVE;
	}
	else
	{
		float fCompHead = pChSpawn->Heading + HEADING_HALF;

		if (fNewHead < pChSpawn->Heading) fNewHead += HEADING_MAX;
		if (fNewHead < fCompHead)
		{
			pChSpawn->Heading      = SaneHead(pChSpawn->Heading + SET->TurnRate);
			pChSpawn->SpeedHeading = 12.0f;
		}
		else
		{
			pChSpawn->Heading      = SaneHead(pChSpawn->Heading - SET->TurnRate);
			pChSpawn->SpeedHeading = -12.0f;
		}
	}
}

void CMUMovement::TrueTurn(float fNewHead)
{
	if (!ValidIngame()) return;
	gFaceAngle = H_INACTIVE;
	PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
	if (fabs(pChSpawn->Heading - fNewHead) < 14.0f)
	{
		pKeypressHandler->CommandState[iTurnLeft]  = 0;
		*pulTurnLeft                               = 0;
		pKeypressHandler->CommandState[iTurnRight] = 0;
		*pulTurnRight                              = 0;
		pChSpawn->Heading                          = fNewHead;
		pChSpawn->SpeedHeading                     = 0.0f;
		ChangeHead                                 = H_INACTIVE;
	}
	else
	{
		float fCompHead = pChSpawn->Heading + HEADING_HALF;

		if (fNewHead < pChSpawn->Heading) fNewHead += HEADING_MAX;
		if (fNewHead < fCompHead)
		{
			pKeypressHandler->CommandState[iTurnRight] = 0;
			*pulTurnRight                              = 0;
			pKeypressHandler->CommandState[iTurnLeft]  = 1;
			*pulTurnLeft                               = 1;
		}
		else
		{
			pKeypressHandler->CommandState[iTurnLeft]  = 0;
			*pulTurnLeft                               = 0;
			pKeypressHandler->CommandState[iTurnRight] = 1;
			*pulTurnRight                              = 1;
		}
	}
}

void CMUMovement::TrueMoveOn(unsigned char ucDirection)
{
	switch(ucDirection)
	{
	case GO_FORWARD:
		pMU->CmdFwd                               = true;
		pKeypressHandler->CommandState[iAutoRun]  = 0;
		*pulAutoRun                               = 0;
		pKeypressHandler->CommandState[iBackward] = 0;
		*pulBackward                              = 0;
		pKeypressHandler->CommandState[iForward]  = 1;
		*pulForward                               = 1;
		break;
	case GO_BACKWARD:
		pMU->CmdFwd                               = false;
		pKeypressHandler->CommandState[iAutoRun]  = 0;
		*pulAutoRun                               = 0;
		pKeypressHandler->CommandState[iForward]  = 0;
		*pulForward                               = 0;
		pKeypressHandler->CommandState[iBackward] = 1;
		*pulBackward                              = 1;
		break;
	case GO_LEFT:
		pMU->CmdStrafe                               = true;
		pKeypressHandler->CommandState[iAutoRun]     = 0;
		*pulAutoRun                                  = 0;
		pKeypressHandler->CommandState[iStrafeRight] = 0;
		*pulStrafeRight                              = 0;
		pKeypressHandler->CommandState[iStrafeLeft]  = 1;
		*pulStrafeLeft                               = 1;
		break;
	case GO_RIGHT:
		pMU->CmdStrafe                               = true;
		pKeypressHandler->CommandState[iAutoRun]     = 0;
		*pulAutoRun                                  = 0;
		pKeypressHandler->CommandState[iStrafeLeft]  = 0;
		*pulStrafeLeft                               = 0;
		pKeypressHandler->CommandState[iStrafeRight] = 1;
		*pulStrafeRight                              = 1;
		break;
	}
}

void CMUMovement::TrueMoveOff(unsigned char ucDirection)
{
	switch(ucDirection)
	{
	case APPLY_TO_ALL:
		pKeypressHandler->CommandState[iAutoRun]     = 0;
		*pulAutoRun                                  = 0;
		pKeypressHandler->CommandState[iStrafeLeft]  = 0;
		*pulStrafeLeft                               = 0;
		pKeypressHandler->CommandState[iStrafeRight] = 0;
		*pulStrafeRight                              = 0;
		pKeypressHandler->CommandState[iForward]     = 0;
		*pulForward                                  = 0;
		pKeypressHandler->CommandState[iBackward]    = 0;
		*pulBackward                                 = 0;
		pMU->CmdFwd = pMU->CmdStrafe                 = false;
		STICK->TimeStop();
		break;
	case GO_FORWARD:
		pKeypressHandler->CommandState[iAutoRun] = 0;
		*pulAutoRun                              = 0;
		pKeypressHandler->CommandState[iForward] = 0;
		*pulForward                              = 0;
		pMU->CmdFwd                              = false;
		break;
	case GO_BACKWARD:
		pKeypressHandler->CommandState[iAutoRun]  = 0;
		*pulAutoRun                               = 0;
		pKeypressHandler->CommandState[iBackward] = 0;
		*pulBackward                              = 0;
		pMU->CmdFwd                               = false;
		break;
	case GO_LEFT:
		pKeypressHandler->CommandState[iAutoRun]    = 0;
		*pulAutoRun                                 = 0;
		pKeypressHandler->CommandState[iStrafeLeft] = 0;
		*pulStrafeLeft                              = 0;
		pMU->CmdStrafe                              = false;
		STICK->TimeStop();
		break;
	case GO_RIGHT:
		pKeypressHandler->CommandState[iAutoRun]     = 0;
		*pulAutoRun                                  = 0;
		pKeypressHandler->CommandState[iStrafeRight] = 0;
		*pulStrafeRight                              = 0;
		pMU->CmdStrafe                               = false;
		STICK->TimeStop();
		break;
	case KILL_STRAFE:
		pKeypressHandler->CommandState[iStrafeLeft]  = 0;
		*pulStrafeLeft                               = 0;
		pKeypressHandler->CommandState[iStrafeRight] = 0;
		*pulStrafeRight                              = 0;
		pMU->CmdStrafe                               = false;
		STICK->TimeStop();
		break;
	case KILL_FB:
		pKeypressHandler->CommandState[iAutoRun]  = 0;
		*pulAutoRun                               = 0;
		pKeypressHandler->CommandState[iForward]  = 0;
		*pulForward                               = 0;
		pKeypressHandler->CommandState[iBackward] = 0;
		*pulBackward                              = 0;
		pMU->CmdFwd                               = false;
		break;
	}
}

void CMUMovement::SimMoveOn(unsigned char ucDirection)
{
	switch (ucDirection)
	{
	case GO_FORWARD:
		pMU->CmdFwd = true;
		MQ2Globals::ExecuteCmd(iBackward, 0, 0);
		MQ2Globals::ExecuteCmd(iForward,  1, 0);
		break;
	case GO_BACKWARD:
		pMU->CmdFwd = false;
		MQ2Globals::ExecuteCmd(iForward,  0, 0);
		MQ2Globals::ExecuteCmd(iBackward, 1, 0);
		break;
	case GO_LEFT:
		pMU->CmdStrafe = true;
		MQ2Globals::ExecuteCmd(iStrafeRight, 0, 0);
		MQ2Globals::ExecuteCmd(iStrafeLeft,  1, 0);
		break;
	case GO_RIGHT:
		pMU->CmdStrafe = true;
		MQ2Globals::ExecuteCmd(iStrafeLeft,  0, 0);
		MQ2Globals::ExecuteCmd(iStrafeRight, 1, 0);
		break;
	}
}

void CMUMovement::SimMoveOff(unsigned char ucDirection)
{
	switch (ucDirection)
	{
	case APPLY_TO_ALL:
		MQ2Globals::ExecuteCmd(iForward,     0, 0);
		MQ2Globals::ExecuteCmd(iBackward,    1, 0);
		MQ2Globals::ExecuteCmd(iBackward,    0, 0);
		MQ2Globals::ExecuteCmd(iStrafeLeft,  0, 0);
		MQ2Globals::ExecuteCmd(iStrafeRight, 1, 0);
		MQ2Globals::ExecuteCmd(iStrafeRight, 0, 0);
		pMU->CmdFwd = pMU->CmdStrafe = false;
		break;
	case KILL_FB:
	case GO_FORWARD:
	case GO_BACKWARD:
		MQ2Globals::ExecuteCmd(iForward,     1, 0);
		MQ2Globals::ExecuteCmd(iForward,     0, 0);
		MQ2Globals::ExecuteCmd(iBackward,    1, 0);
		MQ2Globals::ExecuteCmd(iBackward,    0, 0);
		pMU->CmdFwd = false;
		break;
	case KILL_STRAFE:
	case GO_LEFT:
	case GO_RIGHT:
		MQ2Globals::ExecuteCmd(iStrafeRight, 1, 0);
		MQ2Globals::ExecuteCmd(iStrafeRight, 0, 0);
		MQ2Globals::ExecuteCmd(iStrafeLeft,  1, 0);
		MQ2Globals::ExecuteCmd(iStrafeLeft,  0, 0);
		pMU->CmdStrafe = false;
		break;
	}
}

void CMUMovement::DoMove(unsigned char ucDirection, bool bTurnOn, unsigned char ucWalk)
{
	// this one due to prototype
	switch(ucWalk)
	{
	case MU_WALKON:
		SetWalk(true);
		break;
	case MU_WALKOFF:
		SetWalk(false);
		break;
	case MU_WALKIGNORE:
		break;
	}

	if (SET->WinEQ || bOffsetOverride)
	{
		if (bTurnOn)
		{
			SimMoveOn(ucDirection);
			return;
		}
		SimMoveOff(ucDirection);
		return;
	}
	if (bTurnOn)
	{
		TrueMoveOn(ucDirection);
		return;
	}
	TrueMoveOff(ucDirection);
};

bool CMoveToCmd::DidAggro()
{
	if (!CURCAMP->On && On && ME->InCombat())
	{
		pMU->MovetoBroke = true;
		EndPreviousCmd(true);
		sprintf(szMsg, "\ay%s\aw:: Aggro gained during /moveto, Halting command...", MODULE_NAME);
		WriteLine(szMsg, V_BREAKONAGGRO);
		return true;
	}
	return false;
};

bool CStickCmd::AlwaysStatus()
{
	if (!pTarget || ((PSPAWNINFO)pTarget)->Type != SPAWN_NPC)
	{
		if (AlwaysReady)
		{
			sprintf(szMsg, "\ay%s\aw:: Stick awaiting next valid NPC target...", MODULE_NAME);
			WriteLine(szMsg, V_STICKALWAYS);
			MOVE->StopMove(APPLY_TO_ALL);
			DoRandomize();
			AlwaysReady = false;
		}
		HaveTarget = false;
		return false;
	}

	if (!AlwaysReady)
	{
		EndPreviousCmd(true, CMD_STICK, true);
		PAUSE->Reset();
		MOVE->DoStand();
		MOVE->StopHeading();
		AlwaysReady = true;
	}
	HaveTarget = true;
	return true;
};

bool CPauseHandler::MouseCheck()
{
	if (*pMouseLook)
	{
		UserMouse = true;
		if ((SET->PauseMouse || SET->BreakMouse) && PauseNeeded())
		{
			CAMP->TimeStop();
			TimeStop();
			MOVE->StopHeading();
			if (SET->BreakMouse)
			{
				EndPreviousCmd(true);
				if (!CAMP->Auto)
				{
					sprintf(szMsg, "\ay%s\aw:: Current command ended from mouse movement.", MODULE_NAME);
					WriteLine(szMsg, V_MOUSEPAUSE);
				}
				return false;
			}
			PausedMU = true;
			MOVE->DoMove(APPLY_TO_ALL, false, MU_WALKOFF);
			HandleMouse = true;
			return true;
		}
		return false;
	}
	UserMouse = false;
	return false;
};

void CPauseHandler::Reset()
{
	CAMP->TimeStop();
	STUCK->Reset();
	pMU->CmdFwd    = false;
	pMU->CmdStrafe = false;
	pMU->NewSummon();
	STICK->NewSnaproll();
	STICK->TimeStop();
	STICK->ResetLoc();
};

void CMUActive::AggroTLO()
{
	if (pTarget)
	{
		if (fabs(MOVE->AngDist(((PSPAWNINFO)pTarget)->Heading, ((PSPAWNINFO)pCharSpawn)->Heading)) > 190.0f)
		{
			Aggro = true;
			return;
		}
	}
	Aggro = false;
};

// InnerSpace data types for Top Level Objects

bool IS_MakeCampType::GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest)
{
	PLSTYPEMEMBER pMember = IS_MakeCampType::FindMember(Member);
	if (!pMember || !ValidIngame(false)) return false;
	switch((MakeCampMembers)pMember->ID)
	{
	case Status:
		Dest.CharPtr = "OFF";
		if (PAUSE->PausedMU || PAUSE->PausedCmd)
		{
			Dest.CharPtr = "PAUSED";
		}
		else if (CURCAMP->On)
		{
			Dest.CharPtr = "ON";
		}
		Dest.Type = pStringType;
		return true;
	case Leash:
		Dest.DWord = CURCAMP->Leash;
		Dest.Type  = pBoolType;
		return true;
	case AnchorY:
		Dest.Float = CURCAMP->Y;
		Dest.Type  = pFloatType;
		return true;
	case AnchorX:
		Dest.Float = CURCAMP->X;
		Dest.Type  = pFloatType;
		return true;
	case LeashLength:
		Dest.Float = CURCAMP->Length;
		Dest.Type  = pFloatType;
		return true;
	case CampRadius:
		Dest.Float = CURCAMP->Radius;
		Dest.Type  = pFloatType;
		return true;
	case MinDelay:
		Dest.DWord = CAMP->Min;
		Dest.Type  = pIntType;
		return true;
	case MaxDelay:
		Dest.DWord = CAMP->Max;
		Dest.Type  = pIntType;
		return true;
	case Returning:
		Dest.DWord = CAMP->DoReturn;
		Dest.Type  = pBoolType;
		return true;
	case AltAnchorY:
		Dest.Float = ALTCAMP->Y;
		Dest.Type  = pFloatType;
		return true;
	case AltAnchorX:
		Dest.Float = ALTCAMP->X;
		Dest.Type  = pFloatType;
		return true;
	case CampDist:
		Dest.Float = 0.0f;
		if (CURCAMP->On)
		{
			PSPAWNINFO pLPlayer = (PSPAWNINFO)pLocalPlayer;
			Dest.Float = GetDistance(pLPlayer->Y, pLPlayer->X, CURCAMP->Y, CURCAMP->X);
		}
		Dest.Type = pFloatType;
		return true;
	case AltCampDist:
		Dest.Float = 0.0f;
		if (ALTCAMP->On)
		{
			PSPAWNINFO pLPlayer = (PSPAWNINFO)pLocalPlayer;
			Dest.Float = GetDistance(pLPlayer->Y, pLPlayer->X, ALTCAMP->Y, ALTCAMP->X);
		}
		Dest.Type = pFloatType;
		return true;
	case AltRadius:
		Dest.Float = ALTCAMP->Radius;
		Dest.Type  = pFloatType;
		return true;
	case Scatter:
		Dest.DWord = CURCAMP->Scatter;
		Dest.Type  = pBoolType;
		return true;
	case ReturnNoAggro:
		Dest.DWord = CURCAMP->NoAggro;
		Dest.Type  = pBoolType;
		return true;
	case ReturnNotLooting:
		Dest.DWord = CURCAMP->NotLoot;
		Dest.Type  = pBoolType;
		return true;
	case ReturnHaveTarget:
		Dest.DWord = CURCAMP->HaveTarget;
		Dest.Type  = pBoolType;
		return true;
	case Bearing:
		Dest.Float = CURCAMP->Bearing;
		Dest.Type  = pFloatType;
		return true;
	case ScatDist:
		Dest.Float = CURCAMP->ScatDist;
		Dest.Type  = pFloatType;
		return true;
	case ScatSize:
		Dest.Float = CURCAMP->ScatSize;
		Dest.Type  = pFloatType;
		return true;
	}
	return false;
}

bool IS_StickType::GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest)
{
	PLSTYPEMEMBER pMember = IS_StickType::FindMember(Member);
	if (!pMember || !ValidIngame(false)) return false;
	switch((StickMembers)pMember->ID)
	{
	case Status:
		Dest.CharPtr = "OFF";
		if (PAUSE->PausedMU || PAUSE->PausedCmd) {
			Dest.CharPtr = "PAUSED";
		} else if (STICK->On) {
			Dest.CharPtr = "ON";
		}
		Dest.Type = pStringType;
		return true;
	case Active:
		Dest.DWord = STICK->On;
		Dest.Type  = pBoolType;
		return true;
	case Distance:
		Dest.Float = STICK->Dist;
		Dest.Type  = pFloatType;
		return true;
	case MoveBehind:
		Dest.DWord = STICK->Behind;
		Dest.Type  = pBoolType;
		return true;
	case MoveBack:
		Dest.DWord = STICK->MoveBack;
		Dest.Type  = pBoolType;
		return true;
	case Loose:
		Dest.DWord = (pMU->Head == H_LOOSE) ? true : false;
		Dest.Type  = pBoolType;
		return true;
	case Paused:
		Dest.DWord = PAUSE->PausedMU;
		Dest.Type  = pBoolType;
		return true;
	case Behind:
		Dest.DWord = false;
		if (PSPAWNINFO psTarget = (PSPAWNINFO)(STICK->Hold ? GetSpawnByID(STICK->HoldID) : pTarget))
		{
			PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
			Dest.DWord = (fabs(GetDistance(pChSpawn, psTarget)) <= ((STICK->Dist > 0.0f ? STICK->Dist : (psTarget->StandState ? get_melee_range(pLocalPlayer, (EQPlayer *)psTarget) : 15.0f)) * STICK->DistModP + STICK->DistMod) && fabs(MOVE->AngDist(psTarget->Heading, pChSpawn->Heading)) <= STICK->ArcBehind) ? true : false;
		}
		Dest.Type = pBoolType;
		return true;
	case Stopped:
		Dest.DWord = false;
		if (PSPAWNINFO psTarget = (PSPAWNINFO)(STICK->Hold ? GetSpawnByID(STICK->HoldID) : pTarget))
		{
			Dest.DWord = (fabs(GetDistance((PSPAWNINFO)pCharSpawn, psTarget)) <= STICK->Dist) ? true : false;
		}
		Dest.Type = pBoolType;
		return true;
	case Pin:
		Dest.DWord = STICK->Pin;
		Dest.Type  = pBoolType;
		return true;
	case StickTarget:
		Dest.Int = 0;
		if (PSPAWNINFO psTarget = (PSPAWNINFO)(STICK->Hold ? GetSpawnByID(STICK->HoldID) : pTarget))
		{
			Dest.Int = psTarget->SpawnID;
		}
		Dest.Type = pIntType;
		return true;
	case StickTargetName:
		Dest.CharPtr = "NONE";
		if (PSPAWNINFO psTarget = (PSPAWNINFO)(STICK->Hold ? GetSpawnByID(STICK->HoldID) : pTarget))
		{
			Dest.Ptr = pISInterface->GetTempBuffer(strlen(psTarget->DisplayedName)+1, psTarget->DisplayedName);
		}
		Dest.Type = pStringType;
		return true;
	case DistMod:
		Dest.Float = STICK->DistMod;
		Dest.Type  = pFloatType;
		return true;
	case DistModPercent:
		Dest.Float = STICK->DistModP;
		Dest.Type  = pFloatType;
		return true;
	case Always:
		Dest.DWord = STICK->Always;
		Dest.Type  = pBoolType;
		return true;
	}
	return false;
}

bool IS_MoveToType::GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest)
{
	PLSTYPEMEMBER pMember = IS_MoveToType::FindMember(Member);
	if (!pMember || !ValidIngame(false)) return false;
	switch((MoveToMembers)pMember->ID)
	{
	case Moving:
		Dest.DWord = MOVETO->On;
		Dest.Type  = pBoolType;
		return true;
	case Stopped:
		Dest.DWord = pMU->StoppedMoveto;
		Dest.Type  = pBoolType;
		return true;
	case CampStopped:
		Dest.DWord = false;
		if (pLocalPlayer)
		{
			Dest.DWord = (fabs(GetDistance(((PSPAWNINFO)pCharSpawn)->Y, ((PSPAWNINFO)pCharSpawn)->X, CAMP->Y, CAMP->X)) <= MOVETO->Dist) ? true : false;
		}
		Dest.Type  = pBoolType;
		return true;
	case UseWalk:
		Dest.DWord = MOVETO->Walk;
		Dest.Type  = pBoolType;
		return true;
	case ArrivalDist:
		Dest.Float = MOVETO->Dist;
		Dest.Type  = pFloatType;
		return true;
	case ArrivalDistY:
		Dest.Float = MOVETO->DistY;
		Dest.Type  = pFloatType;
		return true;
	case ArrivalDistX:
		Dest.Float = MOVETO->DistX;
		Dest.Type  = pFloatType;
		return true;
	case Broken:
		Dest.DWord = pMU->MovetoBroke;
		Dest.Type  = pBoolType;
		return true;
	}
	return false;
}

bool IS_CircleType::GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest)
{
	PLSTYPEMEMBER pMember = IS_CircleType::FindMember(Member);
	if (!pMember || !ValidIngame(false)) return false;
	switch((CircleMembers)pMember->ID)
	{
	case Status:
		Dest.CharPtr = "OFF";
		if (PAUSE->PausedMU || PAUSE->PausedCmd)
		{
			Dest.CharPtr = "PAUSED";
		}
		else if (CIRCLE->On)
		{
			Dest.CharPtr = "ON";
		}
		Dest.Type = pStringType;
		return true;
	case CircleY:
		Dest.Float = CIRCLE->Y;
		Dest.Type  = pFloatType;
		return true;
	case CircleX:
		Dest.Float = CIRCLE->X;
		Dest.Type = pFloatType;
		return true;
	case Drunken:
		Dest.DWord = CIRCLE->Drunk;
		Dest.Type  = pBoolType;
		return true;
	case Rotation:
		Dest.CharPtr = "CW";
		if (CIRCLE->CCW)
		{
			Dest.CharPtr = "CCW";
		}
		Dest.Type = pStringType;
		return true;
	case Direction:
		Dest.CharPtr = "FORWARDS";
		if (CIRCLE->Backward)
		{
			Dest.CharPtr = "BACKWARDS";
		}
		Dest.Type = pStringType;
		return true;
	case Clockwise:
		Dest.DWord = !CIRCLE->CCW;
		Dest.Type  = pBoolType;
		return true;
	case Backwards:
		Dest.DWord = CIRCLE->Backward;
		Dest.Type  = pBoolType;
		return true;
	case Radius:
		Dest.Float = CIRCLE->Radius;
		Dest.Type  = pFloatType;
		return true;
	}
	return false;
}

bool IS_MovementType::GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest)
{
	char szTemp[10] = {0};
	PLSTYPEMEMBER pMember = IS_MovementType::FindMember(Member);
	if (!pMember || !ValidIngame(false)) return false;
	switch((MovementMembers)pMember->ID)
	{
	case Command:
		Dest.CharPtr = "NONE";
		if (STICK->On) {
			Dest.CharPtr = "STICK";
		} else if (CIRCLE->On) {
			Dest.CharPtr = "CIRCLE";
		} else if (MOVETO->On) {
			Dest.CharPtr = "MOVETO";
		} else if (CURCAMP->On) {
			Dest.CharPtr = "MAKECAMP";
		}
		Dest.Type = pStringType;
		return true;
	case Stuck:
		Dest.DWord = false;
		if (STUCK->StuckInc)
		{
			Dest.DWord = true;
		}
		Dest.Type = pBoolType;
		return true;
	case Summoned:
		Dest.DWord = pMU->BrokeSummon;
		Dest.Type  = pBoolType;
		return true;
	case StuckLogic:
		Dest.DWord = STUCK->On;
		Dest.Type  = pBoolType;
		return true;
	case Verbosity:
		Dest.DWord = (uiVerbLevel & V_VERBOSITY) == V_VERBOSITY ? true : false;
		Dest.Type  = pBoolType;
		return true;
	case FullVerbosity:
		Dest.DWord = (uiVerbLevel & V_FULLVERBOSITY) == V_FULLVERBOSITY ? true : false;
		Dest.Type  = pBoolType;
		return true;
	case TotalSilence:
		Dest.DWord = (uiVerbLevel == V_SILENCE) ? true : false;
		Dest.Type  = pBoolType;
		return true;
	case Aggro:
		Dest.DWord = pMU->Aggro;
		Dest.Type  = pBoolType;
		return true;
	case PauseMinDelay:
		Dest.Int  = PAUSE->Min;
		Dest.Type = pIntType;
		return true;
	case PauseMaxDelay:
		Dest.Int  = PAUSE->Max;
		Dest.Type = pIntType;
		return true;
	case PulseCheck:
		Dest.Int  = STUCK->Check;
		Dest.Type = pIntType;
		return true;
	case PulseUnstuck:
		Dest.Int  = STUCK->Unstuck;
		Dest.Type = pIntType;
		return true;
	case TryToJump:
		Dest.DWord = STUCK->Jump;
		Dest.Type  = pBoolType;
		return true;
	case DistStuck:
		Dest.Float = STUCK->Dist;
		Dest.Type  = pFloatType;
		return true;
	case Version:
		sprintf(szTemp, "%1.2f", MODULE_VERSION);
		Dest.Ptr  = pISInterface->GetTempBuffer(strlen(szTemp)+1,szTemp);
		Dest.Type = pStringType;
		return true;
	case MovePause:
		Dest.DWord = SET->PauseKB;
		Dest.Type  = pBoolType;
		return true;
	case GM:
		Dest.DWord = pMU->BrokeGM;
		Dest.Type  = pBoolType;
		return true;
	case MeshLoaded:
		Dest.DWord = pMU->navMesh != NULL;
		Dest.Type  = pBoolType;
		return true;
	}
	return false;
}

bool __cdecl TLO_MakeCamp(int argc, char *argv[], LSTYPEVAR &Dest)
{
	Dest.DWord = 1;
	Dest.Type  = pMakeCampType;
	return true;
}

bool __cdecl TLO_Stick(int argc, char *argv[], LSTYPEVAR &Dest)
{
	Dest.DWord = 1;
	Dest.Type  = pStickType;
	return true;
}

bool __cdecl TLO_MoveTo(int argc, char *argv[], LSTYPEVAR &Dest)
{
	Dest.DWord = 1;
	Dest.Type  = pMoveToType;
	return true;
}

bool __cdecl TLO_Circling(int argc, char *argv[], LSTYPEVAR &Dest)
{
	Dest.DWord = 1;
	Dest.Type  = pCircleType;
	return true;
}

bool __cdecl TLO_Movement(int argc, char *argv[], LSTYPEVAR &Dest)
{
	Dest.DWord = 1;
	Dest.Type  = pMovementType;
	return true;
}

// End Custom Top-Level Objects
// -----------------------------
// Blech Events

void __stdcall CheckAggro_Event(unsigned int ID, void *pData, PBLECHVALUE pValues)
{
	// do not process if makecamp is on, as that would interfere with camp returns
	if (!ValidIngame() || !MOVETO->On || !MOVETO->BreakHit || CURCAMP->On) return;
	pMU->MovetoBroke = true;
	EndPreviousCmd(true);
	sprintf(szMsg, "\ay%s\aw:: Aggro gained during /moveto, Halting command...", MODULE_NAME);
	WriteLine(szMsg, V_BREAKONHIT);
}

void __stdcall CheckGates_Event(unsigned int ID, void *pData, PBLECHVALUE pValues)
{
	if (!ValidIngame() || !STICK->On || !STICK->BreakGate) return;
	PSPAWNINFO psTarget = (PSPAWNINFO)((STICK->Hold && STICK->HoldID) ? GetSpawnByID(STICK->HoldID) : pTarget);
	if (psTarget && pValues)
	{
		if (!strcmp(pValues->Value, psTarget->DisplayedName))
		{
			EndPreviousCmd(true);
			sprintf(szMsg, "\ay%s\aw:: Mob gating ended previous command.", MODULE_NAME);
			WriteLine(szMsg, V_BREAKONGATE);
		}
	}
}

void SetupEvents(bool bAddEvent, bool bForceRemove)
{
	if (bAddEvent)
	{
		if (SET_M->BreakHit || MOVETO->BreakHit)
		{
			if (!Event_AggroNorm)
				Event_AggroNorm = EzAddTrigger("#Name# #Hurts# YOU for #Damage#", CheckAggro_Event, 0);
			if (!Event_MissNorm)
				Event_MissNorm = EzAddTrigger("#Name# tries to #Hit# YOU, but #YouRock#", CheckAggro_Event, 0);
			if (!Event_AggroAbbrev)
				Event_AggroAbbrev = EzAddTrigger("#Name# #Hurt#s for #Damage#", CheckAggro_Event, 0);
			if (!Event_MissAbbrev)
				Event_MissAbbrev = EzAddTrigger("#Name# missed", CheckAggro_Event, 0);
			if (!Event_MissNumOnly)
				Event_MissNumOnly = EzAddTrigger("miss", CheckAggro_Event, 0);
			// not going to parse for just numbers as that would require parsing every line
			// and evaluating if its only numerical - this would be a bit over the top
		}
		if (SET_S->BreakGate || STICK->BreakGate)
		{
			if (!Event_Gates)
				Event_Gates = EzAddTrigger("#Name# Gates.", CheckGates_Event, 0);
		}
	}
	else
	{
		if (bForceRemove || !SET_M->BreakHit)
		{
			if (Event_AggroNorm)    EzRemoveTrigger(Event_AggroNorm);
			Event_AggroNorm = NULL;

			if (Event_MissNorm)     EzRemoveTrigger(Event_MissNorm);
			Event_MissNorm = NULL;

			if (Event_AggroAbbrev)  EzRemoveTrigger(Event_AggroAbbrev);
			Event_AggroAbbrev = NULL;

			if (Event_MissAbbrev)   EzRemoveTrigger(Event_MissAbbrev);
			Event_MissAbbrev = NULL;

			if (Event_MissNumOnly)  EzRemoveTrigger(Event_MissNumOnly);
			Event_MissNumOnly = NULL;
		}
		if (bForceRemove || !SET_S->BreakGate)
		{
			if (Event_Gates) EzRemoveTrigger(Event_Gates);
			Event_Gates = NULL;
		}
	}
}

// ----------------------------------------
// deadchicken formula functions


// CampReturn copyright: deadchicken
// not to be released outside of VIP without permission
inline void CampReturn(float fY, float fX, float fUseRadius, float* fUseY, float* fUseX)
{
	float fRandHead = rand() / RAND_MAX * CIRCLE_MAX;
	float fRandDist = rand() / RAND_MAX * fUseRadius;

	SpewDebug(DBG_MISC, "MoveUtils::CampReturn() fRandHead = %.2f, fRandDist = %.2f", fRandHead, fRandDist);

	*fUseY = fY + (fRandDist * cos(fRandHead));
	*fUseX = fX + (fRandDist * sin(fRandHead));
}

// PolarSpot copyright: deadchicken
// not to be released outside of VIP without permission
// MOB:  PolarSpot(targetX, targetY, targetHeading, desiredHeading, distanceAway, scatter);
// CAMP: PolarSpot(anchorX, anchorY, heading doesnt matter, bearing = which dir from center, dist = how far from center, scatter=size of scatter);
inline void PolarSpot(float fY, float fX, float fPHeading, float fPBearing, float fPDist, float fPScatter, float* fUseY, float* fUseX)
{
	// if camp returning
	if (fPScatter != 0.0f)
	{
		float fRandHead = MOVE->SaneHead(rand() / RAND_MAX * HEADING_MAX);
		float fRandDist = rand() / RAND_MAX * fPScatter;

		*fUseY = fY + (fRandDist * cos(fRandHead));
		*fUseX = fX + (fRandDist * sin(fRandHead));

		// 0.0175f converts degrees to radians which sinf/cosf expect
		/*fUseX = fOurGotoX + fRandDist * sinf(fRandHead*0.0175f));
		fUseY = fOurGotoY + fRandDist * cosf(fRandHead*0.0175f));*/

		return;
	}
	// else snaproll

	//float fRelHead = (fPHeading / fPBearing) * -(float)PI;
	//STICK->Snap->Y = fY  - (float)cos(fRelHead)* fPDist;
	//STICK->Snap->X = fX  + (float)sin(fRelHead)* fPDist;
	float fRelHead = MOVE->SaneHead(fPHeading - fPBearing);
	fRelHead       = ((fRelHead / HEADING_MAX) * CIRCLE_MAX) * 0.0175f;
	*fUseY         = fY + (fPDist * cos(fRelHead));
	*fUseX         = fX + (fPDist * sin(fRelHead));
}

// MovingAvg copyright: deadchicken
// not to be released outside of VIP without permission
// returns a moving average of size iEntries by adding fNew to the ring and computing
// fNew = New value to add to the ring
// iEntries = number of entries in ring, used for divisior and to re-init ring on change/init.
// Returns the moving average based on said values
// Notes: MaxRing size is 32 by #define up top and not checked, should be
float MovingAvg(float fNew, int iEntries)
{
	static float fRing[MAXRINGSIZE] = {0.0f};
	static int            iOldStuck = 0;
	static int              iRinger = 0;
	static int            iRingFull = 0;
	int                    i = 0;
	float                 fAvg = 0.0f;

	// Bounds checking
	if (iEntries > MAXRINGSIZE || iEntries < 2 ) return fNew;
	// Do we have a new ring size?
	if (iOldStuck != iEntries)
	{
		// Do some shit to make us right
		SpewDebug(DBG_MISC, "MoveUtils::MovingAvg() Entry # changed, filling ring with %3.2f!  %d != %d", fNew, iOldStuck, iEntries);
		// Fill the array with this latest value
		// maybe this should be our default preload value of 2.0f?
		for(i = 0; i < iEntries; i++) fRing[i] = fNew;
		// update iOldStuck and reset counter to 0
		iRinger   = 0;
		iOldStuck = iEntries;
		return fNew;
	}
	else
	{
		// Plain old ring buffer
		fRing[iRinger] = fNew;
		SpewDebug(DBG_MISC, "MoveUtils::MovingAvg() Added %3.2f to fRing[%d]", fNew, iRinger);
		// Increment iRinger and wrap if needed, if we wrap then it's full
		iRinger++;
		if (iRinger >= iEntries)
		{
			iRinger   = 0;
			iRingFull = 1;
		}
		// Get the sum of the ring
		//for( i=0; i<(iRingFull?iEntries:iRinger); i++) {  <-- this was a bad idea
		for(i = 0; i < iEntries; i++)
		{
			SpewDebug(DBG_MISC, "MoveUtils::MovingAvg() i=%d and fRing[%d]=%3.2f", i, i, fRing[i]);
			fAvg += fRing[i];
		}
	}
	return (fAvg / (float)iEntries);
}

// ----------------------------------------
// Utility
inline bool ValidIngame(bool bCheckDead)
{
	// CTD prevention function
	PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
	if (GetGameState() != GAMESTATE_INGAME || !pLocalPlayer || !pChSpawn->SpawnID || !pMU || (bCheckDead && pChSpawn->RespawnTimer > 0))
	{
		return false;
	}
	return true;
}

char* ftoa(float fNum, char* szText)
{
	sprintf(szText, "%.2f", fNum);
	return szText;
}

// ----------------------------------------
// Begin user command handling

// ends active commands
void EndPreviousCmd(bool bStopMove, unsigned char ucCmdUsed, bool bPreserveSelf)
{
	//reset pause
	PAUSE->PausedMU = PAUSE->PausedCmd = false;
	PAUSE->TimeStop();
	PAUSE->Reset();

	// break any active commands
	if (ucCmdUsed != CMD_CIRCLE || !bPreserveSelf)
	{
		pMU->NewCircle();
	}
	if (ucCmdUsed != CMD_MOVETO || !bPreserveSelf)
	{
		pMU->NewMoveTo();
	}
	if (ucCmdUsed != CMD_STICK || !bPreserveSelf)
	{
		pMU->NewStick();
	}

	pMU->Defaults();
	MOVE->SetWalk(false);
	SetupEvents(false);
	if (bStopMove)
	{
		MOVE->StopHeading();
		MOVE->StopMove(APPLY_TO_ALL);
	}
}

// main MU command handler, called by wrappers
int HandleCommand(int argc, char* argv[])
{
	// don't allow commands from char select or cfg files that load before entering world
	if (!ValidIngame(false)) return 1;

	unsigned char ucCmdUsed;
	if (!strnicmp(argv[0], "stick", 6)) {
		ucCmdUsed = CMD_STICK;
	} else if (!strnicmp(argv[0], "moveto", 7)) {
		ucCmdUsed = CMD_MOVETO;
	} else if (!strnicmp(argv[0], "circle", 7)) {
		ucCmdUsed = CMD_CIRCLE;
	} else if (!strnicmp(argv[0], "makecamp", 9)) {
		ucCmdUsed = CMD_MAKECAMP;
	}

	char szTempID[MAX_STRING]           = {0};  // stores output msg for stick
	signed int uiArgNum               = 1;    // argument number to evaluate
	float fTempY                        = 0.0f; // store cmd input to temp var before setting to prevent inconsistency on failed input

	PSPAWNINFO pTargetUsed = NULL; // stick id, moveto id
	PSPAWNINFO pCampPlayer = NULL; // makecamp player
	PSPAWNINFO psTarget    = (PSPAWNINFO)pTarget;
	PSPAWNINFO pLPlayer    = (PSPAWNINFO)pLocalPlayer;
	PSPAWNINFO pChSpawn    = (PSPAWNINFO)pCharSpawn;

	// switch direction of turnhalf randomly
	if (rand() % 100 > 50) STUCK->TurnSize *= -1.0f;
	// call bardcheck function upon command usage instead of onpulse
	pMU->Bard = ME->IsBard();
	// reset state of ${MoveTo.Broken} upon next '/moveto' command
	if (ucCmdUsed == CMD_MOVETO)
	{
		// added moveto.stopped here as well
		pMU->StoppedMoveto = false;
		pMU->MovetoBroke   = false;
	}

	// if no argument supplied
	if (argc == 1)
	{
		// halt all other commands due to BreakOnGM/BreakOnSummon
		if (pMU->BrokeGM || pMU->BrokeSummon)
		{
			sprintf(szMsg, "\ay%s\aw:: \arCommand failed due to \ay%s\ax triggering.", MODULE_NAME, pMU->BrokeGM ? "BreakOnGM" : "BreakOnSummon");
			WriteLine(szMsg, V_SILENCE);
			return 1;
		}
		// halt all other commands due to rootme being on
		if (pMU->Rooted)
		{
			sprintf(szMsg, "\ay%s\aw:: \arCommand failed due to \ayrootme\ax active.", MODULE_NAME);
			WriteLine(szMsg, V_SILENCE);
			return 1;
		}
		// halt new commands if plugin is paused & lockpause is on
		if (PAUSE->PausedCmd && pMU->LockPause)
		{
			sprintf(szMsg, "\ay%s\aw:: \arCommand failed due to plugin paused with lockpause.", MODULE_NAME);
			WriteLine(szMsg, V_PAUSED);
			return 1;
		}
		PAUSE->PausedMU = false;
		switch(ucCmdUsed)
		{
		case CMD_MAKECAMP:
			if (!CURCAMP->On)
			{
				PAUSE->TimeStop();
				CAMP->Activate(pChSpawn->Y, pChSpawn->X);
				sprintf(szMsg, "\ay%s\aw:: MakeCamp actived. Y(\ag%.2f\ax) X(\ag%.2f\ax) Radius(\ag%.2f\ax) Leash(%s) LeashLen(\ag%.2f\ax) Min(\ag%d\ax) Max(\ag%d\ax)", MODULE_NAME, CURCAMP->Y, CURCAMP->X, CURCAMP->Radius, CURCAMP->Leash ? "\agon\ax" : "\aroff\ax", CURCAMP->Length, CAMP->Min, CAMP->Max);
				WriteLine(szMsg, V_MAKECAMPV);
				break;
			}
			CAMP->ResetCamp(true);
			break;
		case CMD_STICK:
			EndPreviousCmd(true);
			if (psTarget)
			{
				// prevent sticking to self/mount
				if (ME->IsMe(psTarget))
				{
					SpewMUError(ERR_STICKSELF);
					break;
				}
				STICK->TurnOn();
				MOVE->DoStand();
				sprintf(szMsg, "\ay%s\aw:: You are now sticking to \ag%s\ax.", MODULE_NAME, psTarget->DisplayedName);
				WriteLine(szMsg, V_STICKV);
				break;
			}
			SpewMUError(ERR_STICKNONE);
			break;
		case CMD_MOVETO:
		case CMD_CIRCLE:
		default:
			EndPreviousCmd(true);
			// possible future use, currently '/circle' and '/moveto' designed to fail
			sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) /moveto or /circle command used with no parameter.", MODULE_NAME);
			WriteLine(szMsg, V_ERRORS);
			break;
		}
		return 1;
	}

	// get first argument
	if (argc > 1)
	{
		// generic parameters that we want to enforce first-parameter syntax
		if (!strnicmp(argv[1], "help", 5))
		{
			unsigned char ucCaller = ucCmdUsed;
			if (argc > 2 && !strnicmp(argv[2], "settings", 9))
			{
				ucCaller = HELP_SETTINGS;
			}
			OutputHelp(ucCaller);
			return 1;
		}
		else if (!strnicmp(argv[1], "debug", 6))
		{
			DebugToINI(ucCmdUsed);
			sprintf(szMsg, "\ay%s\aw:: Debug file created.", MODULE_NAME);
			WriteLine(szMsg, V_SAVED);
			return 1;
		}
		else if (!strnicmp(argv[1], "status", 7))
		{
			if (argc > 2 && !strnicmp(argv[2], "all", 4))
			{
				DebugToWnd(APPLY_TO_ALL);
				return 1;
			}
			DebugToWnd(ucCmdUsed);
			return 1;
		}
		else if (!strnicmp(argv[1], "pause", 6))
		{
			bool bDisplayLock = false;
			if (argc > 2 && !strnicmp(argv[2], "lock", 5))
			{
				pMU->LockPause = true;
				bDisplayLock = true;
			}
			if (!PAUSE->PausedCmd)
			{
				PAUSE->PausedCmd = true;
				PAUSE->TimeStop();
				MOVE->StopHeading();
				MOVE->StopMove(APPLY_TO_ALL);
				sprintf(szMsg, "\ay%s\aw:: \arPAUSED\ax%s", MODULE_NAME, pMU->LockPause ? " \ayLOCKED" : "");
				WriteLine(szMsg, V_PAUSED);
				return 1;
			}
			if (!bDisplayLock)
			{
				sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) Plugin was already paused.", MODULE_NAME);
				WriteLine(szMsg, V_ERRORS);
			}
			else
			{
				sprintf(szMsg, "\ay%s\aw:: Pause \ayLOCKED", MODULE_NAME);
				WriteLine(szMsg, V_PAUSED);
			}
			return 1;
		}
		else if (!strnicmp(argv[1], "unpause", 8))
		{
			if (PAUSE->PausedMU || PAUSE->PausedCmd)
			{
				PAUSE->PausedMU = PAUSE->PausedCmd = false;
				pMU->LockPause = SET->LockPause; // reset one-time usage if non-default
				PAUSE->TimeStop();
				PAUSE->Reset();
				sprintf(szMsg, "\ay%s\aw:: \agRESUMED", MODULE_NAME);
				WriteLine(szMsg, V_PAUSED);
				return 1;
			}
			sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) Plugin was not paused.", MODULE_NAME);
			WriteLine(szMsg, V_ERRORS);
			return 1;
		}
		else if (!strnicmp(argv[1], "save", 5))
		{
			SaveConfig();
			sprintf(szMsg, "\ay%s\aw:: Saved settings to %s", MODULE_NAME, XMLFileName);
			WriteLine(szMsg, V_SAVED);
			return 1;
		}
		else if (!strnicmp(argv[1], "load", 5))
		{
			EndPreviousCmd(true);
			LoadConfig();
			sprintf(szMsg, "\ay%s\aw:: Loaded settings from %s", MODULE_NAME, XMLFileName);
			WriteLine(szMsg, V_SAVED);
			return 1;
		}
		else if (!strnicmp(argv[1], "imsafe", 7))
		{
			pMU->BrokeSummon = pMU->BrokeGM = false;
			EndPreviousCmd(true);
			sprintf(szMsg, "\ay%s\aw:: Command usage allowed once again.", MODULE_NAME);
			WriteLine(szMsg, V_SILENCE);
			return 1;
		}
		else if (!strnicmp(argv[1], "set", 4) || !strnicmp(argv[1], "toggle", 7))
		{
			return ChangeSetting(argc, argv);
		}
		else if (!strnicmp(argv[1], "verbflags", 10))
		{
			sprintf(szMsg, "\ay%s\aw:: Current verbosity flags \ag%d", MODULE_NAME, uiVerbLevel);
			WriteLine(szMsg, V_SILENCE);
			return 1;
		}

		// halt all other commands due to BreakOnGM/BreakOnSummon
		if (pMU->BrokeGM || pMU->BrokeSummon)
		{
			sprintf(szMsg, "\ay%s\aw:: \arCommand failed due to \ay%s\ax triggering.", MODULE_NAME, pMU->BrokeGM ? "BreakOnGM" : "BreakOnSummon");
			WriteLine(szMsg, V_SILENCE);
			return 1;
		}
		// halt all other commands due to rootme being on
		if (pMU->Rooted)
		{
			sprintf(szMsg, "\ay%s\aw:: \arCommand failed due to \ayrootme\ax active.", MODULE_NAME);
			WriteLine(szMsg, V_SILENCE);
			return 1;
		}
		// halt new commands if plugin is paused & lockpause is on
		if (PAUSE->PausedCmd && pMU->LockPause)
		{
			sprintf(szMsg, "\ay%s\aw:: \arCommand failed due to plugin paused with lockpause.", MODULE_NAME);
			WriteLine(szMsg, V_PAUSED);
			return 1;
		}

		// non-generic parameters that we want to enforce first-parameter syntax
		if (!strnicmp(argv[1], "on", 3) && (ucCmdUsed == CMD_MAKECAMP || ucCmdUsed == CMD_CIRCLE))
		{
			switch (ucCmdUsed)
			{
			case CMD_MAKECAMP:
				PAUSE->PausedMU = false;
				PAUSE->TimeStop();
				CAMP->Activate(pChSpawn->Y, pChSpawn->X);
				if (argc > 2 && isdigit(argv[2][0]))
				{
					CURCAMP->SetRadius((float)atof(argv[2]));
					uiArgNum = 3; // because when we break from this we enter 'while' for NEW args
				}
				break;
			case CMD_CIRCLE:
				EndPreviousCmd(true);
				if (argc > 2 && isdigit(argv[2][0]))
				{
					CIRCLE->SetRadius((float)atof(argv[2]));
					uiArgNum = 3; // because when we break from this we enter 'while' for NEW args
				}
				CIRCLE->AtMe();
				break;
			}
		}
		else if (!strnicmp(argv[1], "mod", 4) && ucCmdUsed == CMD_STICK)
		{
			if (argc > 2 && validNumber(argv[2]))
			{
				STICK->DistMod = (float)atof(argv[2]);
				sprintf(szMsg, "\ay%s\aw:: Stick modifier changed to Mod(\ag%.2f\ax) Mod%%(\ag%.2f%%\ax)", MODULE_NAME, STICK->DistMod, STICK->DistModP);
				WriteLine(szMsg, V_SETTINGS);
				return 1;
			}
			sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) \ay/stick mod [#]\ax supplied incorrectly.", MODULE_NAME);
			WriteLine(szMsg, V_ERRORS);
			return 1;
		}
		else if(!strnicmp(argv[1], "loc", 4) && ucCmdUsed != CMD_STICK)
		{
			switch (ucCmdUsed)
			{
			case CMD_MOVETO:
				EndPreviousCmd(true);
				if (argc > 3 && validNumber(argv[2]) && validNumber(argv[3]))
				{
					fTempY = (float)atof(argv[2]);
					float fTempX = (float)atof(argv[3]);
					if (argc > 4 && validNumber(argv[4]))
					{
						MOVETO->Activate(fTempY, fTempX, (float)atof(argv[4]));
						uiArgNum = 5;
						break;
					}
					MOVETO->Activate(fTempY, fTempX, 0.0f);
					uiArgNum = 4;
					break;
				}
				SpewMUError(ERR_BADMOVETO);
				return 1;
			case CMD_MAKECAMP:
				PAUSE->PausedMU = false;
				PAUSE->TimeStop();
				if (argc > 3 && validNumber(argv[2]) && validNumber(argv[3]))
				{
					fTempY = (float)atof(argv[2]);
					CAMP->Activate(fTempY, (float)atof(argv[3]));
					uiArgNum = 4;
					break;
				}
				SpewMUError(ERR_BADMAKECAMP);
				return 1;
			case CMD_CIRCLE:
				EndPreviousCmd(true, ucCmdUsed, true); // dont reset circle variables
				if (argc > 3 && validNumber(argv[2]) && validNumber(argv[3]))
				{
					fTempY = (float)atof(argv[2]);
					CIRCLE->AtLoc(fTempY, (float)atof(argv[3]));
					uiArgNum = 4;
					break;
				}
				SpewMUError(ERR_BADCIRCLE);
				return 1;
			}
		}
		else if((!strnicmp(argv[1], "yloc", 5) || !strnicmp(argv[1], "xloc", 5)) && ucCmdUsed == CMD_MOVETO)
		{
			bool bUsingY = (!strnicmp(argv[1], "yloc", 5)) ? true : false;
			EndPreviousCmd(true);
			if (argc > 2 && validNumber(argv[2]))
			{
				if (bUsingY)
				{
					MOVETO->Activate((float)atof(argv[2]), pChSpawn->X, 0.0f);
				}
				else
				{
					MOVETO->Activate(pChSpawn->Y, (float)atof(argv[2]), 0.0f);
				}
				uiArgNum = 3;
			}
			else
			{
				sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) \ay/moveto %s\ax was supplied incorrectly.", MODULE_NAME, bUsingY ? "yloc [Y]" : "xloc [X]");
				WriteLine(szMsg, V_ERRORS);
				return 1;
			}
		}

		// reset stick vars one time
		if (ucCmdUsed == CMD_STICK)
		{
			EndPreviousCmd(true);
		}
	}

	while (argc > uiArgNum)
	{
		if (!strnicmp(argv[uiArgNum], "off", 4))
		{
			switch(ucCmdUsed)
			{
			case CMD_MAKECAMP:
				CAMP->ResetCamp(true);
				MOVE->StopHeading();
				return 1;
			case CMD_STICK:
				sprintf(szMsg, "\ay%s\aw:: You are no longer sticking to anything.", MODULE_NAME);
				WriteLine(szMsg, V_STICKV);
				break;
			case CMD_CIRCLE:
				sprintf(szMsg, " \ay%s\aw:: Circling radius (\ag%.2f\ax), center (\ag%.2f\ax, \ag%.2f\ax) : \arOFF", MODULE_NAME, CIRCLE->Radius, CIRCLE->Y, CIRCLE->X);
				WriteLine(szMsg, V_CIRCLEV);
				break;
			case CMD_MOVETO:
				sprintf(szMsg, "\ay%s\aw:: Moveto off.", MODULE_NAME);
				WriteLine(szMsg, V_MOVETOV);
				break;
			}
			EndPreviousCmd(true);
			return 1;
		}
		else if (!strnicmp(argv[uiArgNum], "id", 3) && ucCmdUsed != CMD_MAKECAMP)
		{
			EndPreviousCmd(true, ucCmdUsed, true);
			PSPAWNINFO pByID = NULL;

			if (argc > uiArgNum + 1)
			{
				char* pNotNum = NULL;
				int iValid = strtoul(argv[uiArgNum + 1], &pNotNum, 10);
				// strtoul verifies the arg is 100% numerical, atoi/atof do not
				if (iValid < 1 || *pNotNum)
				{
					EndPreviousCmd(true);
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) SpawnID must be a positive numerical value.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
				pByID = (PSPAWNINFO)GetSpawnByID((unsigned long)iValid);
				if (pByID)
				{
					if (ME->IsMe(pByID))
					{
						EndPreviousCmd(true);
						sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) You cannot use yourself or your mount.", MODULE_NAME);
						WriteLine(szMsg, V_ERRORS);
						return 1;
					}
					pTargetUsed = (PSPAWNINFO)pByID;
					uiArgNum++; // incremeted if # is valid, but not otherwise so that someone can use '/stick id behind' to use target. bad form but nonetheless
				}
				else
				{
					pTargetUsed = NULL;
				}
			}
			else if (psTarget)
			{
				if (ME->IsMe(psTarget))
				{
					SpewMUError(ERR_BADSPAWN);
					return 1;
				}
				pTargetUsed = psTarget; // only use target if its not ourself
			}
			if (!pTargetUsed)
			{
				SpewMUError(ERR_BADSPAWN);
				return 1;
			}
			//if we've made it this far, pTargetUsed is valid
			switch (ucCmdUsed)
			{
			case CMD_STICK:
				STICK->Always   = false; // turns off 'always' when using 'id'
				STICK->HoldID   = pTargetUsed->SpawnID;
				STICK->HoldType = GetSpawnType(pTargetUsed);
				STICK->Hold     = true;
				STICK->TurnOn();
				break;
			case CMD_MOVETO:
				// moveto id with GetDistance3D is causing some users issues
				MOVETO->Activate(pTargetUsed->Y, pTargetUsed->X, 0.0f);
				break;
			case CMD_CIRCLE:
				CIRCLE->AtLoc(pTargetUsed->Y, pTargetUsed->X);
				break;
			}
		}
		// stick specific parameters
		else if (ucCmdUsed == CMD_STICK)
		{
			if (strstr(argv[uiArgNum], "%"))
			{
				STICK->DistModP = (float)atof(argv[uiArgNum]) / 100.0f;
				// shouldnt do this here, need logic to output this only if used by itself
				// cant do it on an 'else' for the pTarget 'if' because of 'always' param
				sprintf(szMsg, "\ay%s\aw:: Stick mod changed Mod(\ag%.2f\ax) ModPercent(\ag%.2f%%\ax)", MODULE_NAME, STICK->DistMod, STICK->DistModP);
				WriteLine(szMsg, V_SETTINGS);
				if (STICK->SetDist && STICK->Dist * STICK->DistModP > 0.0f) STICK->Dist *= STICK->DistModP; // possible float math error here?
				STICK->TurnOn();
			}
			else if (argv[uiArgNum][0] == '-')
			{
				STICK->DistMod = (float)atof(argv[uiArgNum]);
				sprintf(szMsg, "\ay%s\aw:: Stick mod changed Mod(\ag%.2f\ax) ModPercent(\ag%.2f%%\ax)", MODULE_NAME, STICK->DistMod, STICK->DistModP);
				WriteLine(szMsg, V_SETTINGS);
				if (STICK->SetDist && STICK->Dist + STICK->DistMod >= 0.0f) STICK->Dist += STICK->DistMod; // possible float math error here?
				STICK->TurnOn();
			}
			else if (isdigit(argv[uiArgNum][0]) || argv[uiArgNum][0] == '.' )
			{
				if ((float)atof(argv[uiArgNum]) * STICK->DistModP + STICK->DistMod > 0.0f)
				{
					STICK->Dist = (float)atof(argv[uiArgNum]) * STICK->DistModP + STICK->DistMod;
				}
				STICK->SetDist = true;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "moveback", 9))
			{
				STICK->MoveBack = true;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "loose", 6))
			{
				pMU->Head = H_LOOSE;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "truehead", 9))
			{
				pMU->Head = H_TRUE;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "healer", 7))
			{
				STICK->Healer = true;
				STICK->Behind = STICK->BehindOnce = STICK->Front = STICK->Snaproll = false;
				STICK->NotFront = STICK->Pin = false;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "uw", 3) || !strnicmp(argv[uiArgNum], "underwater", 11))
			{
				STICK->UW = true;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "hold", 5))
			{
				if (psTarget)
				{
					if (ME->IsMe(psTarget))
					{
						sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) You cannot stick hold to yourself.", MODULE_NAME);
						WriteLine(szMsg, V_ERRORS);
						EndPreviousCmd(true);
						return 1;
					}
					pTargetUsed     = (PSPAWNINFO)psTarget;
					STICK->HoldID   = pTargetUsed->SpawnID;
					STICK->HoldType = GetSpawnType(pTargetUsed);
					STICK->Hold     = true;
					STICK->TurnOn();
				}
				else
				{
					EndPreviousCmd(true);
					SpewMUError(ERR_STICKNONE);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "on", 3))
			{
				// useless param, but removing it breaks popular macros
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "behind", 7))
			{
				STICK->Behind = true;
				STICK->Healer = STICK->Snaproll = STICK->BehindOnce = STICK->Pin = STICK->NotFront = STICK->Front = false;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "behindonce", 11))
			{
				STICK->BehindOnce = true;
				STICK->Healer = STICK->Snaproll = STICK->Behind = STICK->Pin = STICK->NotFront = STICK->Front = false;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "!front", 7))
			{
				STICK->NotFront = true;
				STICK->Healer = STICK->Snaproll = STICK->Behind = STICK->Pin = STICK->BehindOnce = STICK->Front = false;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "front", 6))
			{
				STICK->Front = true;
				STICK->Healer = STICK->Snaproll = STICK->Behind = STICK->Pin = STICK->BehindOnce = STICK->NotFront = false;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "pin", 4))
			{
				STICK->Pin = true;
				STICK->Healer = STICK->Snaproll = STICK->Behind = STICK->BehindOnce = STICK->NotFront = STICK->Front = false;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "snaproll", 9))
			{
				STICK->Snaproll = true;
				STICK->Healer = STICK->Behind = STICK->BehindOnce = STICK->NotFront = STICK->Front = STICK->Pin = false;
				STICK->Snap->Bearing = HEADING_HALF;
				if (argc > uiArgNum + 1) {
					char szTemp[MAX_STRING] = {0};
					pISInterface->GetArgs(uiArgNum, uiArgNum + 1, argv, szTemp, MAX_STRING);
					if (!strnicmp(szTemp, "face", 6)) {
						uiArgNum++;
						STICK->Snap->Bearing = 1.0f;
					} else if (!strnicmp(szTemp, "left", 5)) {
						uiArgNum++;
						STICK->Snap->Bearing = HEADING_QUARTER;
					} else if (!strnicmp(szTemp, "right", 6)) {
						uiArgNum++;
						STICK->Snap->Bearing = (HEADING_HALF + HEADING_QUARTER);
					} else if (!strnicmp(szTemp, "rear", 5)) {
						uiArgNum++; // uses HEADING_HALF (set above)
					}
				}
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "breakontarget", 14))
			{
				STICK->BreakTarget = true;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "breakongate", 12))
			{
				STICK->BreakGate = true;
				SetupEvents(true);
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "breakonwarp", 12))
			{
				STICK->BreakWarp = true;
				STICK->PauseWarp = false;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "pauseonwarp", 12))
			{
				STICK->PauseWarp = true;
				STICK->BreakWarp = false;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "randomize", 10))
			{
				STICK->Randomize = true;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "delaystrafe", 12))
			{
				STICK->DelayStrafe = true;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "useback", 8))
			{
				STICK->UseBack = true;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "usefleeing", 11))
			{
				STICK->UseFleeing = true;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "strafewalk", 11))
			{
				STICK->Walk = true;
				STICK->TurnOn();
			}
			else if (!strnicmp(argv[uiArgNum], "mindelay", 9) || !strnicmp(argv[uiArgNum], "maxdelay", 9))
			{
				bool bDoMax = !strnicmp(argv[uiArgNum], "maxdelay", 9) ? true : false;
				uiArgNum++;
				if (argc > uiArgNum && isdigit(argv[uiArgNum][0]))
				{
					if (bDoMax)
					{
						STICK->MaxDelay(atoi(argv[uiArgNum]));
					}
					else
					{
						STICK->MinDelay(atoi(argv[uiArgNum]));
					}
					STICK->TurnOn();
				}
				else
				{
					EndPreviousCmd(true);
					SpewMUError(ERR_BADDELAY);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "backupdist", 11))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f)
				{
					STICK->DistBack = (float)atof(argv[uiArgNum]);
					STICK->TurnOn();
				}
				else
				{
					EndPreviousCmd(true);
					sprintf(szMsg, "\ay%s\aw:: \arbackupdist must be 1.0 or larger.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "breakdist", 10))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f)
				{
					STICK->DistBreak = (float)atof(argv[uiArgNum]);
					STICK->TurnOn();
				}
				else
				{
					sprintf(szMsg, "\ay%s\aw:: \arbreakdist must be 1.0 or larger.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "snapdist", 9))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f)
				{
					STICK->DistSnap = (float)atof(argv[uiArgNum]);
					STICK->TurnOn();
				}
				else
				{
					EndPreviousCmd(true);
					sprintf(szMsg, "\ay%s\aw:: \arsnapdist must be 1.0 or larger.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "!frontarc", 10))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f && (float)atof(argv[uiArgNum]) <= 260.0f)
				{
					STICK->ArcNotFront = (float)atof(argv[uiArgNum]);
					STICK->TurnOn();
				}
				else
				{
					EndPreviousCmd(true);
					sprintf(szMsg, "\ay%s\aw:: \ar!frontarc must be between 1.0 and 260.0", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "behindarc", 10))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f && (float)atof(argv[uiArgNum]) <= 260.0f)
				{
					STICK->ArcBehind = (float)atof(argv[uiArgNum]);
					STICK->TurnOn();
				}
				else
				{
					EndPreviousCmd(true);
					sprintf(szMsg, "\ay%s\aw:: \arbehindarc must be between 1.0 and 260.0", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "always", 7))
			{
				STICK->FirstAlways();
				sprintf(szMsg, "\ay%s\aw:: You will now stick to every valid NPC target supplied.", MODULE_NAME);
				WriteLine(szMsg, V_STICKV);
				MOVE->DoStand();
				return 1; // 'always' must be the last parameter
			}
			else
			{
				EndPreviousCmd(true);
				if (uiVerbLevel != 0 && (uiVerbLevel & V_HIDEHELP) != V_HIDEHELP) OutputHelp(ucCmdUsed, true);
				return 1;
			}
		}
		// moveto specific parameters
		else if (ucCmdUsed == CMD_MOVETO)
		{
			if (!strnicmp(argv[uiArgNum], "loose", 6))
			{
				pMU->Head = H_LOOSE;
			}
			else if (!strnicmp(argv[uiArgNum], "truehead", 9))
			{
				pMU->Head = H_TRUE;
			}
			else if (!strnicmp(argv[uiArgNum], "dist", 5))
			{
				uiArgNum++;
				if (argc > uiArgNum && argv[uiArgNum][0] == '-')
				{
					MOVETO->Mod = (float)atof(argv[uiArgNum]);
					MOVETO->Dist += MOVETO->Mod;
				}
				else if (argc > uiArgNum && isdigit(argv[uiArgNum][0]))
				{
					MOVETO->Dist = ((float)atof(argv[uiArgNum]) >= 1.0f) ? (float)atof(argv[uiArgNum]) : MOVETO->Dist;
				}
				else
				{
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) Incorrectly used \ay/moveto dist [#]\ax", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
				SET_M->Dist = MOVETO->Dist;
				SET_M->Mod  = MOVETO->Mod;
				sprintf(szMsg, "\ay%s\aw:: Moveto distance mod changed to \ag%.2f\ax.", MODULE_NAME, MOVETO->Dist);
				WriteLine(szMsg, V_SETTINGS);
				return 1;
			}
			else if (!strnicmp(argv[uiArgNum], "mdist", 6))
			{
				uiArgNum++;
				if (argc > uiArgNum && argv[uiArgNum][0] == '-')
				{
					MOVETO->Mod = (float)atof(argv[uiArgNum]);
					MOVETO->Dist += MOVETO->Mod;
				}
				else if (argc > uiArgNum && isdigit(argv[uiArgNum][0]))
				{
					MOVETO->Dist = ((float)atof(argv[uiArgNum]) >= 1.0f) ? (float)atof(argv[uiArgNum]) : MOVETO->Dist;
				}
				else
				{
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) Incorrectly used \ay/moveto mdist [#]\ax", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
				SET_M->Dist = MOVETO->Dist;
				SET_M->Mod  = MOVETO->Mod;
			}
			else if (!strnicmp(argv[uiArgNum], "precisey", 9))
			{
				MOVETO->PreciseY = true;
				MOVETO->PreciseX = false;
			}
			else if (!strnicmp(argv[uiArgNum], "precisex", 9))
			{
				MOVETO->PreciseX = true;
				MOVETO->PreciseY = false;
			}
			else if (!strnicmp(argv[uiArgNum], "uw", 3) || !strnicmp(argv[uiArgNum], "underwater", 11))
			{
				MOVETO->UW = true;
			}
			else if (!strnicmp(argv[uiArgNum], "breakonaggro", 13))
			{
				MOVETO->BreakAggro = true;
				SetupEvents(true);
			}
			else if (!strnicmp(argv[uiArgNum], "breakonhit", 11))
			{
				MOVETO->BreakHit = true;
				SetupEvents(true);
			}
			else if (!strnicmp(argv[uiArgNum], "usewalk", 8))
			{
				MOVETO->Walk = true;
			}
			else if (!strnicmp(argv[uiArgNum], "useback", 8))
			{
				MOVETO->UseBack = true;
			}
			else if (!strnicmp(argv[uiArgNum], "backupdist", 11))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f)
				{
					MOVETO->DistBack = (float)atof(argv[uiArgNum]);
				}
				else
				{
					sprintf(szMsg, "\ay%s\aw:: \armoveto backup must be 1.0 or larger.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "ydist", 6))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f)
				{
					MOVETO->DistY = (float)atof(argv[uiArgNum]);
				}
				else
				{
					sprintf(szMsg, "\ay%s\aw:: \armoveto ydist must be 1.0 or larger.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "xdist", 6))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f)
				{
					MOVETO->DistX = (float)atof(argv[uiArgNum]);
				}
				else
				{
					sprintf(szMsg, "\ay%s\aw:: \armoveto xdist must be 1.0 or larger.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else
			{
				EndPreviousCmd(true);
				if (uiVerbLevel != 0 && (uiVerbLevel & V_HIDEHELP) != V_HIDEHELP) OutputHelp(ucCmdUsed, true);
				return 1;
			}
		}
		// makecamp specific parameters
		else if (ucCmdUsed == CMD_MAKECAMP)
		{
			if (!strnicmp(argv[uiArgNum], "leash", 6))
			{
				uiArgNum++;
				if (argc > uiArgNum && isdigit(argv[uiArgNum][0]))
				{
					CURCAMP->SetLeash((float)atof(argv[uiArgNum]));
					CURCAMP->Leash = true;
				}
				else
				{
					CURCAMP->Leash = !CURCAMP->Leash;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "radius", 7))
			{
				uiArgNum++;
				if (argc > uiArgNum && isdigit(argv[uiArgNum][0]))
				{
					CURCAMP->SetRadius((float)atof(argv[uiArgNum]));
				}
				else
				{
					CAMP->NewCamp(false);
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) \ay/makecamp [radius <dist>]\ax was supplied incorrectly.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "mindelay", 9) || !strnicmp(argv[uiArgNum], "maxdelay", 9))
			{
				bool bDoMax = !strnicmp(argv[uiArgNum], "maxdelay", 9) ? true : false;
				uiArgNum++;
				if (argc > uiArgNum && isdigit(argv[uiArgNum][0]))
				{
					if (bDoMax)
					{
						CAMP->MaxDelay(atoi(argv[uiArgNum]));
					}
					else
					{
						CAMP->MinDelay(atoi(argv[uiArgNum]));
					}
				}
				else
				{
					CAMP->NewCamp(false);
					SpewMUError(ERR_BADDELAY);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "return", 7))
			{
				if (!CURCAMP->On)
				{
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) You do not have an active camp.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
				CAMP->DoReturn = true;
				MOVE->DoStand();
				sprintf(szMsg, "\ay%s\aw:: MakeCamp returning to within camp radius immediately.", MODULE_NAME);
				WriteLine(szMsg, V_MAKECAMPV);
				return 1;
			}
			else if (!strnicmp(argv[uiArgNum], "altreturn", 10))
			{
				if (CURCAMP->Pc)
				{
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) You cannot use this command with a player-camp active.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
				if (!ALTCAMP->On || (ALTCAMP->X == 0.0f && ALTCAMP->Y == 0.0f))
				{
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) You cannot use this command until you've established an altcamp location.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
				sprintf(szMsg, "\ay%s\aw:: MakeCamp returning to altcamp immediately.%s", MODULE_NAME, CURCAMP->On ? " Current camp now \arOFF\ax." : "");
				CAMP->NewCamp(false);
				CAMP->DoAlt = true;
				MOVE->DoStand();
				WriteLine(szMsg, V_MAKECAMPV);
				return 1;
			}
			else if (!strnicmp(argv[uiArgNum], "player", 7))
			{
				if (argc > uiArgNum + 1)
				{
					uiArgNum++;
					pCampPlayer = (PSPAWNINFO)GetSpawnByName(argv[uiArgNum]);
				}
				else if (psTarget && psTarget->Type == SPAWN_PLAYER)
				{
					pCampPlayer = (PSPAWNINFO)GetSpawnByID(psTarget->SpawnID);
				}
				if (!pCampPlayer)
				{
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) Invalid player name and do not have a valid player target.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
				if (ME->IsMe(pCampPlayer))
				{
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) You cannot makecamp yourself.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
				// if we made it this far, pCampPlayer is valid
				CAMP->ActivatePC(pCampPlayer);
			}
			else if (!strnicmp(argv[uiArgNum], "returnhavetarget", 17))
			{
				CURCAMP->HaveTarget = true;
			}
			else if (!strnicmp(argv[uiArgNum], "returnnoaggro", 14))
			{
				CURCAMP->NoAggro = true;
			}
			else if (!strnicmp(argv[uiArgNum], "returnnotlooting", 17))
			{
				CURCAMP->NotLoot = true;
			}
			else if (!strnicmp(argv[uiArgNum], "realtimeplayer", 15))
			{
				CURCAMP->Realtime = true;
			}
			else if (!strnicmp(argv[uiArgNum], "scatter", 8))
			{
				CURCAMP->Scatter = true;
			}
			else if (!strnicmp(argv[uiArgNum], "bearing", 8))
			{
				uiArgNum++;
				if (argc > uiArgNum && isdigit(argv[uiArgNum][0]))
				{
					CURCAMP->Bearing = (float)atof(argv[uiArgNum]);
				}
				else
				{
					CAMP->NewCamp(false);
					sprintf(szMsg, "\ay%s\aw:: \armakecamp bearing parameter must be a number.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "scatsize", 9))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f)
				{
					CURCAMP->ScatSize = (float)atof(argv[uiArgNum]);
				}
				else
				{
					CAMP->NewCamp(false);
					sprintf(szMsg, "\ay%s\aw:: \armakecamp scatsize must be 1.0 or larger.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else if (!strnicmp(argv[uiArgNum], "scatdist", 9))
			{
				uiArgNum++;
				if (argc > uiArgNum && (float)atof(argv[uiArgNum]) > 1.0f)
				{
					CURCAMP->ScatDist = (float)atof(argv[uiArgNum]);
				}
				else
				{
					CAMP->NewCamp(false);
					sprintf(szMsg, "\ay%s\aw:: \armakecamp scatdist must be 1.0 or larger.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else
			{
				// reset camp on failed input?
				if (uiVerbLevel != 0 && (uiVerbLevel & V_HIDEHELP) != V_HIDEHELP) OutputHelp(ucCmdUsed, true);
				return 1;
			}
		}
		// circle specific parameters
		else if (ucCmdUsed == CMD_CIRCLE)
		{
			if (!strnicmp(argv[uiArgNum], "drunken", 8))
			{
				CIRCLE->Drunk = true;
			}
			else if (!strnicmp(argv[uiArgNum], "clockwise", 10) || !strnicmp(argv[uiArgNum], "cw", 3))
			{
				CIRCLE->CCW = false;
			}
			else if (!strnicmp(argv[uiArgNum], "ccw", 4) || !strnicmp(argv[uiArgNum], "counterclockwise", 17) || !strnicmp(argv[uiArgNum], "reverse", 8))
			{
				CIRCLE->CCW = true;
			}
			else if (!strnicmp(argv[uiArgNum], "forward", 7)) // allow 's'
			{
				CIRCLE->Backward = false;
			}
			else if (!strnicmp(argv[uiArgNum], "backward", 8)) // allow 's'
			{
				CIRCLE->Backward = true;
			}
			else if (!strnicmp(argv[uiArgNum], "radius", 7))
			{
				uiArgNum++;
				if (argc > uiArgNum && isdigit(argv[uiArgNum][0]))
				{
					CIRCLE->SetRadius((float)atof(argv[uiArgNum]));
				}
				else
				{
					sprintf(szMsg, "\ay%s\aw:: (\arERROR\ax) Use \ay/circle radius [#]\ax to set radius.", MODULE_NAME);
					WriteLine(szMsg, V_ERRORS);
					return 1;
				}
			}
			else
			{
				EndPreviousCmd(true);
				if (uiVerbLevel != 0 && (uiVerbLevel & V_HIDEHELP) != V_HIDEHELP) OutputHelp(ucCmdUsed, true);
				return 1;
			}
		}
		// no valid parameter given
		else
		{
			EndPreviousCmd(true);
			if (uiVerbLevel != 0 && (uiVerbLevel & V_HIDEHELP) != V_HIDEHELP) OutputHelp(ucCmdUsed);
			return 1;
		}
		uiArgNum++;
	}

	// Output Messages
	char szTempHead[50] = {0};
	switch (pMU->Head)
	{
	case H_TRUE:
		sprintf(szTempHead, "\agTrue\ax");
		break;
	case H_LOOSE:
		sprintf(szTempHead, "\ayLoose\ax");
		break;
	case H_FAST:
	default:
		sprintf(szTempHead, "\arFast\ax");
		break;
	}

	if (ucCmdUsed == CMD_STICK)
	{
		if (!STICK->Hold)
		{
			if (!psTarget)
			{
				// dont continue if no target unless 'always' (returns) or 'id || hold'
				EndPreviousCmd(true);
				SpewMUError(ERR_STICKNONE);
				return 1;
			}
			// if self targeted and not 'always' or 'id || hold'
			if (ME->IsMe(psTarget))
			{
				EndPreviousCmd(true);
				SpewMUError(ERR_STICKSELF);
				return 1;
			}
			// else setup output msg
			sprintf(szTempID, "%s", psTarget->DisplayedName);
		}
		else if (pTargetUsed || (STICK->Hold && psTarget))
		{
			// setup output msg for 'id || hold'
			sprintf(szTempID, "%s", pTargetUsed->DisplayedName);
		}
		else
		{
			// error checking in command line parsing should prevent this from ever happening
			// if user reports this error, needs investigation
			EndPreviousCmd(true);
			char szTempOut[MAX_STRING] = {0};
			sprintf(szTempOut, "\ay%s\aw:: \ar/stick NO TARGET ERROR", MODULE_NAME);
			WriteLine(szTempOut, V_SILENCE);
			return 1;
		}

		// randomize arcs
		STICK->DoRandomize();

		char szDir[25] = "\agAny\ax";
		if (STICK->Behind)
		{
			sprintf(szDir, "\ayBehind\ax");
		}
		else if (STICK->Pin) 
		{
			sprintf(szDir, "\aySide\ax");
		}
		else if (STICK->NotFront) 
		{
			sprintf(szDir, "\ayNot Front\ax");
		}
		else if (STICK->Front)
		{
			sprintf(szDir, "\ayFront\ax");
		}

		char szTempHold[25] = {0};
		if (STICK->Hold && pTargetUsed) sprintf(szTempHold, " ID(\ay%d\ax)", pTargetUsed->SpawnID);
		char szTempMod[50] = {0};
		bool bOutputMod = false;
		if (STICK->DistMod != 0.0f)
		{
			bOutputMod = true;
			if (STICK->DistModP != 1.0f)
			{
				sprintf(szTempMod, "Mod(\ag%.2f\ax | \ag%.2f%%\ax) ", STICK->DistMod, STICK->DistModP);
			}
			else
			{
				sprintf(szTempMod, "Mod(\ag%.2f\ax) ", STICK->DistMod);
			}
		}
		else if (STICK->DistModP != 1.0f)
		{
			bOutputMod = true;
			sprintf(szTempMod, "Mod(\ag%.2f%%\ax) ", STICK->DistModP);
		}

		if (STICK->Behind || STICK->BehindOnce || STICK->Pin || STICK->NotFront) STICK->Strafe = true;
		sprintf(szMsg, "\ay%s\aw:: You are now sticking to %s.", MODULE_NAME, szTempID);
		WriteLine(szMsg, V_STICKV);
		sprintf(szMsg, "\ay%s\aw:: Dir(%s) Dist(\ag%.2f\ax) %sHead(%s)%s%s%s%s", MODULE_NAME, szDir, STICK->Dist, bOutputMod ? szTempMod : "", szTempHead, STICK->Hold ? szTempHold : "", STICK->UW ? " \agUW\ax" : "", STICK->MoveBack ? " \agMB\ax" : "", STICK->Healer ? " \agHEALER\ax" : "");
		WriteLine(szMsg, V_STICKFV);
	}
	else if (ucCmdUsed == CMD_MOVETO)
	{
		char szInfoY[35] = {0};
		char szInfoX[35] = {0};
		char szZInfo[35] = {0};
		sprintf(szZInfo, " %.2f.", MOVETO->Z);
		sprintf(szInfoY, " YDist(\ag%.2f\ax)", MOVETO->DistY);
		sprintf(szInfoX, " XDist(\ag%.2f\ax)", MOVETO->DistX);
		sprintf(szMsg, "\ay%s\aw:: Moving to loc %.2f %.2f%s  Dist(\ag%.2f\ax) Head(%s)%s", MODULE_NAME, MOVETO->Y, MOVETO->X, MOVETO->Z != 0.0f ? szZInfo : ".", MOVETO->Dist, szTempHead, MOVETO->PreciseY ? szInfoY : (MOVETO->PreciseX ? szInfoX : ""));
		WriteLine(szMsg, V_MOVETOV);
		if (MOVETO->UsingPath) {
			sprintf(szMsg, "\ay%s\aw:: Using navMesh pathing with %d waypoints", MODULE_NAME, MOVETO->currentPathSize);
			WriteLine(szMsg, V_MOVETOV);
		}
	}
	else if (ucCmdUsed == CMD_CIRCLE)
	{
		sprintf(szMsg, " \ay%s\aw:: Circling radius (\ag%.2f\ax), center (\ag%.2f\ax, \ag%.2f\ax)%s%s%s - Head(%s) : %s", MODULE_NAME, CIRCLE->Radius, CIRCLE->Y, CIRCLE->X, CIRCLE->CCW ? ", Reverse" : "", CIRCLE->Backward ? ", Backwards" : "", CIRCLE->Drunk ? ", \agDrunken\ax" : "", szTempHead, CIRCLE->On ? "\agON" : "\arOFF");
		WriteLine(szMsg, V_CIRCLEV);
	}
	else if (ucCmdUsed == CMD_MAKECAMP)
	{
		if (!CURCAMP->Pc)
		{
			sprintf(szMsg, "\ay%s\aw:: MakeCamp (%s). Y(\ag%.2f\ax) X(\ag%.2f\ax) Radius(\ag%.2f\ax) Leash(%s - \ag%.2f\ax) Delay(\ag%d\ax to \ag%d\ax)", MODULE_NAME, CURCAMP->On ? "\agon\ax" : "\aroff\ax", CURCAMP->Y, CURCAMP->X, CURCAMP->Radius, CURCAMP->Leash ? "\agon\ax" : "\aroff\ax", CURCAMP->Length, CAMP->Min, CAMP->Max);
		}
		else
		{
			sprintf(szMsg, "\ay%s\aw:: MakeCamp Player (\ag%s\ax). Radius(\ag%.2f\ax) Leash(%s - \ag%.2f\ax) Delay(\ag%d\ax to \ag%d\ax)", MODULE_NAME, CURCAMP->PcName, CURCAMP->Radius, CURCAMP->Leash ? "\agon\ax" : "\aroff\ax", CURCAMP->Length, CAMP->Min, CAMP->Max);
		}
		WriteLine(szMsg, V_MAKECAMPV);
		return 1; // return so makecamp doesnt stand up
	}

	MOVE->DoStand();
	return 1;
}

// calcangle command
int CalcOurAngle(int argc, char *argv[])
{
	if (!ValidIngame() || !pTarget) return 0;
	PSPAWNINFO psTarget = (PSPAWNINFO)pTarget;
	PSPAWNINFO pChSpawn = (PSPAWNINFO)pCharSpawn;
	float fAngle   = MOVE->AngDist(psTarget->Heading, pChSpawn->Heading);
	float fReqHead = MOVE->SaneHead(atan2(psTarget->X - pChSpawn->X, psTarget->Y - pChSpawn->Y) * HEADING_HALF / (float)PI);
	fReqHead = pChSpawn->Heading - fReqHead;
	float fMeleeRng = get_melee_range(pLocalPlayer, (EQPlayer *)psTarget);
	float fStickRng = fMeleeRng * STICK->DistModP + STICK->DistMod;
	float fSaneH = MOVE->SaneHead(fReqHead);
	float fDist   = GetDistance(pChSpawn, psTarget);
	float fDist3D = GetDistance3D(pChSpawn, psTarget);
	char szTempOut[MAX_STRING] = {0};
	sprintf(szTempOut, "\ay%s\aw:: AngularDist ( \ag%.2f\ax ) Adjust ( \ag%.2f\ax ) Sane ( \ag%.2f\ax ) Dist ( \ag%.2f\ax ) Dist3D ( \ag%.2f\ax ) Melee ( \ag%f\ax ) Stick ( \ag%f\ax)", MODULE_NAME, fAngle, fReqHead, fSaneH, fDist, fDist3D, fMeleeRng, fStickRng);
	WriteChatf(szTempOut);
	sprintf(szTempOut, " - Walking ( %s )  RunSpeed ( \ag%.2f\ax ) SpeedMultiplier ( \ag%.2f\ax )", (*EQADDR_RUNWALKSTATE) ? "\arno\ax" : "\agyes\ax", pChSpawn->RunSpeed, pChSpawn->SpeedMultiplier);
	WriteChatf(szTempOut);
	return 1;
}

// root command
int RootCmd(int argc, char *argv[])
{
	if (!ValidIngame()) return 0;
	char szTempOut[MAX_STRING] = {0};

	if (SET->WinEQ || bOffsetOverride)
	{
		sprintf(szTempOut, "\ay%s\aw:: \arUnable to use this command due to use of old movement type.", MODULE_NAME);
		WriteLine(szTempOut, V_SILENCE);
		return 0;
	}

	if (argc == 2) {
		if (!strnicmp(argv[1], "off", 4))
		{
			MOVE->StopRoot();
		}
	}
	else
	{
		EndPreviousCmd(true);
		CAMP->NewCamp(CURCAMP->On);
		pMU->Rooted = true;
		MOVE->RootHead = ((PSPAWNINFO)pCharSpawn)->Heading;
		sprintf(szTempOut, "\ay%s\aw:: You are now rooted in place.", MODULE_NAME);
		WriteLine(szTempOut, V_SILENCE);
	}
	return 1;
}

// End user command input handling
// ----------------------------------------
// settings - saved to INI

bool ToggleSetting(const char* pszToggleOutput, bool* pbEvalThis, bool* pbUsedToggle, bool* pbTurnOn)
{
	char szTheMsg[MAX_STRING] = {0};

	// setting we are changing = did we use 'toggle' ? setting toggled ELSE set to 'on' or 'off'
	*pbEvalThis = *pbUsedToggle ? !*pbEvalThis : *pbTurnOn;
	sprintf(szTheMsg, "\ay%s\aw:: %s turned %s", MODULE_NAME, pszToggleOutput, *pbEvalThis ? szOn : szOff);
	WriteLine(szTheMsg, V_SETTINGS);
	return *pbEvalThis; // changesetting is only evaluating
}

int ChangeSetting(int argc, char *argv[])
{
	char szSetError[MAX_STRING]  = {0};

	// argc[0] = command
	// argc[1] = set || toggle
	// argc[2] = key
	// argc[3] = value (optional)

	// improper arg count, give error and return
	if (argc < 3 || argc > 4) {
		char szArgs[MAX_STRING] = {0};
		pISInterface->GetArgs(1,argc,argv,szArgs,MAX_STRING);
		sprintf(szSetError, "\ay%s\aw:: \arERROR\ax: Invalid '%s' syntax ( \ar%s\ax )", MODULE_NAME, argv[0], szArgs);
		WriteLine(szSetError, V_ERRORS);
		return 1;
	}

	unsigned char ucCmdUsed;
	if (!strnicmp(argv[0], "stick", 6)) {
		ucCmdUsed = CMD_STICK;
	} else if (!strnicmp(argv[0], "moveto", 7)) {
		ucCmdUsed = CMD_MOVETO;
	} else if (!strnicmp(argv[0], "circle", 7)) {
		ucCmdUsed = CMD_CIRCLE;
	} else if (!strnicmp(argv[0], "makecamp", 9)) {
		ucCmdUsed = CMD_MAKECAMP;
	}

	// are we setting, or toggling?
	bool bToggle = (!strnicmp(argv[1], "set", 4) ? false : true);

	char *szCommand = argv[0];
	char *szParameter = argv[2];
	char *szSetState = argv[3];
	char szSetDigit[MAX_STRING]  = {0};
	bool bTurnOn                 = false;
	bool bSetDigit               = false;
	bool bCustomMsg              = false;

	// check for valid parameter if "set" used (on, off, number)
	if (!bToggle)
	{
		if (isdigit(szSetState[0]) || szSetState[0] == '.' || szSetState[0] == '-')
		{
			bSetDigit = true;
			sprintf(szSetDigit, szSetState); // for naming clarification only, waste ram ftw.
		}
		else if (!strnicmp(szSetState, "on", 3))
		{
			bTurnOn = true;
		}
		else if (!strnicmp(szSetState, "off", 4))
		{
			bTurnOn = false; // serves no point other than to confirm valid input
		}
		else if (!strnicmp(szParameter, "heading", 7))
		{
			if (!strnicmp(szSetState, "true", 5))
			{
				pMU->Head = SET->Head = H_TRUE;
				sprintf(szMsg, "\ay%s\aw:: Heading adjustment type set to: \agtrue", MODULE_NAME);
			}
			else if (!strnicmp(szSetState, "loose", 6))
			{
				pMU->Head = SET->Head = H_LOOSE;
				sprintf(szMsg, "\ay%s\aw:: Heading adjustment type set to: \ayloose", MODULE_NAME);
			}
			else if (!strnicmp(szSetState, "fast", 5))
			{
				pMU->Head = SET->Head = H_FAST;
				sprintf(szMsg, "\ay%s\aw:: Heading adjustment type set to: \arfast", MODULE_NAME);
			}
			else
			{
				sprintf(szSetError, "\ay%s\aw:: \arERROR\ax: Invalid '%s set' syntax ( \ar%s\ax ) [true|loose|fast]", MODULE_NAME, szCommand, szParameter);
				WriteLine(szSetError, V_ERRORS);
				return 1;
			}
			WriteLine(szMsg, V_SETTINGS);
			if (SET->AutoSave) SaveConfig();
			return 1;
		}
		else
		{
			sprintf(szSetError, "\ay%s\aw:: \arERROR\ax: Invalid '%s set' syntax ( \ar%s\ax ) [on|off|number]", MODULE_NAME, szCommand, szParameter);
			WriteLine(szSetError, V_ERRORS);
			return 1;
		}
	}

	if (!bCustomMsg && (bToggle || !bSetDigit))
	{
		if (!strnicmp(szParameter, "mpause", 7))
		{
			if (ToggleSetting("Pause from manual movement", &SET->PauseKB, &bToggle, &bTurnOn))
			{
				SET->BreakKB = false;
			}
		}
		else if (!strnicmp(szParameter, "mousepause", 11))
		{
			if (ToggleSetting("Pause from mouse movement", &SET->PauseMouse, &bToggle, &bTurnOn))
			{
				SET->BreakMouse = false;
			}
		}
		else if (!strnicmp(szParameter, "breakonkb", 10))
		{
			if (ToggleSetting("Break from keyboard movement", &SET->BreakKB, &bToggle, &bTurnOn))
			{
				SET->PauseKB = false;
			}
		}
		else if (!strnicmp(szParameter, "breakonmouse", 13))
		{
			if (ToggleSetting("Break from mouse movement", &SET->BreakMouse, &bToggle, &bTurnOn))
			{
				SET->PauseMouse = false;
			}
		}
		else if (!strnicmp(szParameter, "autosave", 9))
		{
			ToggleSetting("Auto-save settings to INI file", &SET->AutoSave, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "savebychar", 11))
		{
			ToggleSetting("Save INI file Character Name section", &SET->SaveByChar, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "feign", 6))
		{
			ToggleSetting("Remain feign support", &SET->Feign, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "lockpause", 10))
		{
			ToggleSetting("Prevent command usage breaking pause", &SET->LockPause, &bToggle, &bTurnOn);
			pMU->LockPause = SET->LockPause;
		}
		else if (!strnicmp(szParameter, "autopause", 10))
		{
			ToggleSetting("AutoPause upon spell cast", &SET->AutoPause, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "autopauseoutput", 16))
		{
			if (bToggle)
			{
				uiVerbLevel ^= V_AUTOPAUSE;
			}
			else if (bTurnOn)
			{
				uiVerbLevel |= V_AUTOPAUSE;
			}
			else
			{
				uiVerbLevel &= ~V_AUTOPAUSE;
			}
			sprintf(szMsg, "\ay%s\aw:: Display AutoPause output: %s", MODULE_NAME, (uiVerbLevel & V_AUTOPAUSE) == V_AUTOPAUSE ? szOn : szOff);
			WriteLine(szMsg, V_SETTINGS);
		}
		else if (!strnicmp(szParameter, "stucklogic", 13))
		{
			ToggleSetting("Stuck-checking logic", &STUCK->On, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "trytojump", 10))
		{
			ToggleSetting("Try to jump when stuck", &STUCK->Jump, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "turnhalf", 9))
		{
			ToggleSetting("Switch direction if turned halfway (stucklogic)", &STUCK->TurnHalf, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "verbosity", 10))
		{
			if (bToggle)
			{
				uiVerbLevel ^= V_VERBOSITY;
			}
			else if (bTurnOn)
			{
				uiVerbLevel |= V_VERBOSITY;
			}
			else
			{
				uiVerbLevel &= ~V_VERBOSITY;
			}
			sprintf(szMsg, "\ay%s\aw:: Basic verbosity turned: %s", MODULE_NAME, (uiVerbLevel & V_VERBOSITY) == V_VERBOSITY ? szOn : szOff);
			WriteLine(szMsg, V_SETTINGS);
		}
		else if (!strnicmp(szParameter, "fullverbosity", 14))
		{
			if (bToggle)
			{
				uiVerbLevel ^= V_FULLVERBOSITY;
			}
			else if (bTurnOn)
			{
				uiVerbLevel |= V_FULLVERBOSITY;
			}
			else
			{
				uiVerbLevel &= ~V_FULLVERBOSITY;
			}
			sprintf(szMsg, "\ay%s\aw:: Enhanced verbosity turned: %s", MODULE_NAME, (uiVerbLevel & V_FULLVERBOSITY) == V_FULLVERBOSITY ? szOn : szOff);
			WriteLine(szMsg, V_SETTINGS);
		}
		else if (!strnicmp(szParameter, "totalsilence", 13))
		{
			if (bToggle)
			{
				if (uiVerbLevel != V_SILENCE)
				{
					uiRetainFlags = uiVerbLevel;
					uiVerbLevel = V_SILENCE;
				}
				else
				{
					uiVerbLevel = uiRetainFlags;
				}
			}
			else if (bTurnOn)
			{
				uiRetainFlags = uiVerbLevel;
				uiVerbLevel = V_SILENCE;
			}
			else
			{
				uiVerbLevel = uiRetainFlags;
			}
			sprintf(szMsg, "\ay%s\aw:: Plugin silence turned: %s", MODULE_NAME, (uiVerbLevel == V_SILENCE) ? szOn : szOff);
			WriteLine(szMsg, V_SILENCE);
		}
		else if (!strnicmp(szParameter, "totalverbosity", 15))
		{
			if (bToggle)
			{
				if (uiVerbLevel != V_EVERYTHING)
				{
					uiRetainFlags = uiVerbLevel;
					uiVerbLevel = V_EVERYTHING;
				}
				else
				{
					uiVerbLevel = uiRetainFlags;
				}
			}
			else if (bTurnOn)
			{
				uiRetainFlags = uiVerbLevel;
				uiVerbLevel = V_EVERYTHING;
			}
			else
			{
				uiVerbLevel = uiRetainFlags;
			}
			sprintf(szMsg, "\ay%s\aw:: Plugin total verbosity turned: %s", MODULE_NAME, (uiVerbLevel == V_EVERYTHING) ? szOn : szOff);
			WriteLine(szMsg, V_SILENCE);
		}
		else if (!strnicmp(szParameter, "wineq", 6))
		{
			ToggleSetting("Use old-style movement", &SET->WinEQ, &bToggle, &bTurnOn);
			if (SET->WinEQ)
			{
				if (SET->Head == H_TRUE) SET->Head = H_LOOSE;
				if (pMU->Head == H_TRUE) pMU->Head = H_LOOSE;
			}
		}
		else if (!strnicmp(szParameter, "hidehelp", 9))
		{
			if (bToggle)
			{
				uiVerbLevel ^= V_HIDEHELP;
			}
			else if (bTurnOn)
			{
				uiVerbLevel |= V_HIDEHELP;
			}
			else
			{
				uiVerbLevel &= ~V_HIDEHELP;
			}
			sprintf(szMsg, "\ay%s\aw:: Hide automatic help output: %s", MODULE_NAME, (uiVerbLevel & V_HIDEHELP) == V_HIDEHELP ? szOn : szOff);
			WriteLine(szMsg, V_SETTINGS);
		}
		else if (!strnicmp(szParameter, "breakontarget", 14))
		{
			STICK->BreakTarget = ToggleSetting("Break stick on target change", &SET_S->BreakTarget, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "breakonsummon", 14))
		{
			ToggleSetting("Break command when summoned", &SET->BreakSummon, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "breakongm", 10))
		{
			ToggleSetting("Break command when visible GM enters", &SET->BreakGM, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "breakonwarp", 12))
		{
			if (ToggleSetting("Break command when NPC warps away", &SET_S->BreakWarp, &bToggle, &bTurnOn))
			{
				SET_S->PauseWarp = STICK->PauseWarp = false;
			}
			STICK->BreakWarp = SET_S->BreakWarp;
		}
		else if (!strnicmp(szParameter, "pauseonwarp", 12))
		{
			if (ToggleSetting("Pause command when NPC warps away", &SET_S->PauseWarp, &bToggle, &bTurnOn))
			{
				SET_S->PauseWarp = STICK->BreakWarp = false;
			}
			STICK->PauseWarp = SET_S->PauseWarp;
		}
		else if (!strnicmp(szParameter, "breakongate", 12))
		{
			SetupEvents(ToggleSetting("Break command when NPC gates", &SET_S->BreakGate, &bToggle, &bTurnOn));
			STICK->BreakGate = SET_S->BreakGate;
		}
		else if (!strnicmp(szParameter, "breakonhit", 11))
		{
			SetupEvents(ToggleSetting("Break MoveTo if attacked", &SET_M->BreakHit, &bToggle, &bTurnOn));
			MOVETO->BreakHit = SET_M->BreakHit;
		}
		else if (!strnicmp(szParameter, "breakonaggro", 13))
		{
			SetupEvents(ToggleSetting("Break MoveTo if aggro", &SET_M->BreakAggro, &bToggle, &bTurnOn));
			MOVETO->BreakAggro = SET_M->BreakAggro;
		}
		else if (!strnicmp(szParameter, "alwaysdrunk", 12))
		{
			CIRCLE->Drunk = ToggleSetting("Circle always drunken", &SET_C->Drunk, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "alwaysbackwards", 16))
		{
			CIRCLE->Backward = ToggleSetting("Circle always backwards", &SET_C->Backward, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "alwaysccw", 10))
		{
			CIRCLE->CCW = ToggleSetting("Circle always counter-clockwise", &SET_C->CCW, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "alwaysuw", 9) && (ucCmdUsed == CMD_MOVETO || ucCmdUsed == CMD_STICK))
		{
			if (ucCmdUsed == CMD_MOVETO)
			{
				MOVETO->UW = ToggleSetting("Always use underwater parameter with /moveto", &SET_M->UW, &bToggle, &bTurnOn);
			}
			else
			{
				STICK->UW = ToggleSetting("Always use underwater parameter with /stick", &SET_S->UW, &bToggle, &bTurnOn);
			}
		}
		else if (!strnicmp(szParameter, "loose", 6) && ucCmdUsed != CMD_MAKECAMP)
		{
			pMU->Head = bToggle ? (pMU->Head == H_LOOSE ? H_FAST : H_LOOSE) : (bTurnOn ? H_LOOSE : H_FAST);
			sprintf(szMsg, "\ay%s\aw:: Active \ay%s\ax loose heading turned %s (Cur: %s)", MODULE_NAME, szCommand, (pMU->Head == H_LOOSE) ? szOn : szOff, (pMU->Head == H_LOOSE) ? "\ayloose\ax" : "\arfast\ax");
			WriteLine(szMsg, V_SETTINGS);
		}
		else if (!strnicmp(szParameter, "truehead", 9) && ucCmdUsed != CMD_MAKECAMP)
		{
			pMU->Head = bToggle ? (pMU->Head == H_TRUE ? H_FAST : H_TRUE) : (bTurnOn ? H_TRUE : H_FAST);
			sprintf(szMsg, "\ay%s\aw:: Active \ay%s\ax true turning turned %s (Cur: %s)", MODULE_NAME, szCommand, (pMU->Head == H_TRUE) ? szOn : szOff, (pMU->Head == H_TRUE) ? "\ayloose\ax" : "\arfast\ax");
			WriteLine(szMsg, V_SETTINGS);
		}
		else if (!strnicmp(szParameter, "nohottfront", 12))
		{
			ToggleSetting("Spin in circles when I lose aggro with no HoTT using '/stick front'", &SET->Spin, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "returnnoaggro", 14))
		{
			CURCAMP->NoAggro = ToggleSetting("Return to camp when not aggro", &SET_CAMP->NoAggro, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "returnnotlooting", 17))
		{
			CURCAMP->NotLoot = ToggleSetting("Return to camp when not looting", &SET_CAMP->NotLoot, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "returnhavetarget", 17))
		{
			CURCAMP->HaveTarget = ToggleSetting("Return to camp regardless of target", &SET_CAMP->HaveTarget, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "realtimeplayer", 15))
		{
			CURCAMP->Realtime = ToggleSetting("Return to realtime camp player location", &SET_CAMP->Realtime, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "leash", 6))
		{
			CURCAMP->Leash = ToggleSetting("Leash to camp", &SET_CAMP->Leash, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "usewalk", 8))
		{
			MOVETO->Walk = ToggleSetting("Walk when close to moveto/camp return destination", &SET_M->Walk, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "strafewalk", 11))
		{
			STICK->Walk = ToggleSetting("Walk when stick is close and strafing", &SET_S->Walk, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "randomize", 10))
		{
			STICK->Randomize = ToggleSetting("Randomize custom arcs", &SET_S->Randomize, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "delaystrafe", 12))
		{
			STICK->DelayStrafe = ToggleSetting("Enable custom stick angle delay", &SET_S->DelayStrafe, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "autouw", 7))
		{
			ToggleSetting("Automatically use 'uw' when underwater", &SET->AutoUW, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "useback", 8) && (ucCmdUsed == CMD_MOVETO || ucCmdUsed == CMD_STICK))
		{
			if (ucCmdUsed == CMD_MOVETO)
			{
				MOVETO->UseBack = ToggleSetting("Use backward movement when close to destination", &SET_M->UseBack, &bToggle, &bTurnOn);
			}
			else
			{
				STICK->UseBack = ToggleSetting("Use backwards movement when close to target", &SET_S->UseBack, &bToggle, &bTurnOn);
			}
		}
		else if (!strnicmp(szParameter, "usefleeing", 11))
		{
			STICK->UseFleeing = ToggleSetting("Stick front fleeing mob detection", &SET_S->UseFleeing, &bToggle, &bTurnOn);
		}
		else if (!strnicmp(szParameter, "usescatter", 11))
		{
			bCustomMsg = true;
			SET_CAMP->Scatter = bToggle ? !SET_CAMP->Scatter : bTurnOn;
			CURCAMP->Scatter = SET_CAMP->Scatter;
			sprintf(szMsg, "\ay%s\aw:: Scatter camp returns (%s) ScatDist(\ag%.2f\ax) Bearing(\ag%.2f\ax) ScatDist(\ag%.2f\ax) ScatSize(\ag%.2f\ax)", MODULE_NAME, SET_CAMP->Scatter ? szOn : szOff, SET_CAMP->ScatDist, SET_CAMP->Bearing, SET_CAMP->ScatSize);
		}
		else
		{
			sprintf(szSetError, "\ay%s\aw:: \arERROR\ax: Not a valid %s %s ( \ar%s\ax ).", MODULE_NAME, szCommand, bToggle ? "toggle" : "set option", szParameter);
			WriteLine(szSetError, V_ERRORS);
			return 1;
		}

		if (!bCustomMsg)
		{
			if (SET->AutoSave) SaveConfig();
			return 1;
		}
	}
	else if (!bCustomMsg && bSetDigit)
	{
		float fDigit = (float)atof(szSetDigit);
		if (!strnicmp(szParameter, "pulsecheck", 11))
		{
			STUCK->Check = (unsigned int)fDigit > 1 ? (unsigned int)fDigit : STUCK->Check;
			sprintf(szMsg, "\ay%s\aw:: StuckLogic pulse check rate set to \ag%d\ax pulses.", MODULE_NAME, STUCK->Check);
		}
		else if (!strnicmp(szParameter, "pulseunstuck", 13))
		{
			STUCK->Unstuck = (unsigned int)fDigit > 1 ? (unsigned int)fDigit : STUCK->Unstuck;
			sprintf(szMsg, "\ay%s\aw:: StuckLogic pulse unstuck value set to \ag%d\ax pulses.", MODULE_NAME, STUCK->Unstuck);
		}
		else if (!strnicmp(szParameter, "diststuck", 10))
		{
			STUCK->Dist = fDigit > 0.0f ? fDigit : STUCK->Dist;
			sprintf(szMsg, "\ay%s\aw:: StuckLogic distance moved per pulse (else stuck) set to \ag%.3f", MODULE_NAME, STUCK->Dist);
		}
		else if (!strnicmp(szParameter, "campmindelay", 13))
		{
			SET_CAMP->MinDelay((int)fDigit);
			CAMP->MinDelay(SET_CAMP->Min);
			sprintf(szMsg, "\ay%s\aw:: Mindelay for camp return set to \ag%d", MODULE_NAME, SET_CAMP->Min);
		}
		else if (!strnicmp(szParameter, "campmaxdelay", 13))
		{
			SET_CAMP->MaxDelay((int)fDigit);
			CAMP->MaxDelay(SET_CAMP->Max);
			sprintf(szMsg, "\ay%s\aw:: Maxdelay for camp return set to \ag%d", MODULE_NAME, CAMP->Max);
		}
		else if (!strnicmp(szParameter, "pausemindelay", 14))
		{
			PAUSE->MinDelay((int)fDigit);
			sprintf(szMsg, "\ay%s\aw:: Mindelay for mpause/mousepause set to \ag%d", MODULE_NAME, PAUSE->Min);
		}
		else if (!strnicmp(szParameter, "pausemaxdelay", 14))
		{
			PAUSE->MaxDelay((int)fDigit);
			sprintf(szMsg, "\ay%s\aw:: Maxdelay for mpause/mousepause set to \ag%d", MODULE_NAME, PAUSE->Max);
		}
		else if (!strnicmp(szParameter, "strafemindelay", 15))
		{
			SET_S->MinDelay((int)fDigit);
			STICK->MinDelay(SET_S->Min);
			sprintf(szMsg, "\ay%s\aw:: Mindelay for strafe resume set to \ag%d", MODULE_NAME, SET_S->Min);
		}
		else if (!strnicmp(szParameter, "strafemaxdelay", 15))
		{
			SET_S->MaxDelay((int)fDigit);
			STICK->MaxDelay(SET_S->Max);
			sprintf(szMsg, "\ay%s\aw:: Maxdelay for strafe resume set to \ag%d", MODULE_NAME, SET_S->Max);
		}
		else if (!strnicmp(szParameter, "ydist", 6))
		{
			SET_M->DistY = fDigit >= 1.0f ? fDigit : SET_M->DistY;
			MOVETO->DistY = SET_M->DistY;
			sprintf(szMsg, "\ay%s\aw:: MoveTo Y-Precision set to \ag%.2f", MODULE_NAME, SET_M->DistY);
		}
		else if (!strnicmp(szParameter, "xdist", 6))
		{
			SET_M->DistX = fDigit >= 1.0f ? fDigit : SET_M->DistX;
			MOVETO->DistY = SET_M->DistY;
			sprintf(szMsg, "\ay%s\aw:: MoveTo X-Precision set to \ag%.2f", MODULE_NAME, SET_M->DistX);
		}
		else if (!strnicmp(szParameter, "dist", 5) && ucCmdUsed == CMD_MOVETO)
		{
			SET_M->Dist = fDigit >= 1.0f ? fDigit : SET_M->Dist;
			MOVETO->Dist = SET_M->Dist;
			sprintf(szMsg, "\ay%s\aw:: MoveTo ArrivalDist set to \ag%.2f", MODULE_NAME, SET_M->Dist);
		}
		else if (!strnicmp(szParameter, "snapdist", 9))
		{
			SET_S->DistSnap = fDigit >= 1.0f ? fDigit : SET_S->DistSnap;
			STICK->DistSnap = SET_S->DistSnap;
			sprintf(szMsg, "\ay%s\aw:: Snaproll Distance from target set to \ag%.2f", MODULE_NAME, SET_S->DistSnap);
		}
		else if (!strnicmp(szParameter, "summondist", 11))
		{
			SET->DistSummon = fDigit >= 1.0f ? fDigit : SET->DistSummon;
			sprintf(szMsg, "\ay%s\aw:: BreakOnSummon distance set to \ag%.2f", MODULE_NAME, SET->DistSummon);
		}
		else if (!strnicmp(szParameter, "turnrate", 9))
		{
			SET->TurnRate = fDigit >= 1.0f && fDigit <= 100.0f ? fDigit : SET->TurnRate;
			sprintf(szMsg, "\ay%s\aw:: Loose Turn Rate set to \ag%.2f", MODULE_NAME, SET->TurnRate);
		}
		else if (!strnicmp(szParameter, "!frontarc", 10))
		{
			SET_S->ArcNotFront = fDigit <= 260.0f && fDigit > 1.0f ? fDigit : SET_S->ArcNotFront;
			STICK->ArcNotFront = SET_S->ArcNotFront;
			sprintf(szMsg, "\ay%s\aw:: !front arc set to \ag%.2f", MODULE_NAME, SET_S->ArcNotFront);
		}
		else if (!strnicmp(szParameter, "behindarc", 10))
		{
			SET_S->ArcBehind = fDigit <= 260.0f && fDigit > 1.0f ? fDigit : SET_S->ArcBehind;
			STICK->ArcBehind = SET_S->ArcBehind;
			sprintf(szMsg, "\ay%s\aw:: behind arc set to \ag%.2f", MODULE_NAME, SET_S->ArcBehind);
		}
		else if (!strnicmp(szParameter, "breakdist", 10))
		{
			SET_S->DistBreak = fDigit >= 1.0f ? fDigit : SET_S->DistBreak;
			STICK->DistBreak = SET_S->DistBreak;
			sprintf(szMsg, "\ay%s\aw:: BreakOnWarp dist set to \ag%.2f", MODULE_NAME, SET_S->DistBreak);
		}
		else if (!strnicmp(szParameter, "campradius", 11))
		{
			SET_CAMP->SetRadius(fDigit);
			CURCAMP->SetRadius(SET_CAMP->Radius);
			sprintf(szMsg, "\ay%s\aw:: Camp radius set to \ag%.2f", MODULE_NAME, SET_CAMP->Radius);
		}
		else if (!strnicmp(szParameter, "circleradius", 13))
		{
			SET_C->SetRadius(fDigit);
			CIRCLE->SetRadius(SET_C->Radius);
			sprintf(szMsg, "\ay%s\aw:: Circle radius set to \ag%.2f", MODULE_NAME, SET_C->Radius);
		}
		else if (!strnicmp(szParameter, "leashlength", 12))
		{
			SET_CAMP->SetLeash(fDigit);
			CURCAMP->SetLeash(SET_CAMP->Length);
			sprintf(szMsg, "\ay%s\aw:: Leash length set to \ag%.2f", MODULE_NAME, SET_CAMP->Length);
		}
		else if (!strnicmp(szParameter, "bearing", 8))
		{
			SET_CAMP->Bearing = fDigit;
			CURCAMP->Bearing = SET_CAMP->Bearing;
			sprintf(szMsg, "\ay%s\aw:: Camp return scatter bearing set to \ag%.2f", MODULE_NAME, SET_CAMP->Bearing);
		}
		else if (!strnicmp(szParameter, "scatsize", 9))
		{
			SET_CAMP->ScatSize = fDigit >= 1.0f ? fDigit : SET_CAMP->ScatSize;
			CURCAMP->ScatSize = SET_CAMP->ScatSize;
			sprintf(szMsg, "\ay%s\aw:: Camp return scatter size set to \ag%.2f", MODULE_NAME, SET_CAMP->ScatSize);
		}
		else if (!strnicmp(szParameter, "scatdist", 9))
		{
			SET_CAMP->ScatDist = fDigit >= 1.0f ? fDigit : SET_CAMP->ScatDist;
			CURCAMP->ScatDist = SET_CAMP->ScatDist;
			sprintf(szMsg, "\ay%s\aw:: Camp return scatter dist set to \ag%.2f", MODULE_NAME, SET_CAMP->ScatDist);
		}
		else if (!strnicmp(szParameter, "backupdist", 11) && (ucCmdUsed == CMD_MOVETO || ucCmdUsed == CMD_STICK))
		{
			if (ucCmdUsed == CMD_MOVETO)
			{
				SET_M->DistBack = fDigit > 1.0f ? fDigit : SET_M->DistBack;
				MOVETO->DistBack = SET_M->DistBack;
			}
			else
			{
				SET_S->DistBack = fDigit > 1.0f ? fDigit : SET_S->DistBack;
				STICK->DistBack = SET_S->DistBack;
			}
			sprintf(szMsg, "\ay%s\aw:: Range to use %s backwards positioning set to \ag%.2f", MODULE_NAME, szCommand, (ucCmdUsed == CMD_MOVETO) ? SET_M->DistBack : SET_S->DistBack);
		}
		else if (!strnicmp(szParameter, "allowmove", 10))
		{
			SET->AllowMove = fDigit > 10.0f ? fDigit : SET->AllowMove;
			sprintf(szMsg, "\ay%s\aw:: Allow movement when turning at \ag%.2f", MODULE_NAME, SET->AllowMove);
		}
		else if (!strnicmp(szParameter, "verbflags", 10))
		{
			int iNewLevel = atoi(szSetDigit);
			if (iNewLevel > 0)
			{
				uiVerbLevel = (unsigned int)iNewLevel;
			}
			else
			{
				// set to silence for both 0 and negative numbers
				uiVerbLevel = V_SILENCE;
			}
			sprintf(szMsg, "\ay%s\aw:: Verbosity flags set to \ag%d", MODULE_NAME, uiVerbLevel);
		}
		else
		{
			sprintf(szSetError, "\ay%s\aw:: \arERROR\ax: Invalid '%s set' parameter ( \ar%s\ax )", MODULE_NAME, szCommand, szParameter);
			WriteLine(szSetError, V_ERRORS);
			return 1;
		}
	}

	if (SET->AutoSave) SaveConfig();
	WriteLine(szMsg, V_SETTINGS);
	return 1;
}

// ----------------------------------------
// Begin Main function

void MainProcess(unsigned char ucCmdUsed)
{
	PCHARINFO  pChData     = (PCHARINFO)pCharData;
	PSPAWNINFO pChSpawn    = (PSPAWNINFO)pCharSpawn;
	PSPAWNINFO pLPlayer    = (PSPAWNINFO)pLocalPlayer;
	PSPAWNINFO psTarget    = (PSPAWNINFO)(STICK->Hold ? GetSpawnByID(STICK->HoldID) : pTarget);
	PSPAWNINFO pCampPlayer = (PSPAWNINFO)GetSpawnByID(CURCAMP->PcID);

	// ------------------------------------------
	// handle null stick pointers
	if (ucCmdUsed == CMD_STICK)
	{
		// prevent sticking to a target that does not exist or stick id to target that has changed types
		if (STICK->On && (!psTarget || (STICK->Hold && STICK->HoldType != GetSpawnType(psTarget))))
		{
			EndPreviousCmd(true);
			sprintf(szMsg, "\ay%s\aw:: You are no longer sticking to anything.", MODULE_NAME);
			WriteLine(szMsg, V_STICKV);
			return;
		}
	}
	// end null stick pointers

	// handle /makecamp player if the player no longer exists or has died
	if (CURCAMP->Pc && (!pCampPlayer || CURCAMP->PcType != GetSpawnType(pCampPlayer)))
	{
		sprintf(szMsg, "\ay%s\aw:: MakeCamp player ended due to player leaving/death.", MODULE_NAME);
		WriteLine(szMsg, V_MAKECAMPV);
		CAMP->ResetPlayer(false);
		return;
	}
	// end /makecamp player handling

	// handle null pointers for all commands
	if (!pChData || !pLPlayer || !pChSpawn->SpawnID || !GetCharInfo2())
	{
		sprintf(szMsg, "\ay%s\aw:: Null pointer, turning off current command", MODULE_NAME);
		WriteLine(szMsg, V_SILENCE);
		EndPreviousCmd(false);
		return;
	}
	// end null pointers
	// ------------------------------------------

	// setup used only in MainProcess
	float fNewHeading     = 0.0f; // heading changes used by all commands
	bool bMoveToOOR       = true;  // used by moveto, set to false if in range
	static bool sbJumping = false; // true if we executed a jump in stucklogic

	// variables reflecting character state
	float fSpeedMulti  = pChSpawn->SpeedMultiplier;
	bool  bSwimming    = pChSpawn->UnderWater                                      ? true : false; // used by stucklogic & uw
	bool  bMounted     = pChSpawn->Mount                                           ? true : false; // used by stucklogic
	bool  bStunned     = pChData->Stunned                                          ? true : false; // used by stucklogic and autopause
	bool  bLevitated   = (pChSpawn->mPlayerPhysicsClient.Levitate == 2)                                 ? true : false; // used by stucklogic
	bool  bRooted      = (fSpeedMulti == -10000.0f  || pChSpawn->RunSpeed < -0.4f) ? true : false; // we return without moving when rooted
	bool  bSnared      = (fSpeedMulti < 0.0f        || pChSpawn->RunSpeed < 0.0f)  ? true : false; // used by stucklogic
	bool  bInJump      = (pChSpawn->Animation == 19 || pChSpawn->Animation == 20)  ? true : false; // used by trytojump

	if (!bInJump)                              sbJumping = false;
	if (bSwimming && SET->AutoUW) STICK->UW = MOVETO->UW = true;
	bool bUseStuck = (!bRooted && !bSnared) ? true : false; // we set false elsewhere if stucklogic is not desired this pulse
	// end MainProcess setup

	// handle breakonsummon
	if (SET->BreakSummon && ucCmdUsed != CMD_MAKECAMP)
	{
		if (SUMMON->CurDist == 0.0f && SUMMON->Y == 0.0f && SUMMON->X == 0.0f)
		{
			// set last loc to current loc if this is first evaluation for current command
			// so that comparison is near-zero and below 'if' will be false
			SUMMON->Y = pChSpawn->Y;
			SUMMON->X = pChSpawn->X;
		}

		// get the distance moved
		SUMMON->CurDist = fabs(GetDistance(SUMMON->Y, SUMMON->X, pChSpawn->Y, pChSpawn->X));
		// store current location for next pulse
		SUMMON->Y = pChSpawn->Y;
		SUMMON->X = pChSpawn->X;

		// if distance moved is larger than user value, halt commands & lock plugin
		if (SUMMON->CurDist > SET->DistSummon)
		{
			sprintf(szMsg, "\ay%s\aw:: \arWARNING\ax Command ended from character summoned \ag%.2f\ax distance in a pulse.", MODULE_NAME, SUMMON->CurDist);
			EndPreviousCmd(true);
			WriteLine(szMsg, V_BREAKONSUMMON);
			sprintf(szMsg, "\ay%s\aw:: \arWARNING\ax Verify you are not being monitored and type \ag/stick imsafe\ax to allow command usage.", MODULE_NAME);
			WriteLine(szMsg, V_BREAKONSUMMON);
			pMU->BrokeSummon = PAUSE->PausedMU = true;
			SpewDebug(DBG_MAIN, "Summon Detection fired. All Commands Paused. SET->DistSummon(%.2f) - Moved(%.2f)", SET->DistSummon, SUMMON->CurDist);
			return;
		}
	}
	//end breakonsummon

	// handle autopause
	if (SET->AutoPause && ucCmdUsed != CMD_MAKECAMP)
	{
		static bool sbAPOutput = false;
		// convert to long because spellid is defined as unsigned but data can be negative to represent not casting
		if (((long)(pChSpawn->CastingData.SpellID) >= 0 && !pMU->Bard) || (pChSpawn->StandState != STANDSTATE_STAND && pChSpawn->StandState != STANDSTATE_DUCK) || bStunned || bRooted || (ucCmdUsed == CMD_STICK && ME->IsMe(psTarget)))
		{
			if ((uiVerbLevel & V_AUTOPAUSE) == V_AUTOPAUSE && !sbAPOutput)
			{
				sprintf(szMsg, "\ay%s\aw:: AutoPause halting movement...", MODULE_NAME);
				WriteLine(szMsg, V_AUTOPAUSE);
				sbAPOutput = true;
				MOVE->StopHeading();
			}
			STICK->TimeStop();
			CAMP->TimeStop(); // reset delay times
			MOVE->StopMove(APPLY_TO_ALL);
			return; // return until above is false
		}
		sbAPOutput = false;
	}
	// end autopause

	// assign needed stick values
	if (ucCmdUsed == CMD_STICK)
	{
		if (!STICK->SetDist)
		{
			STICK->Dist    = (psTarget->StandState ? get_melee_range(pLocalPlayer, (EQPlayer *)psTarget) : 15.0f) * STICK->DistModP + STICK->DistMod;
			STICK->SetDist = true;
		}

		// assign distance
		STICK->DifDist = STICK->CurDist;
		STICK->CurDist = fabs(GetDistance(pChSpawn, psTarget));

		static bool sbSelfOutput = false;
		// if we've changed targets mid-stick or this is the first pulse, dont trigger stucklogic or breakonwarp
		if (STICK->LastID != psTarget->SpawnID)
		{
			if (STICK->LastID && STICK->BreakTarget) // if we had set the ID
			{
				EndPreviousCmd(true);
				sprintf(szMsg, "\ay%s\aw:: \arStick broken from target change.", MODULE_NAME);
				WriteLine(szMsg, V_BREAKONWARP);
				return;
			}
			bUseStuck = false;
			STICK->DifDist = 0.0f;
			sbSelfOutput = false;
		}
		STICK->LastID = psTarget->SpawnID;

		// make sure havent switched to self - if so, stop movement
		if (ME->IsMe(psTarget))
		{
			if (!sbSelfOutput)
			{
				MOVE->StopMove(APPLY_TO_ALL);
				sprintf(szMsg, "\ay%s\aw:: Movement pausing due to self target...", MODULE_NAME);
				WriteLine(szMsg, V_AUTOPAUSE);
				sbSelfOutput = true;
			}
			return;
		}
		sbSelfOutput = false;

		// if this is not the first pass and target has not changed
		// handle pauseonwarp or breakonwarp
		if ((STICK->PauseWarp || STICK->BreakWarp) && STICK->DifDist != 0.0f)
		{
			static bool sbBWOutput = false;
			// if the distance between us and the desired stick distance is > user break distance
			// (we compare prevdist so that user can initially stick from distances
			//       larger than breakdist as long as the distance keeps decreasing)
			if (sbBWOutput)
			{
				if (STICK->CurDist - STICK->Dist > STICK->DistBreak) return;
				// return until back in range
			}
			else if (STICK->CurDist - STICK->DifDist - STICK->Dist > STICK->DistBreak)
			{
				if (STICK->PauseWarp)
				{
					if (!sbBWOutput)
					{
						MOVE->StopMove(APPLY_TO_ALL);
						sprintf(szMsg, "\ay%s\aw: Stick pausing until target back in BreakDist range...", MODULE_NAME);
						WriteLine(szMsg, V_BREAKONWARP);
						sbBWOutput = true;
						STICK->TimeStop();
						CAMP->TimeStop(); // reset delay times
					}
					// return until above is false
					return;
				}
				else
				{
					EndPreviousCmd(true);
					sprintf(szMsg, "\ay%s\aw:: Stick ending from target warping out of BreakDist range.", MODULE_NAME);
					WriteLine(szMsg, V_BREAKONWARP);
					return;
				}
			}
			sbBWOutput = false;
		}
		// end pauseonwarp/breakonwarp
	}
	// end stick values assignment

	// handle makecamp altreturn
	if (CAMP->DoAlt && !CAMP->Returning)
	{
		CAMP->TimeStop(); // reset delay time
		if (!CURCAMP->Scatter)
		{
			CampReturn(ALTCAMP->Y, ALTCAMP->X, ALTCAMP->Radius, &CAMP->Y, &CAMP->X);
		}
		else
		{
			PolarSpot(ALTCAMP->Y, ALTCAMP->X, 0.0f, CURCAMP->Bearing, CURCAMP->ScatDist, CURCAMP->ScatSize, &CAMP->Y, &CAMP->X);
		}
		MOVETO->On       = CAMP->Returning  = true;
		MOVETO->PreciseX = MOVETO->PreciseY = false;
		CAMP->DoReturn   = CAMP->Auto  = false;
		return;
	}
	// end altreturn

	// makecamp handling
	if (!CAMP->Returning && CURCAMP->On)
	{
		CAMP->CurDist = GetDistance(pChSpawn->Y, pChSpawn->X, CURCAMP->Pc ? pCampPlayer->Y : CURCAMP->Y, CURCAMP->Pc ? pCampPlayer->X : CURCAMP->X);
		CAMP->DifDist = GetDistance(STICK->On ? psTarget->Y : MOVETO->Y, STICK->On ? psTarget->X : MOVETO->X, CURCAMP->Pc ? pCampPlayer->Y : CURCAMP->Y, CURCAMP->Pc ? pCampPlayer->X : CURCAMP->X);

		// break from command if it would exceed active leash
		if (CURCAMP->Leash && (STICK->On || MOVETO->On))
		{
			if (CAMP->CurDist < CAMP->DifDist && CAMP->DifDist > CURCAMP->Length)
			{
				EndPreviousCmd(true);
				sprintf(szMsg, "\ay%s\aw:: Outside of leash length, breaking from current command", MODULE_NAME);
				WriteLine(szMsg, V_MAKECAMPV);
				return;
			}
		}
		// end leash new command check

		// if makecamp return issued, or if makecamp on check to see if we need to move back
		if (!PAUSE->UserKB && !PAUSE->UserMouse && !STICK->On && !MOVETO->On && !CIRCLE->On)
		{
			// normal return
			if (CAMP->CurDist > CURCAMP->Radius + 2.0f || CAMP->DoReturn) // give leeway to avoid teetering
			{
				bool bDoReturn = true;
				if (!CAMP->DoReturn)
				{
					if (CURCAMP->NoAggro     && ME->InCombat()) bDoReturn = false;
					if (!CURCAMP->HaveTarget && pTarget)        bDoReturn = false;
					if (CURCAMP->NotLoot     && pActiveCorpse)  bDoReturn = false;
				}

				// check this logic - if the timer starts and then doreturn turns false, the conditions might not reset
				// and the return will happen instantly once doreturn is true again
				//
				// instead it SHOULD RESTART THE TIMER from the point the conditions are ready

				// processed conditions in which not to return, if none are met, begin returning
				if (bDoReturn)
				{
					char cResult = CAMP->TimeStatus();
					if (CAMP->DoReturn || cResult == T_READY)
					{
						CAMP->TimeStop();
						if (!CURCAMP->Scatter)
						{
							CampReturn(CURCAMP->Pc ? pCampPlayer->Y : CURCAMP->Y, CURCAMP->Pc ? pCampPlayer->X : CURCAMP->X, CURCAMP->Radius, &CAMP->Y, &CAMP->X);
						}
						else
						{
							PolarSpot(CURCAMP->Pc ? pCampPlayer->Y : CURCAMP->Y, CURCAMP->Pc ? pCampPlayer->X : CURCAMP->X, 0.0f, CURCAMP->Bearing, CURCAMP->ScatDist, CURCAMP->ScatSize, &CAMP->Y, &CAMP->X);
						}
						MOVETO->On       = CAMP->Returning  = true;
						MOVETO->PreciseX = MOVETO->PreciseY = false;
						CAMP->DoAlt                         = false;
						if (!CAMP->DoReturn) CAMP->Auto     = true; // so the output msg isnt displayed unless user/macro issued command and used in pause
					}
					else if (cResult == T_INACTIVE)
					{
						CAMP->Auto = false;
						CAMP->TimeStart();
						return; // return here to begin waiting for return time
					}
				}
			}
			// end normal return
		}
		// end auto return checking

		// begin leash processing with active stick/circle
		if (!PAUSE->UserKB && !PAUSE->UserMouse && CURCAMP->Leash && (STICK->On || CIRCLE->On || CURCAMP->RedoStick || CURCAMP->RedoCircle))
		{
			CMULoc HeadBack;
			HeadBack.Y = CURCAMP->Pc ? pCampPlayer->Y : CURCAMP->Y;
			HeadBack.X = CURCAMP->Pc ? pCampPlayer->X : CURCAMP->X;

			if (CAMP->CurDist > CURCAMP->Length + 2.0f) // give leeway if teetering
			{
				if (STICK->On || CIRCLE->On)
				{
					if (STICK->On)
					{
						EndPreviousCmd(true, CMD_STICK, true);
						STICK->On         = false; // disable stick but don't reset current cmd settings
						STICK->BehindOnce = STICK->Snaproll = false; // disable these as well since locations will no longer be accurate
						CURCAMP->RedoStick                = true;
					}
					else
					{
						EndPreviousCmd(true, CMD_CIRCLE, true);
						CIRCLE->On            = false; // disable circling but don't reset current cmd settings
						CURCAMP->RedoCircle = true;
					}
				}
				if (psTarget && (CURCAMP->RedoStick || CURCAMP->RedoCircle))
				{
					fNewHeading = MOVE->SaneHead((atan2(HeadBack.Y - pChSpawn->Y, HeadBack.X - pChSpawn->X) * HEADING_HALF / (float)PI));
					MOVE->TryMove(GO_FORWARD, MU_WALKOFF, fNewHeading, HeadBack.Y, HeadBack.X);
				}
			}
			else if (CURCAMP->RedoStick || CURCAMP->RedoCircle)
			{
				EndPreviousCmd(false, (CURCAMP->RedoStick ? CMD_STICK : CMD_CIRCLE), true);
				CURCAMP->RedoStick ? STICK->TurnOn() : CIRCLE->On = true;
				CURCAMP->RedoStick = CURCAMP->RedoCircle = false;
				return;
			}
		}
	}
	// end return to camp handling
	if (ucCmdUsed == CMD_MAKECAMP) return; // nothing below applies makecamp, return turns bMoveTo on which calls with CMD_MOVETO

	// reset use of /face command
	gFaceAngle = H_INACTIVE;

	// assign values for circle
	if (ucCmdUsed == CMD_CIRCLE)
	{
		float fUseCirYX[2] = {(pChSpawn->Y - CIRCLE->Y), (pChSpawn->X - CIRCLE->X)};
		CIRCLE->CurDist = sqrt(fUseCirYX[0] * fUseCirYX[0] + fUseCirYX[1] * fUseCirYX[1]);
		if (CIRCLE->CurDist < CIRCLE->Radius * (2.0f / 3.0f)) bUseStuck = false;
	}
	// end circle assignments

	if (ucCmdUsed == CMD_MOVETO)
	{
		if (MOVETO->PreciseY)
		{
			MOVETO->CurDist = fabs(GetDistance(pChSpawn->Y, 0.0f, MOVETO->Y, 0.0f));
			MOVETO->DifDist = MOVETO->DistY;
		}
		else if (MOVETO->PreciseX)
		{
			MOVETO->CurDist = fabs(GetDistance(0.0f, pChSpawn->X, 0.0f, MOVETO->X));
			MOVETO->DifDist = MOVETO->DistX;
		}
		else
		{
			if (CAMP->Returning)
			{
				// if camp player return is active, keep moveto location updated real time
				if (CURCAMP->Pc && CURCAMP->Realtime)
				{
					if (!CURCAMP->Scatter)
					{
						CampReturn(pCampPlayer->Y, pCampPlayer->X, CURCAMP->Radius, &CAMP->Y, &CAMP->X);
					}
					else
					{
						PolarSpot(pCampPlayer->Y, pCampPlayer->X, 0.0f, CURCAMP->Bearing, CURCAMP->ScatDist, CURCAMP->ScatSize, &CAMP->Y, &CAMP->X);
					}
				}
				MOVETO->CurDist = fabs(GetDistance(pChSpawn->Y, pChSpawn->X, CAMP->Y, CAMP->X));
			}
			else if (MOVETO->Z == 0.0f)
			{
				MOVETO->CurDist = fabs(GetDistance(pChSpawn->Y, pChSpawn->X, MOVETO->Y, MOVETO->X));
			}
			else
			{
				MOVETO->CurDist = fabs(GetDistance3D(pChSpawn, MOVETO));
			}
			MOVETO->DifDist = MOVETO->Dist;
		}

		// check for stucklogic and MoveToAggro
		if (MOVETO->CurDist <= MOVETO->DifDist)
		{
			if (MOVETO->UsingPath && MOVETO->currentPathCursor < MOVETO->currentPathSize) {
				MOVETO->currentPathCursor++;
				MOVETO->X = MOVETO->currentPath[MOVETO->currentPathCursor*3];
				MOVETO->Y = MOVETO->currentPath[MOVETO->currentPathCursor*3+2];
				MOVETO->Z = 0.0f;
				//sprintf(szMsg, "\ay%s\aw:: Next loc %.2f %.2f navMesh: (%d/%d)", MODULE_NAME, MOVETO->Y, MOVETO->X, MOVETO->currentPathCursor, MOVETO->currentPathSize);
				//WriteLine(szMsg, V_MOVETOV);
				if (MOVETO->currentPathCursor >= MOVETO->currentPathSize) {
					MOVETO->Walk = true;
				    MOVETO->Dist = 10.0f;
				}
			} else
				MOVETO->UsingPath = bMoveToOOR = bUseStuck = false;
		}
		else
		{
			bMoveToOOR = true;
			if (MOVETO->BreakAggro && MOVETO->DidAggro()) return;
		}
	}

	// BEGIN STUCKLOGIC
	if (STUCK->On)
	{
		// use 3D to compare Z so stucklogic doesn't fire if we are moving more z than y/x (up/down slopes)
		// if bJumping then dont check z axis movement
		STUCK->DifDist = (bLevitated || (sbJumping && bInJump)) ? GetDistance(pChSpawn->Y, pChSpawn->X, STUCK->Y, STUCK->X) : GetDistance3D(pChSpawn, STUCK);

		// sanity check, if we've moved more than 5 (summon, knockback, user)
		// it will throw off our readjustment, so keep the last value instead
		if (STUCK->DifDist < 5.0f) STUCK->CurDist = MovingAvg(STUCK->DifDist, STUCK->Check);
		//SpewDebug(DBG_STUCK, "STUCK->CurDist = %.2f, fPulseMoved %.2f", STUCK->CurDist, fPulseMoved);

		STUCK->Y = pChSpawn->Y;
		STUCK->X = pChSpawn->X;
		STUCK->Z = pChSpawn->Z;

		//SpewDebug(DBG_DISABLE, "runspeed %.2f and walkspeed %.2f and speedrun %.2f and speedmultiplier %.2f", pChSpawn->RunSpeed, pChSpawn->WalkSpeed, pChSpawn->SpeedRun, pChSpawn->SpeedMultiplier);
		if (bSnared || bRooted)
		{
			// dont use stucklogic if snared
			bUseStuck = false;
			STUCK->StuckInc = 0;
			STUCK->StuckDec = 0;
		}
		else if (fSpeedMulti < 0.25f && *EQADDR_RUNWALKSTATE)
		{
			fSpeedMulti = 0.25f; // boost multiplier of those without runspeed if not walking
		}
		else if (!*EQADDR_RUNWALKSTATE)
		{
			fSpeedMulti = 0.1f; // boost multiplier of those walking
		}

		// if we were stuck last pulse and we moved more than stuckdist since (making progress to get unstuck)
		if (STUCK->StuckInc && STUCK->CurDist > STUCK->Dist)
		{
			STUCK->StuckInc--;
			//SpewDebug(DBG_STUCK, "Decremented STUCK->StuckInc to (%d) because we moved (%3.2f)", STUCK->StuckInc, STUCK->CurDist);

			//force unstuck after defined increments of not being stuck. we reset STUCK->StuckDec if we get stuck again
			STUCK->StuckDec++;
			if (STUCK->StuckDec > STUCK->Unstuck)
			{
				STUCK->StuckInc = 0;
				STUCK->StuckDec = 0;
				//SpewDebug(DBG_STUCK, "Zeroed STUCK->StuckInc and STUCK->StuckDec after consecutive decrements");
			}
		}

		if (bStunned || MOVE->ChangeHead != H_INACTIVE) bUseStuck = false; // dont analyze if stunned or plugin is adjusting heading
		//SpewDebug(DBG_STUCK, "if STUCK->CurDist(%.2f) < STUCK->Dist(%.2f) * fSpeedMulti(%.2f) && bUse(%s) && Fwd(%s) or Side(%s)", STUCK->CurDist, STUCK->Dist, fSpeedMulti, bUseStuck ? "true" : "false", pMU->CmdFwd ? "true" : "false", pMU->CmdStrafe ? "true" : "false");

		if ((pMU->CmdFwd || pMU->CmdStrafe) && bUseStuck &&
			// if moved forward or strafe (not backwards)
			// bUseStuck is set false if using stucklogic this pulse is not desired

			// main stucklogic formula here
			( (STUCK->CurDist < STUCK->Dist * fSpeedMulti && !bSwimming && !bMounted) ||

			// maybe handle water and mounts on their own system?
			(bSwimming && (double)STUCK->CurDist < 0.0010) ||

			(bMounted && STUCK->CurDist < (STUCK->Dist + fSpeedMulti) / 3.0f) )

			)
		{
			//SpewDebug(DBG_STUCK, "Im Stuck (STUCK->StuckInc = %d) -- if fpulsemoved (%.2f) < STUCK->Dist(%.2f) * fSpeedMultiplier(%.2f) then increment", STUCK->StuckInc, fPulseMoved, STUCK->Dist, fSpeedMulti);

			// 'big if' is true, if we've moved less than user-set dist * current speed multiplier
			if (STUCK->DifDist < STUCK->Dist * fSpeedMulti) // check if our movement this single pulse is still bad
			{
				STUCK->StuckInc++;
				STUCK->StuckDec = 0;
				//SpewDebug(DBG_STUCK, "incremented STUCK->StuckInc %d, reset STUCK->StuckDec to zero", STUCK->StuckInc);

				// STUCK->Jump user set value in INI
				// try to jump early (but not 1-2pulse misfires) and again after a few seconds/turns
				if (STUCK->Jump && !sbJumping && !bLevitated && !bSwimming && ((STUCK->StuckInc % 5) == 0))
				{
					MQ2Globals::ExecuteCmd(iJumpKey, 1, 0);
					MQ2Globals::ExecuteCmd(iJumpKey, 0, 0);
					sbJumping = true;
				}

				// calculate original heading for turnhalf before first turn attempt
				float fOrigHead, fHalfHead, fCompNegHead, fCompPosHead = 0.0f;
				if (STUCK->StuckInc == 4)
				{
					switch (ucCmdUsed)
					{
					case CMD_STICK:
						fOrigHead = MOVE->SaneHead((atan2(psTarget->X - pChSpawn->X, psTarget->Y - pChSpawn->Y) * HEADING_HALF) / (float)PI);
						break;
					case CMD_MOVETO:
						if (CAMP->Returning)
						{
							fOrigHead = MOVE->SaneHead((atan2(CAMP->X - pChSpawn->X, CAMP->Y - pChSpawn->Y) * HEADING_HALF) / (float)PI);
							break;
						}
						fOrigHead = MOVE->SaneHead((atan2(MOVETO->X - pChSpawn->X, MOVETO->Y - pChSpawn->Y) * HEADING_HALF) / (float)PI);
						break;
					case CMD_CIRCLE:
						fOrigHead = (atan2(pChSpawn->Y - CIRCLE->Y, CIRCLE->X - pChSpawn->X) * CIRCLE_HALF) / (float)PI * CIRCLE_QUARTER;
						fOrigHead += CIRCLE_QUARTER * (CIRCLE->Radius / CIRCLE->CurDist);
						fOrigHead = MOVE->SaneHead(fOrigHead *= HEADING_MAX / CIRCLE_MAX);
					}

					fHalfHead = MOVE->SaneHead(fOrigHead + HEADING_HALF);
					//SpewDebug(DBG_STUCK, "We've determined that halfway from destination is %.2f", fHalfHead);
					fCompNegHead = MOVE->SaneHead(fHalfHead - fabs(STUCK->TurnSize / 2.0f));
					fCompPosHead = MOVE->SaneHead(fHalfHead + fabs(STUCK->TurnSize / 2.0f));
				}

				// if STUCK->StuckInc == multiple of 4 (try to turn 1 increment, every 4 pulses of being stuck)
				if ((STUCK->StuckInc & 3) == 0)
				{
					fNewHeading = MOVE->SaneHead(pChSpawn->Heading + STUCK->TurnSize);
					//SpewDebug(DBG_STUCK, "Stucklogic turned, New heading is %.2f", fNewHeading);

					// if enabled, check if we are halfway away from our destination, reset heading to original heading and start again
					if (STUCK->TurnHalf)
					{
						//SpewDebug(DBG_STUCK, "TURNHALF: Comparing desired heading (%.2f) > %.2f and < %.2f", fNewHeading, fCompNegHead, fCompPosHead);
						if (fNewHeading > fCompNegHead && fNewHeading < fCompPosHead)
						{
							fNewHeading = fOrigHead;
							STUCK->TurnSize *= -1.0f;
							//SpewDebug(DBG_STUCK, "TRUE, so flipped STUCK->TurnSize to other way (%.2f) and reset heading to %.2f", STUCK->TurnSize, fNewHeading);
						}
					}
					MOVE->NewHead(fNewHeading);
					//SpewDebug(DBG_STUCK, "Stucklogic heading change: %.2f", fNewHeading);
				}
			}
			// end fPulseMoved < STUCK->Dist * speedmulti
		}
		// end 'big if'
	}
	// end if STUCK->On (meaning use stucklogic or not)

	// if we are stuck or rooted dont process normal heading & movement
	if (STUCK->StuckInc || bRooted)
	{
		//SpewDebug(DBG_STUCK, "STUCK->StuckInc %d bRooted %s fired, returning without trying to move", STUCK->StuckInc, bRooted ? "true" : "false");
		return;
	}
	// END OF STUCKLOGIC


	// handle heading and movement
	switch (ucCmdUsed)
	{
	case CMD_STICK:
		if (!STICK->Snaproll) fNewHeading = MOVE->SaneHead(atan2(psTarget->X - pChSpawn->X, psTarget->Y - pChSpawn->Y) * HEADING_HALF / (float)PI);
		// jump ahead to stick handling
		break;
	case CMD_CIRCLE:
		fNewHeading = (!CIRCLE->CCW != CIRCLE->Backward) ? (atan2(pChSpawn->Y - CIRCLE->Y, CIRCLE->X - pChSpawn->X) * CIRCLE_HALF) / (float)PI : (atan2(CIRCLE->Y - pChSpawn->Y, pChSpawn->X - CIRCLE->X) * CIRCLE_HALF) / (float)PI;
		CIRCLE->CCW ?  fNewHeading -= CIRCLE_QUARTER + CIRCLE_QUARTER * (CIRCLE->Radius / CIRCLE->CurDist) : fNewHeading += CIRCLE_QUARTER + CIRCLE_QUARTER * (CIRCLE->Radius / CIRCLE->CurDist);
		MOVE->NewHead(MOVE->SaneHead(fNewHeading *= HEADING_MAX / CIRCLE_MAX));
		CIRCLE->Backward ? MOVE->DoMove(GO_BACKWARD) : MOVE->DoMove(GO_FORWARD);
		// end of all circle processing
		return;
	case CMD_MOVETO:
		if (bMoveToOOR)
		{
			if (psTarget && MOVETO->UW)
			{
				double dLookAngle = (double)atan2(psTarget->Z + psTarget->AvatarHeight * StateHeightMultiplier(psTarget->StandState) -
					pChSpawn->Z - pChSpawn->AvatarHeight * StateHeightMultiplier(pChSpawn->StandState), fabs(GetDistance3D(pChSpawn, psTarget))) * HEADING_HALF / (double)PI;
				MOVE->NewFace(dLookAngle);
			}
			if (CAMP->Returning)
			{
				fNewHeading = MOVE->SaneHead(atan2(CAMP->X - pChSpawn->X, CAMP->Y - pChSpawn->Y) * HEADING_HALF / (float)PI);
				if (MOVETO->UseBack)
				{
					if (MOVE->ChangeHead == H_INACTIVE && MOVETO->CurDist < MOVETO->DistBack)
					{
						float fHeadDiff = MOVE->SaneHead(pChSpawn->Heading - fNewHeading);
						if (fHeadDiff >= 200.0f && fHeadDiff <= 300.0f)
						{
							MOVE->DoMove(GO_BACKWARD, true, MOVETO->CurDist < 20.0f ? MU_WALKON : MU_WALKOFF);
							return;
						}
					}
				}
				MOVE->NewHead(fNewHeading);
				MOVE->TryMove(GO_FORWARD, (MOVETO->Walk && MOVETO->CurDist < 20.0f) ? MU_WALKON : MU_WALKOFF, fNewHeading, CAMP->Y, CAMP->X);
				return;
			}
			fNewHeading = MOVE->SaneHead(atan2(MOVETO->X - pChSpawn->X, MOVETO->Y - pChSpawn->Y) * HEADING_HALF / (float)PI);
			if (MOVETO->UseBack)
			{
				if (MOVE->ChangeHead == H_INACTIVE && MOVETO->CurDist < MOVETO->DistBack)
				{
					float fHeadDiff = MOVE->SaneHead(pChSpawn->Heading - fNewHeading);
					if (fHeadDiff >= 200.0f && fHeadDiff <= 300.0f)
					{
						MOVE->DoMove(GO_BACKWARD, true, MOVETO->CurDist < 20.0f ? MU_WALKON : MU_WALKOFF);
						return;
					}
				}
			}
			MOVE->NewHead(fNewHeading);
			MOVE->TryMove(GO_FORWARD, (MOVETO->Walk && MOVETO->CurDist < 20.0f) ? MU_WALKON : MU_WALKOFF, fNewHeading, MOVETO->Y, MOVETO->X);
			return;
		}
		if (!CAMP->Auto) // we do not want output for auto-returns
		{
			if (CAMP->DoReturn)
			{
				sprintf(szMsg, "\ay%s\aw:: Arrived at %s", MODULE_NAME, szArriveCamp);
			}
			else if (CAMP->DoAlt)
			{
				sprintf(szMsg, "\ay%s\aw:: Arrived at %s", MODULE_NAME, szArriveAlt);
			}
			else
			{
				sprintf(szMsg, "\ay%s\aw:: Arrived at %s", MODULE_NAME, szArriveMove);
				pMU->StoppedMoveto = true;
			}
			WriteLine(szMsg, V_MOVETOV);
		}
		CAMP->Auto = CAMP->DoAlt = CAMP->DoReturn = false;
		EndPreviousCmd(true);
		// end of all moveto processing
		return;
	default:
		return;
	}

	// -----------------------------------------------------
	// everything below this point relates to '/stick' ONLY
	// -----------------------------------------------------

	// diff between cur heading vs. desired heading
	float fHeadDiff = MOVE->SaneHead(pChSpawn->Heading - fNewHeading);

	// if we are close to the mob and our desired heading change is large, move backwards instead
	if (STICK->UseBack && !STICK->Snaproll && !STICK->Healer && MOVE->ChangeHead == H_INACTIVE && STICK->CurDist < STICK->Dist + STICK->DistBack)
	{
		// we check the MOVE->ChangeHead so not to affect turns already in motion
		if (fHeadDiff >= 200.0f && fHeadDiff <= 300.0f)
		{
			MOVE->DoMove(GO_BACKWARD);
			return;
		}
	}

	// what is clarity?
	bool bTurnHealer = (!STICK->Healer ? true : (STICK->CurDist > STICK->Dist + 10.0f ? true : false));
	if (bTurnHealer)
	{
		MOVE->NewHead(fNewHeading); // adjust heading for all cases except stick healer being oor

		if (STICK->UW) // adjust look angle if underwater param or autouw
		{
			double dLookAngle = (double)atan2(psTarget->Z + psTarget->AvatarHeight * StateHeightMultiplier(psTarget->StandState) -
				pChSpawn->Z - pChSpawn->AvatarHeight * StateHeightMultiplier(pChSpawn->StandState), STICK->CurDist) * HEADING_HALF / (double)PI;
			MOVE->NewFace(dLookAngle);
		}
	}

	// if stucklogic is on and we are ducking while sticking, un-duck
	if (STUCK->On && pChSpawn->StandState == STANDSTATE_DUCK) MOVE->DoStand();

	// move FORWARD ONLY until near stick range (except snaproll)
	if (STICK->CurDist > STICK->Dist + 10.0f && !STICK->Snaproll)
	{
		MOVE->StopMove(KILL_STRAFE);
		MOVE->TryMove(GO_FORWARD, (STICK->CurDist > STICK->Dist + 20.0f) ? MU_WALKOFF : (STICK->Walk ? MU_WALKON : MU_WALKIGNORE), fNewHeading, psTarget->Y, psTarget->X);
		return;
	}
	// everything below: only if near stick range

	// calculate our angular distance
	float fAngDist     = MOVE->AngDist(psTarget->Heading, pChSpawn->Heading);
	float fFabsAngDist = fabs(fAngDist);
	bool b3xRange   = (STICK->CurDist <  STICK->Dist * 3) ? true : false;
	bool bAnyStrafe = (STICK->Strafe  || STICK->Front)    ? true : false;

	// if too far away, don't do pin or behind or aggro checking (if awareness on)
	if (bAnyStrafe && b3xRange)
	{
		// handling for behind/pin/!front
		if (STICK->Strafe)
		{
			// halt strafing if we are on HoTT
			if (pMU->CmdStrafe && (int)pLPlayer->SpawnID == pChSpawn->TargetOfTarget)
			{
				MOVE->StopMove(KILL_STRAFE);
			}
			// we are not on HoTT
			else
			{
				// if randomize is enabled and using /stick behind or /stick !front, but not /stick pin
				if (STICK->Randomize && !STICK->BehindOnce)
				{
					bool bPosFlag = (rand() % 100 > 50);
					if (!STICK->Pin)
					{
						// our randomized "flag" is negative or positive sign - to determine which arc was modified
						// we make this eval multiple times, so set it to a bool first
						if (fAngDist < 0.0f && fFabsAngDist > (bPosFlag ? STICK->RandMax : STICK->RandMin))
						{
							MOVE->StickStrafe(GO_LEFT);
						}
						else if (fAngDist > 0.0f && fFabsAngDist > (bPosFlag ? STICK->RandMin : STICK->RandMax))
						{
							MOVE->StickStrafe(GO_RIGHT);
						}
						// else we are within the arc range, so stop strafing
						else
						{
							MOVE->StopMove(KILL_STRAFE);
						}
					}
					else
					{
						if (STICK->UseFleeing && IsMobFleeing(pChSpawn, psTarget) && (psTarget->HPCurrent * 100 / psTarget->HPMax) < 25)
						{
							MOVE->StopMove(KILL_STRAFE);
						}
						else if (fFabsAngDist > PIN_ARC_MAX || fFabsAngDist < PIN_ARC_MIN)
						{
							if ((fAngDist < 0.0f && fAngDist > (bPosFlag ? (PIN_ARC_MIN * -1.0f) : (STICK->RandMin * -1.0f))) || fAngDist > (bPosFlag ? STICK->RandMax : PIN_ARC_MAX))
							{
								MOVE->StickStrafe(GO_RIGHT);
							}
							if ((fAngDist > 0.0f && fAngDist < (bPosFlag ? PIN_ARC_MIN : STICK->RandMin)) || fAngDist < (bPosFlag ? (PIN_ARC_MAX * -1.0f) : (STICK->RandMax * -1.0f)))
							{
								MOVE->StickStrafe(GO_LEFT);
							}
						}
						else
						{
							MOVE->StopMove(KILL_STRAFE);
						}
					}
				}
				// else randomize is not on, or /stick behindonce
				else
				{
					// normal processing for 'behind' and 'behindonce'
					if (STICK->Behind || STICK->BehindOnce)
					{
						if (fFabsAngDist > STICK->ArcBehind)
						{
							if (fAngDist < 0.0f)
							{
								MOVE->StickStrafe(GO_LEFT);
							}
							else
							{
								MOVE->StickStrafe(GO_RIGHT);
							}
						}
						else
						{
							STICK->BehindOnce = false;
							MOVE->StopMove(KILL_STRAFE);
						}
					}
					// processing for '/stick pin'
					else if (STICK->Pin)
					{
						if (STICK->UseFleeing && IsMobFleeing(pChSpawn, psTarget) && (psTarget->HPCurrent * 100 / psTarget->HPMax) < 25)
						{
							MOVE->StopMove(KILL_STRAFE);
						}
						else if (fFabsAngDist > PIN_ARC_MAX || fFabsAngDist < PIN_ARC_MIN)
						{
							if ((fAngDist < 0.0f && fAngDist > (PIN_ARC_MIN * -1.0f)) || fAngDist > PIN_ARC_MAX)
							{
								MOVE->StickStrafe(GO_RIGHT);
							}
							if ((fAngDist > 0.0f && fAngDist < PIN_ARC_MIN) || fAngDist < (PIN_ARC_MAX * -1.0f))
							{
								MOVE->StickStrafe(GO_LEFT);
							}
						}
						else
						{
							MOVE->StopMove(KILL_STRAFE);
						}
					}
					// else non-random processing for stick !front
					else if (STICK->NotFront)
					{
						if (fFabsAngDist > STICK->ArcNotFront)
						{
							int iRandT = rand() % 100;
							if (iRandT > 85)
							{
								STICK->BehindOnce = true;
							}
							else if (fAngDist < 0.0f)
							{
								MOVE->StickStrafe(GO_LEFT);
							}
							else
							{
								MOVE->StickStrafe(GO_RIGHT);
							}
						}
						else
						{
							MOVE->StopMove(KILL_STRAFE);
						}
					}
				}
			}
		}
		else if (STICK->Front)
		{
			if (SET->Spin || (int)pLPlayer->SpawnID == pChSpawn->TargetOfTarget)
			{
				// if im the target of target or deliberately want to spin if lose aggro
				if (STICK->UseFleeing && IsMobFleeing(pChSpawn, psTarget) && (psTarget->HPCurrent * 100 / psTarget->HPMax) < 25)
				{
					MOVE->StopMove(KILL_STRAFE);
				}
				else if (fFabsAngDist < FRONT_ARC)
				{
					if (fAngDist > 0.0f)
					{
						MOVE->StickStrafe(GO_LEFT);
					}
					else
					{
						MOVE->StickStrafe(GO_RIGHT);
					}
				}
				else
				{
					MOVE->StopMove(KILL_STRAFE);
				}
			}
			else
			{
				MOVE->StopMove(KILL_STRAFE);
			}
		}
	}
	else
	{
		MOVE->StopMove(KILL_STRAFE);
	}
	//end pin/!front/behind/front

	// handling for stick snaproll
	if (STICK->Snaproll)
	{
		PolarSpot(psTarget->Y, psTarget->X, psTarget->Heading, STICK->Snap->Bearing, STICK->DistSnap, 0.0f, &STICK->Snap->Y, &STICK->Snap->X); // calculate location we want
		STICK->Snap->CurDist = fabs(GetDistance(pChSpawn->Y, pChSpawn->X, STICK->Snap->Y, STICK->Snap->X)); // get distance from that location

		// 3D problems when levitated
		//STICK->Snap->CurDist = fabs(GetDistance3D(pChSpawn->Y, pChSpawn->X, pChSpawn->Z, STICK->Snap->Y, STICK->Snap->X, psTarget->Z)); // get distance from that location

		if (STICK->Snap->CurDist <= 5.0f) // if distance is close enough on first pass, no need to snaproll, this also breaks in future pulses
		{
			MOVE->StopMove(APPLY_TO_ALL);
			STICK->Snaproll      = false;
			STICK->On = true;
			MOVE->NewHead(MOVE->SaneHead(atan2(psTarget->X - pChSpawn->X, psTarget->Y - pChSpawn->Y) * HEADING_HALF / (float)PI));
			STICK->NewSnaproll();
			return;
		}
		// determine heading to that location
		STICK->Snap->Head = MOVE->SaneHead((atan2(STICK->Snap->X - pChSpawn->X, STICK->Snap->Y - pChSpawn->Y) * HEADING_HALF) / (float)PI);
		MOVE->NewHead(STICK->Snap->Head);
		// start movement
		if (STICK->Snap->CurDist > 5.0f)
		{
			MOVE->TryMove(GO_FORWARD, MU_WALKOFF, STICK->Snap->Head, STICK->Snap->Y, STICK->Snap->X);
		}
		return;
	}
	// end snaproll handling

	// ******** main movement handling for stick *******
	// if we are outside stick distance
	if (STICK->CurDist > STICK->Dist)
	{
		MOVE->TryMove(GO_FORWARD, (STICK->CurDist < STICK->Dist + 20.0f) ? (STICK->Walk ? MU_WALKON : MU_WALKIGNORE) : MU_WALKIGNORE, fNewHeading, psTarget->Y, psTarget->X);
		return;
	}
	// else do moveback handling
	if (STICK->MoveBack)
	{
		// if not stick healer, stop movement if we are not within one turn of heading
		if (!STICK->Healer && fabs(pChSpawn->Heading - fNewHeading) > 14.0f)
		{
			MOVE->StopMove(KILL_FB);
			return;
		}
		// moveback not supported unless dist set to more than 5
		float fClose = STICK->Dist > 5.0f ? STICK->Dist - 5.0f : 0.0f;
		// if we are closer than 5.0 from our desired stick distance
		if (STICK->CurDist < fClose)
		{
			MOVE->Walk(GO_BACKWARD);
			return;
		}
		MOVE->StopMove(KILL_FB);
		return;
	}
	// else we are within desired distance, stop forward/backward movement
	MOVE->StopMove(KILL_FB);
}

// End Main function
// ----------------------------------------
// Begin Window Output

void WriteLine(char szOutput[MAX_STRING], VERBLEVEL V_COMPARE)
{
	// never write out if total silence is set
	if (uiVerbLevel == V_SILENCE) return;

	// if we passed totalsilence (0) as type (always output except when totalsilence on, former VERB_ENFORCE)  OR
	// set to display all output OR
	// flag for this msg output is on
	if ((V_COMPARE == V_SILENCE) || (uiVerbLevel & V_EVERYTHING) == V_EVERYTHING || (uiVerbLevel & V_COMPARE) == V_COMPARE)
	{
		WriteChatf(szOutput);
	}
}

void OutputHelp(unsigned char ucCmdUsed, bool bOnlyCmdHelp)
{
	// to support hidehelp, compare uiVerbLevel before calling

	if (!ValidIngame(false)) return;

	char szCommand[20]    = {0};
	bool bDisplaySettings = false;

	switch (ucCmdUsed)
	{
	case CMD_MAKECAMP:
		sprintf(szCommand, "/makecamp");
		break;
	case CMD_STICK:
		sprintf(szCommand, "/stick");
		break;
	case CMD_MOVETO:
		sprintf(szCommand, "/moveto");
		break;
	case CMD_CIRCLE:
		sprintf(szCommand, "/circle");
		break;
	case HELP_SETTINGS:
		bDisplaySettings = true;
		break;
	}

	char szTempOut[MAX_STRING] = {0};
	sprintf(szTempOut, "\ay%s \agv%1.4f", MODULE_NAME, MODULE_VERSION);
	WriteLine(szTempOut, V_SILENCE);

	if (bDisplaySettings)
	{
		WriteLine("\arThe following settings can be changed with any of the 4 commands. There are three options (using /stick as an example).", V_SILENCE);
		WriteLine("\ay/stick [set] [\agsetting\ax] [on | off]\ax - This will turn the setting on or off", V_SILENCE);
		WriteLine("\ay/stick [set] [\agsetting\ax] [#]\ax - This will change settings that use numerical values", V_SILENCE);
		WriteLine("\ay/stick [toggle] [\agsetting\ax]\ax - This will toggle the setting on and off.", V_SILENCE);
		WriteLine("\arThe following settings can be set on/off OR toggled:", V_SILENCE);
		WriteLine("\ag[autosave]\ax - Automatic saving of configuration file each time you change a setting.", V_SILENCE);
		WriteLine("\ag[feign]\ax - Stay-FD support, awaiting you to manually stand before resuming command.", V_SILENCE);
		WriteLine("\ag[breakonkb|breakonmouse]\ax - End command from manual keyboard/mouse movement.", V_SILENCE);
		WriteLine("\ag[mpause|mousepause]\ax - Pause from manual keyboard/mouse movement. Resumes after pausemindelay/pausemaxdelay values.", V_SILENCE);
		WriteLine("\ag[autopause]\ax - Automatic pause&resume command when casting(non-bard), ducking, stunned, targeting self.", V_SILENCE);
		WriteLine("\ag[verbosity|fullverbosity]\ax - MQ2ChatWnd command output and detailed ouput.", V_SILENCE);
		WriteLine("\ag[hidehelp]\ax - Hides help output from showing automatically on command failures.", V_SILENCE);
		WriteLine("\ag[totalsilence]\ax - Hide all MQ2ChatWnd output except critial warnings and requested information.", V_SILENCE);
		WriteLine("\ag[stucklogic]\ax - Automatic detection of stuck movement and attempted correction.", V_SILENCE);
		WriteLine("\ag[trytojump]\ax - Try to jump as part of stucklogic getting over obstacles.", V_SILENCE);
		WriteLine("\ag[turnhalf]\ax - Reset heading and try the other way if halfway from destination in stucklogic.", V_SILENCE);
		WriteLine("\ag[nohottfront]\ax - Awareness for stick front to not spin if loose aggro.", V_SILENCE);
		WriteLine("\ag[savebychar]\ax - Save character-specific settings to [CharName] section of configuration file.", V_SILENCE);
		WriteLine("\ag[breakonsummon]\ax - Halt plugin if character summoned beyond certain distance in a single pulse.", V_SILENCE);
		WriteLine("\ag[usewalk]\ax - Walk when closing in on moveto / camp return locations for precision.", V_SILENCE);
		WriteLine("\ag[alwaystruehed]\ax - Use legit heading adjustments with right/left keys.", V_SILENCE);
		WriteLine("\arThe following settings use a numerical value:", V_SILENCE);
		WriteLine("\ag[pulsecheck] [#]\ax - Set number of frames used to calculate moving average for stucklogic. (default 4)", V_SILENCE);
		WriteLine("\ag[pulseunstuck] [#]\ax - Set number of frames successfully moved forward to consider unstuck (default 5)", V_SILENCE);
		WriteLine("\ag[diststuck] [#.###]\ax - Set distance needed to move per pulse to not be considered stuck. (default 0.5)", V_SILENCE);
		WriteLine("\ag[campmindelay|campmaxdelay|pausemindelay|pausemaxdelay] [#]\ax - Set camp return or mpause/mousepause delays.", V_SILENCE);
		WriteLine("\ag[turnrate] [#.##]\ax (\ay1.0\ax to \ay100.0\ax) - Set increment used for loose heading turns.", V_SILENCE);
		return; // dont re-spam the output below
	}

	if (!bOnlyCmdHelp)
	{
		WriteLine("\arThe following options work for all commands (\ay/makecamp\ax, \ay/stick\ax, \ay/moveto\ax, \ay/circle\ax)", V_SILENCE);
		sprintf(szTempOut, "\ay%s [help]\ax - Displays help output for the command.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [debug]\ax - Outputs debugging information to file.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [load|save]\ax - Load settings from or save settings to the configuration file. (MQ2MoveUtils.ini)", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [pause|unpause]\ax - Pauses/Unpauses all aspects of this plugin.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\arFor detailed information on plugin settings, use \ay%s [help] [settings]\ax for more information.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\arThe remaining options apply to \ay%s\ax only.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
	}

	sprintf(szTempOut, "\ay%s [off]\ax - Ends the command.", szCommand);
	WriteLine(szTempOut, V_SILENCE);

	switch (ucCmdUsed)
	{
	case CMD_STICK:
		sprintf(szTempOut, "\ay%s\ax - Sticks to current target using default values.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [id #]\ax - Sticks to a target by its SpawnID or your target if the SpawnID is invalid.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [<#>]\ax - Sticks to target using the distance you supply, i.e. /stick 10 starts 10 range away.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [<#%%%>]\ax - Sticks to given %% of max melee range from target.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [moveback]\ax - Backs you up to current distance value if you get closer to your target.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [uw/underwater]\ax - Look Up/Down to face your target (underwater or not).", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [truehead|loose]\ax - Adjusts your heading legit/close to legit instead of instantly.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [snaproll]\ax - Moves you behind the target in a straight line, then proceeds with a basic stick.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [behind]\ax - Sticks you behind your target. \ar*Will spin in circles if you get aggro and no HoTT", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [behindonce]\ax - Sticks you behind target immediately, then proceeds with a basic stick.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [hold]\ax - Sticks you to current target even if your target changes.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [!front]\ax - Sticks you anywhere but front melee arc of target.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [front]\ax - Sticks you to the front melee arc of the target.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [pin]\ax - Sticks you to the sides of target.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [toggle] [breakongate|breakonwarp]\ax - Toggle command ending on mob gate/warp.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [breakdist] [#]\ax - Set distance for breakonwarp to trigger.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s <your_normal_options> [always]\ax - \arMust be at the end\ax. Will continue to stick to all new NPC targets supplied. Turns off hold/id.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		break;
	case CMD_MOVETO:
		sprintf(szTempOut, "\ay%s [loc Y X [Z]|off]\ax - Move to location | stop moving", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s id [SpawnID]\ax - Move to spawnid. If not numeric value then move to current target", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [loose]\ax - Toggles more realistic movement", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [dist <#|-#>]\ax - Furthest distance to be considered 'at location', negative subtracts from value", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [ydist|xdist] [#.##]\ax - Set distance used for precisey/precisex distance checking.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s mdist <#> [id | loc] \ax - Set distance inline with a moveto command.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		break;
	case CMD_MAKECAMP:
		sprintf(szTempOut, "\ay%s [on|off]\ax - Drops anchor at current location | removes anchor", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [loc <y> <x>]\ax - Makes camp at supplied anchor location", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [leash]\ax - Toggles leashing to current camp", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [leash [<dist>]]\ax - Set distance to exceed camp before leashing, always turns leash on", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [radius <dist>]\ax - Sets radius/size of camp", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [maxdelay <#>|mindelay <#>]\ax - Sets the max/minimum amount of time before returning to camp.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [return]\ax - Immediately return to camp", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [altreturn]\ax - Immediately return to alternate camp (if established)", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [player [<name>]]\ax - Set dynamic camp around player name in zone, or current target", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [leashlength] [#.##]\ax - Set leash length size.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [returnnoaggro] [on|off]\ax - Return to camp if not aggro (regardless of target).", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [returnnotlooting] [on|off]\ax - Do not return to camp if looting.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [returnhavetarget] [on|off]\ax - Return to camp even if have target.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [campradius] [#]\ax - Change radius of existing camp.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [scatdist] [#.##]\ax - Set scatter distance from center.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [scatsize] [#.##]\ax - Set scatter radius size.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [bearing] [#.##]\ax - Set scatter bearing from center.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [toggle] [usescatter]\ax - Use user-defined scatter values for camp returns.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		break;
	case CMD_CIRCLE:
		sprintf(szTempOut, "\ay%s [on <radius>]\ax - Turn circle on at current loc. If radius supplied sets size of circle.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [loc <y> <x>]\ax - Turn on circle at given anchor location. (Y X are the order /loc prints them).", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [radius <#>]\ax - Change the circle radius without resetting circle.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [drunken]\ax - Toggles random turns.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [clockwise|cw]\ax - Toggles circling clockwise.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [counterclockwise|ccw|reverse]\ax - Toggles circling counter-clockwise.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [forwards|backwards]\ax - Set movement direction forwards or backwards.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [toggle] [alwaysccw]\ax - Always use counter-clockwise (default is clockwise).", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [toggle] [alwaysbackwards]\ax - Always use backwards (default is forwards).", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [toggle] [alwaysdrunk]\ax - Always use drunken movement.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		sprintf(szTempOut, "\ay%s [set] [circleradius] [#]\ax - Change the circle radius without resetting circle.", szCommand);
		WriteLine(szTempOut, V_SILENCE);
		break;
	}
}

void DebugToWnd(unsigned char ucCmdUsed)
{
	char szTemp[MAX_STRING]   = {0};
	char szTempID[MAX_STRING] = "NULL";
	char szYes[8]             = "\agyes\ax";
	char szNo[7]              = "\arno\ax";
	char szDir[25]            = "\agNormal\ax";
	char szLongLine[48]       = "\ay---------------------------------------------";

	sprintf(szTemp, "\ay%s v%1.4f - Current Status", MODULE_NAME, MODULE_VERSION);
	WriteLine(szTemp, V_SILENCE);
	if (ucCmdUsed == CMD_STICK || ucCmdUsed == APPLY_TO_ALL)
	{
		if (STICK->Behind)
		{
			sprintf(szDir, "\agBehind\ax");
		}
		else if (STICK->Pin) 
		{
			sprintf(szDir, "\agSide\ax");
		}
		else if (STICK->NotFront) 
		{
			sprintf(szDir, "\agNot Front\ax");
		}
		else if (STICK->Front)
		{
			sprintf(szDir, "\agFront\ax");
		}
		WriteLine(szLongLine, V_SILENCE);
		sprintf(szTemp, "\ayStick\ax: Status(%s) Dir(%s) Dist(\ag%.2f\ax) Mod(\ag%.2f\ax) Mod%%%%(\ag%.2f%%%%\ax) Head(%s) Water(%s) MoveBack(%s) Hold(%s) Always(%s)", STICK->On ? szOn : szOff, szDir, STICK->Dist, STICK->DistMod, STICK->DistModP, (pMU->Head == H_TRUE) ? "\agTrue\ax" : ((pMU->Head == H_LOOSE) ? "\ayLoose\ax" : "\arFast\ax"), STICK->UW ? szYes : szNo, STICK->MoveBack ? szYes : szNo, STICK->Hold ? szYes : szNo, STICK->Always ? szOn : szOff);
		WriteLine(szTemp, V_SILENCE);
		if (STICK->Hold)
		{
			PSPAWNINFO pStickThis = (PSPAWNINFO)GetSpawnByID(STICK->HoldID);
			if (pStickThis)
			{
				sprintf(szTempID, "%s", pStickThis->DisplayedName);
			}
			sprintf(szTemp, "\ayStick\ax: Holding to ID(\ag%d\ax) Name(\ag%s\ax)", STICK->HoldID, szTempID);
			WriteLine(szTemp, V_SILENCE);
		}
		sprintf(szTemp, "\ayStick Options\ax: BreakOnWarp(%s) BreakDist(\ag%.2f\ax) BreakOnGate(%s) ", STICK->BreakWarp ? szOn : szOff, STICK->DistBreak, STICK->BreakGate ? szOn : szOff);
		WriteLine(szTemp, V_SILENCE);
	}
	if (ucCmdUsed == CMD_MOVETO || ucCmdUsed == APPLY_TO_ALL)
	{
		WriteLine(szLongLine, V_SILENCE);
		sprintf(szTemp, "\ayMoveto\ax: Status(%s) Y(\ag%.2f\ax) X(\ay%.2f\ax) Dist(\ag%.2f\ax) Mod(\ag%.2f\ax) YPrecise(%s) XPrecise(%s)", MOVETO->On ? szOn : szOff, MOVETO->Y, MOVETO->X, MOVETO->Dist, MOVETO->Mod, MOVETO->PreciseY ? szYes : szNo, MOVETO->PreciseX ? szYes : szNo);
		WriteLine(szTemp, V_SILENCE);
		sprintf(szTemp, "\ayMoveto Options\ax: Y-Dist(\ag%.2f\ax) X-Dist(\ag%.2f\ax)", MOVETO->DistY, MOVETO->DistX);
		WriteLine(szTemp, V_SILENCE);
	}
	if (ucCmdUsed == CMD_CIRCLE || ucCmdUsed == APPLY_TO_ALL)
	{
		WriteLine(szLongLine, V_SILENCE);
		sprintf(szTemp, "\ayCircle\ax: Status(%s) Y(\ag%.2f\ax) X(\ag%.2f\ax) Radius(\ag%.2f\ax) Drunken(%s) Backwards(%s) Clockwise(%s)", CIRCLE->On ? szOn : szOff, CIRCLE->Y, CIRCLE->X, CIRCLE->Radius, CIRCLE->Drunk ? szYes : szNo, CIRCLE->Backward ? szYes : szNo, CIRCLE->CCW ? szNo : szYes);
		WriteLine(szTemp, V_SILENCE);
		sprintf(szTemp, "\ayCircle Options\ax: AlwaysDrunk(%s) AlwaysBackwards(%s) AlwaysCCW(%s)", SET_C->Drunk ? szYes : szNo, SET_C->Backward ? szYes : szNo, SET_C->CCW ? szYes : szNo);
		WriteLine(szTemp, V_SILENCE);
	}
	if (ucCmdUsed == CMD_MAKECAMP || ucCmdUsed == APPLY_TO_ALL)
	{
		WriteLine(szLongLine, V_SILENCE);
		sprintf(szTemp, "\ayMakeCamp\ax: Status(%s) Player(%s) Y(\ag%.2f\ax) X(\ag%.2f\ax) Radius(\ag%.2f\ax) Leash(%s) LeashLen(\ag%.2f\ax) Returning(%s) ", CURCAMP->On ? szOn : szOff, CURCAMP->Pc ? szYes : szNo, CURCAMP->Y, CURCAMP->X, CURCAMP->Radius, CURCAMP->Leash ? szOn : szOff, CURCAMP->Length, CAMP->Auto ? szYes : szNo);
		WriteLine(szTemp, V_SILENCE);
		sprintf(szTemp, "\ayMakeCamp\ax: Scatter(%s) Bearing(\ag%.2f\ax) ScatDist(\ag%.2f\ax) ScatSize(\ag%.2f\ax)", CURCAMP->Scatter ? szOn : szOff, CURCAMP->Bearing, CURCAMP->ScatDist, CURCAMP->ScatSize);
		WriteLine(szTemp, V_SILENCE);
		if (CURCAMP->Pc)
		{
			PSPAWNINFO pCampThis = (PSPAWNINFO)GetSpawnByID(CURCAMP->PcID);
			if (pCampThis)
			{
				sprintf(szTempID, "%s", pCampThis->DisplayedName);
			}
			sprintf(szTemp, "\ayMakeCamp\ax: Player Name(\ag%s\ax) ID(\ag%d\ax)", szTempID, CURCAMP->PcID);
			WriteLine(szTemp, V_SILENCE);
		}
		if (ALTCAMP->On)
		{
			sprintf(szTemp, "\ayMakeCamp\ax: AlternateCamp(\agon\ax) AltAnchorY(\ag%.2f\ax) AltAnchorX(\ag%.2f\ax) AltRadius(\ag%.2f\ax)", ALTCAMP->Y, ALTCAMP->X, ALTCAMP->Radius);
			WriteLine(szTemp, V_SILENCE);
		}
		sprintf(szTemp, "\ayMakeCamp Options\ax: ReturnNoAggro(%s) MinDelay(\ag%d\ax) MaxDelay(\ag%d\ax)", CURCAMP->NoAggro ? szOn : szOff, CAMP->Min, CAMP->Max);
		WriteLine(szTemp, V_SILENCE);
	}

	if (ucCmdUsed == APPLY_TO_ALL)
	{
		WriteLine(szLongLine, V_SILENCE);
		sprintf(szTemp, "\ayPlugin Status\ax: Paused(%s) MinPause(\ag%d\ax) MaxPause(\ag%d\ax)", PAUSE->PausedMU ? szYes : szNo, PAUSE->Min, PAUSE->Max);
		WriteLine(szTemp, V_SILENCE);
		sprintf(szTemp, "\ayStuckLogic\ax: Enabled(%s) DistStuck(\ag%.3f\ax) PulseCheck(\ag%d\ax) PulseUnstuck(\ag%d\ax)", STUCK->On ? szYes : szNo, STUCK->Dist, STUCK->Check, STUCK->Unstuck);
		WriteLine(szTemp, V_SILENCE);
		sprintf(szTemp, "\ayPause Options\ax: AutoPause(%s) MPause(%s) BreakOnKB(%s) MousePause(%s) BreakOnMouse(%s)", SET->AutoPause ? szOn : szOff, SET->PauseKB ? szOn : szOff, SET->PauseMouse ? szOn : szOff, SET->BreakKB ? szOn : szOff, SET->BreakMouse ? szOn : szOff);
		WriteLine(szTemp, V_SILENCE);
		sprintf(szTemp, "\ayPlugin Options\ax: AutoSave(%s) FeignSupport(%s) BreakOnSummon(%s) BreakSummonDist(\ag%.2f\ax) AwareNotAggro(%s)", SET->AutoSave ? szOn : szOff, SET->Feign ? szOn : szOff, SET->BreakSummon ? szOn : szOff, SET->DistSummon, SET->Spin ? szOn : szOff);
		WriteLine(szTemp, V_SILENCE);
	}
}

void SpewMUError(unsigned char ucErrorNum)
{
	// this function exists to avoid the duplicate code for error messages that are used multiple places
	// any one-time error will still be in its own logic for clarity
	char szErrorMsg[MAX_STRING] = {0};

	switch(ucErrorNum)
	{
	case ERR_STICKSELF:
		sprintf(szErrorMsg, "\ay%s\aw:: (\arERROR\ax) You cannot stick to yourself!", MODULE_NAME);
		break;
	case ERR_STICKNONE:
		sprintf(szErrorMsg, "\ay%s\aw:: You must specify something to stick to!", MODULE_NAME);
		break;
	case ERR_BADMOVETO:
		EndPreviousCmd(true);
		sprintf(szErrorMsg, "\ay%s\aw:: (\arERROR\ax) \ay/moveto loc [ <Y> <X> [z] ]\ax was supplied incorrectly.", MODULE_NAME);
		break;
	case ERR_BADMAKECAMP:
		CAMP->NewCamp(false);
		sprintf(szErrorMsg, "\ay%s\aw:: (\arERROR\ax) \ay/makecamp loc [ <Y> <X> ]\ax was supplied incorrectly.", MODULE_NAME);
		break;
	case ERR_BADCIRCLE:
		EndPreviousCmd(true);
		sprintf(szErrorMsg, "\ay%s\aw:: (\arERROR\ax) Usage \ay/circle loc [ <y> <x> ] [other options]\ax", MODULE_NAME);
		break;
	case ERR_BADSPAWN:
		EndPreviousCmd(true);
		sprintf(szErrorMsg, "\ay%s\aw:: (\arERROR\ax) Invalid SpawnID and do not have a valid target.", MODULE_NAME);
		break;
	case ERR_BADDELAY:
		sprintf(szErrorMsg, "\ay%s\aw:: (\arERROR\ax) \ay[mindelay|maxdelay] [#]\ax was supplied incorrectly.", MODULE_NAME);
		break;
	default:
		// return if we didnt pass a valid msg number
		return;
	}
	WriteLine(szErrorMsg, V_ERRORS);
}

// End window output
// ----------------------------------------
// Debugger

void DebugToDebugger(char* szFormat, ...)
{
	char szDbgOutput[MAX_STRING] = {0};
	va_list vaList;
	va_start(vaList, szFormat);
	vsprintf(szDbgOutput, szFormat, vaList);
	OutputDebugString("[MUTILS] ");
	OutputDebugString(szDbgOutput);
	OutputDebugString("\n");
}

void SpewDebug(unsigned char ucDbgType, char* szOutput, ...)
{
	int i = 1;
	switch (ucDbgType)
	{
	case DBG_MAIN:
#ifdef DEBUGMAIN
		DebugToDebugger(szOutput);
#else
		i++;
#endif
		break;
	case DBG_STUCK:
#ifdef DEBUGSTUCK
		DebugToDebugger(szOutput);
#else
		i++;
#endif
		break;
	case DBG_MISC:
#ifdef DEBUGMISC
		DebugToDebugger(szOutput);
#else
		i++;
#endif
		break;
	case DBG_DISABLE:
		// do nothing with output
		i++;
		break;
	default:
		i++;
		break;
	}
}

// ----------------------------------------
// Begin file input/output

void DebugToINI(unsigned char ucCmdUsed)
{
	if (!ValidIngame(false)) return;

	char szTemp[MAX_STRING] = {0};
	char szCommand[15]      = {0};

	switch (ucCmdUsed)
	{
	case CMD_MAKECAMP:
		sprintf(szCommand, "/makecamp");
		break;
	case CMD_STICK:
		sprintf(szCommand, "/stick");
		break;
	case CMD_MOVETO:
		sprintf(szCommand, "/moveto");
		break;
	case CMD_CIRCLE:
		sprintf(szCommand, "/circle");
		break;
	}

	sprintf(szTemp, "%s v%1.4f", MODULE_NAME, MODULE_VERSION);
	WritePrivateProfileString("Version",       "Number",                szTemp,                                     szDebugName);
	WritePrivateProfileString("Commands",      "CommandUsed",           szCommand,                                  szDebugName);
	WritePrivateProfileString("GenericBOOL",   "pMU->Keybinds",         pMU->Keybinds         ? "true" : "false",   szDebugName);
	WritePrivateProfileString("GenericBOOL",   "PAUSE->PausedMU",       PAUSE->PausedMU       ? "true" : "false",   szDebugName);
	WritePrivateProfileString("GenericBOOL",   "pMU->BrokeSummon",      pMU->BrokeSummon      ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Strings",       "szMsg",                 szMsg,                                      szDebugName);
	WritePrivateProfileString("AggroChecking", "SET->Spin",             SET->Spin             ? "true" : "false",   szDebugName);
	WritePrivateProfileString("AggroChecking", "pMU->Aggro",            pMU->Aggro            ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->AutoPause",        SET->AutoPause        ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->PauseKB",          SET->PauseKB          ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->PauseMouse",       SET->PauseMouse       ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->BreakKB",          SET->BreakKB          ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->BreakMouse",       SET->BreakMouse       ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "STUCK->On",             STUCK->On             ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->AutoSave",         SET->AutoSave         ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->Feign",            SET->Feign            ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->SaveByChar",       SET->SaveByChar       ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->BreakSummon",      SET->BreakSummon      ? "true" : "false",   szDebugName);
	WritePrivateProfileString("INISettings",   "SET->DistSummon",       ftoa(SET->DistSummon,          szTemp),     szDebugName);
	WritePrivateProfileString("INISettings",   "SET->TurnRate",         ftoa(SET->TurnRate,            szTemp),     szDebugName);
	WritePrivateProfileString("INISettings",   "PAUSE->Min",            itoa(PAUSE->Min,               szTemp, 10), szDebugName);
	WritePrivateProfileString("INISettings",   "PAUSE->Max",            itoa(PAUSE->Max,               szTemp, 10), szDebugName);
	WritePrivateProfileString("INISettings",   "CAMP->Min",             itoa(CAMP->Min,                szTemp, 10), szDebugName);
	WritePrivateProfileString("INISettings",   "CAMP->Max",             itoa(CAMP->Max,                szTemp, 10), szDebugName);
	WritePrivateProfileString("SetOnPulse",    "MOVE->ChangeHead",      ftoa(MOVE->ChangeHead,         szTemp),     szDebugName);
	WritePrivateProfileString("SetOnPulse",    "PAUSE->UserKB",         PAUSE->UserKB         ? "true" : "false",   szDebugName);
	WritePrivateProfileString("SetOnPulse",    "CAMP->Returning",       CAMP->Returning       ? "true" : "false",   szDebugName);
	WritePrivateProfileString("SetOnPulse",    "CAMP->Auto",            CAMP->Auto            ? "true" : "false",   szDebugName);
	WritePrivateProfileString("SetOnPulse",    "STICK->HaveTarget",     STICK->HaveTarget     ? "true" : "false",   szDebugName);
	WritePrivateProfileString("SetOnPulse",    "STICK->CurDist",        ftoa(STICK->CurDist,           szTemp),     szDebugName);
	WritePrivateProfileString("Stick",         "STICK->BreakGate",      STICK->BreakGate      ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->BreakWarp",      STICK->BreakWarp      ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->DistBreak",      ftoa(STICK->DistBreak,         szTemp),     szDebugName);
	WritePrivateProfileString("Stick",         "STICK->On",             STICK->On             ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->SetDist",        STICK->SetDist        ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->Dist",           ftoa(STICK->Dist,              szTemp),     szDebugName);
	WritePrivateProfileString("Stick",         "STICK->DistMod",        ftoa(STICK->DistMod,           szTemp),     szDebugName);
	WritePrivateProfileString("Stick",         "STICK->DistModP",       ftoa(STICK->DistModP,          szTemp),     szDebugName);
	WritePrivateProfileString("Stick",         "STICK->MoveBack",       STICK->MoveBack       ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->Behind",         STICK->Behind         ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->BehindOnce",     STICK->BehindOnce     ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->Front",          STICK->Front          ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->NotFront",       STICK->NotFront       ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->Pin",            STICK->Pin            ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->Snaproll",       STICK->Snaproll       ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->Hold",           STICK->Hold           ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->HoldID",         itoa((int)STICK->HoldID,       szTemp, 10), szDebugName);
	WritePrivateProfileString("Stick",         "STICK->UW",             STICK->UW             ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Stick",         "STICK->Always",         STICK->Always         ? "true" : "false",   szDebugName);
	WritePrivateProfileString("StickSnaproll", "STICK->Snap->Y",        ftoa(STICK->Snap->Y,           szTemp),     szDebugName);
	WritePrivateProfileString("StickSnaproll", "STICK->Snap->X",        ftoa(STICK->Snap->X,           szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->On",         CURCAMP->On         ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->Y",          ftoa(CURCAMP->Y,             szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->X",          ftoa(CURCAMP->X,             szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->Radius",     ftoa(CURCAMP->Radius,        szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "ALTCAMP->On",         ALTCAMP->On         ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MakeCamp",      "ALTCAMP->Y",          ftoa(ALTCAMP->Y,             szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "ALTCAMP->X",          ftoa(ALTCAMP->X,             szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "ALTCAMP->Radius",     ftoa(ALTCAMP->Radius,        szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->Pc",         CURCAMP->Pc         ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->PcID",       itoa((int)CURCAMP->PcID,     szTemp, 10), szDebugName);
	WritePrivateProfileString("MakeCamp",      "CAMP->DoReturn",        CAMP->DoReturn        ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MakeCamp",      "CAMP->DoAlt",           CAMP->DoAlt           ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->Leash",      CURCAMP->Leash      ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->Length",     ftoa(CURCAMP->Length,        szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->Scatter",    CURCAMP->Scatter    ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->Bearing",    ftoa(CURCAMP->Bearing,       szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->ScatSize",   ftoa(CURCAMP->ScatSize,      szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->ScatDist",   ftoa(CURCAMP->ScatDist,      szTemp),     szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->NoAggro",    CURCAMP->NoAggro    ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->HaveTarget", CURCAMP->HaveTarget ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MakeCamp",      "CURCAMP->NotLoot",    CURCAMP->NotLoot    ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MoveTo",        "MOVETO->On",            MOVETO->On            ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MoveTo",        "MOVETO->Y",             ftoa(MOVETO->Y,                szTemp),     szDebugName);
	WritePrivateProfileString("MoveTo",        "MOVETO->X",             ftoa(MOVETO->X,                szTemp),     szDebugName);
	WritePrivateProfileString("MoveTo",        "MOVETO->Dist",          ftoa(MOVETO->Dist,             szTemp),     szDebugName);
	WritePrivateProfileString("MoveTo",        "MOVETO->PreciseY",      MOVETO->PreciseY      ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MoveTo",        "MOVETO->DistY",         ftoa(MOVETO->DistY,            szTemp),     szDebugName);
	WritePrivateProfileString("MoveTo",        "MOVETO->PreciseX",      MOVETO->PreciseX      ? "true" : "false",   szDebugName);
	WritePrivateProfileString("MoveTo",        "MOVETO->DistX",         ftoa(MOVETO->DistX,            szTemp),     szDebugName);
	WritePrivateProfileString("MoveTo",        "MOVETO->Walk",          MOVETO->Walk          ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Circle",        "CIRCLE->On",            CIRCLE->On            ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Circle",        "CIRCLE->Y",             ftoa(CIRCLE->Y,                szTemp),     szDebugName);
	WritePrivateProfileString("Circle",        "CIRCLE->X",             ftoa(CIRCLE->X,                szTemp),     szDebugName);
	WritePrivateProfileString("Circle",        "CIRCLE->Radius",        ftoa(CIRCLE->Radius,           szTemp),     szDebugName);
	WritePrivateProfileString("Circle",        "CIRCLE->Backward",      CIRCLE->Backward      ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Circle",        "CIRCLE->CCW",           CIRCLE->CCW           ? "true" : "false",   szDebugName);
	WritePrivateProfileString("Circle",        "CIRCLE->Drunk",         CIRCLE->Drunk         ? "true" : "false",   szDebugName);
	WritePrivateProfileString("StuckLogic",    "pMU->CmdFwd",           pMU->CmdFwd           ? "true" : "false",   szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->Jump",           STUCK->Jump           ? "true" : "false",   szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->TurnHalf",       STUCK->TurnHalf       ? "true" : "false",   szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->Check",          itoa((int)STUCK->Check,        szTemp, 10), szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->Unstuck",        itoa((int)STUCK->Unstuck,      szTemp, 10), szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->StuckInc",       itoa((int)STUCK->StuckInc,     szTemp, 10), szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->StuckDec",       itoa((int)STUCK->StuckDec,     szTemp, 10), szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->Dist",           ftoa(STUCK->Dist,              szTemp),     szDebugName);
	WritePrivateProfileString("StuckLogic",    "PulseAvg",              ftoa(STUCK->CurDist,           szTemp),     szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->TurnSize",       ftoa(STUCK->TurnSize,          szTemp),     szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->Y",              ftoa(STUCK->Y,                 szTemp),     szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->X",              ftoa(STUCK->X,                 szTemp),     szDebugName);
	WritePrivateProfileString("StuckLogic",    "STUCK->Z",              ftoa(STUCK->Z,                 szTemp),     szDebugName);
}

// ----------------------------------------
// Keybind handling

inline void KeyKiller(int iKeyPressed)
{
	if (iKeyPressed == iForward)
	{
		MOVE->DoMove(KILL_STRAFE, false, MU_WALKOFF);
		MOVE->DoMove(GO_BACKWARD, false, MU_WALKOFF);
	}
	else if (iKeyPressed == iBackward)
	{
		MOVE->DoMove(KILL_STRAFE, false, MU_WALKOFF);
		MOVE->DoMove(GO_FORWARD,  false, MU_WALKOFF);
	}
	else if (iKeyPressed == iStrafeLeft)
	{
		MOVE->DoMove(KILL_FB,     false, MU_WALKOFF);
		MOVE->DoMove(GO_RIGHT,    false, MU_WALKOFF);
	}
	else if (iKeyPressed == iStrafeRight)
	{
		MOVE->DoMove(KILL_FB,     false, MU_WALKOFF);
		MOVE->DoMove(GO_LEFT,     false, MU_WALKOFF);
	}
	else if (iKeyPressed == iAutoRun)
	{
		MOVE->DoMove(KILL_STRAFE, false, MU_WALKOFF);
	}
	else if (iKeyPressed == iTurnRight)
	{
		MOVE->DoMove(APPLY_TO_ALL, false, MU_WALKOFF);
		MOVE->ChangeHead = H_INACTIVE;
		if (pMU->Head == H_TRUE)
		{
			*pulTurnLeft                              = 0;
			pKeypressHandler->CommandState[iTurnLeft] = 0;
		}
		return; // return so that we do not auto heading adjust via StopHeading()
	}
	else if (iKeyPressed == iTurnLeft)
	{
		MOVE->DoMove(APPLY_TO_ALL, false, MU_WALKOFF);
		MOVE->ChangeHead = H_INACTIVE;
		if (pMU->Head == H_TRUE)
		{
			*pulTurnRight                              = 0;
			pKeypressHandler->CommandState[iTurnRight] = 0;
		}
		return; // return so that we do not auto heading adjust via StopHeading()
	}
	MOVE->StopHeading();
}

void KeybindPressed(int iKeyPressed, int iKeyDown)
{
	if (!ValidIngame(false)) return;

	if (pMU->Rooted)
	{
		MOVE->DoRoot();
		return;
	}

	//MOVE->ChangeHead = H_INACTIVE; // true/loose fights for heading if not reset
	bool bCmdActive = (STICK->Ready() || MOVETO->On || CIRCLE->On || CAMP->Auto) ? true : false;

	if (iKeyDown)
	{
		PAUSE->UserKB = true; // so makecamp wont auto return
		PAUSE->TimeStop(); // stop return time from kicking in if you let go of the key and repress again.
		if (!bCmdActive || PAUSE->PausedCmd) return; // do nothing else if plugin not currently moving or 'pause' from user already active

		if (SET->PauseKB)
		{
			if (MOVETO->On && CURCAMP->On && CURCAMP->Leash && !CAMP->Auto)
			{
				sprintf(szMsg, "\ay%s\aw:: Ended '/moveto' or '/makecamp return' because leash is on.", MODULE_NAME);
				WriteLine(szMsg, V_MAKECAMPFV);
				EndPreviousCmd(false);
			}
			PAUSE->PausedMU = true;
			if (!pMU->KeyKilled && !SET->WinEQ && !bOffsetOverride)
			{
				KeyKiller(iKeyPressed);
				pMU->KeyKilled = true;
			}
		}
		else if (SET->BreakKB)
		{
			if (!CAMP->Auto)
			{
				sprintf(szMsg, "\ay%s\aw:: Current command ended from manual movement.", MODULE_NAME);
				WriteLine(szMsg, V_MOVEPAUSE);
			}
			EndPreviousCmd(false); //EndPreviousCmd(true); // this stops kb input
			if (!pMU->KeyKilled && !SET->WinEQ && !bOffsetOverride)
			{
				KeyKiller(iKeyPressed);
				pMU->KeyKilled = true;
			}
		}
	}
	else
	{
		if (!SET->WinEQ && !bOffsetOverride)
		{
			if (*pulForward || *pulBackward || *pulTurnLeft || *pulTurnRight || *pulStrafeLeft || *pulStrafeRight || *pulAutoRun)
			{
				// return until all keys let go
				return;
			}
		}

		PAUSE->UserKB = pMU->KeyKilled = false;
		if (!SET->PauseKB || !bCmdActive || PAUSE->PausedCmd) return; // return if no reason to resume
		PAUSE->TimeStart(); // start resume timer, picked back up OnPulse()
	}
}

void FwdWrapper(char* szName, int iKeyDown)
{
	KeybindPressed(iForward, iKeyDown);
}

void BckWrapper(char* szName, int iKeyDown)
{
	KeybindPressed(iBackward, iKeyDown);
}

void LftWrapper(char* szName, int iKeyDown)
{
	KeybindPressed(iTurnLeft, iKeyDown);
}

void RgtWrapper(char* szName, int iKeyDown)
{
	KeybindPressed(iTurnRight, iKeyDown);
}

void StrafeLftWrapper(char* szName, int iKeyDown)
{
	KeybindPressed(iStrafeLeft, iKeyDown);
}

void StrafeRgtWrapper(char* szName, int iKeyDown)
{
	KeybindPressed(iStrafeRight, iKeyDown);
}

void AutoRunWrapper(char* szName, int iKeyDown)
{
	KeybindPressed(iAutoRun, iKeyDown);
}

void DoKeybinds()
{
	if (!pMU || pMU->Keybinds) return;
	AddMQ2KeyBind("MUTILS_FWD",        FwdWrapper);
	AddMQ2KeyBind("MUTILS_BCK",        BckWrapper);
	AddMQ2KeyBind("MUTILS_LFT",        LftWrapper);
	AddMQ2KeyBind("MUTILS_RGT",        RgtWrapper);
	AddMQ2KeyBind("MUTILS_STRAFE_LFT", StrafeLftWrapper);
	AddMQ2KeyBind("MUTILS_STRAFE_RGT", StrafeRgtWrapper);
	AddMQ2KeyBind("MUTILS_AUTORUN",    AutoRunWrapper);
	SetMQ2KeyBind("MUTILS_FWD",        FALSE, pKeypressHandler->NormalKey[iForward]);
	SetMQ2KeyBind("MUTILS_BCK",        FALSE, pKeypressHandler->NormalKey[iBackward]);
	SetMQ2KeyBind("MUTILS_LFT",        FALSE, pKeypressHandler->NormalKey[iTurnLeft]);
	SetMQ2KeyBind("MUTILS_RGT",        FALSE, pKeypressHandler->NormalKey[iTurnRight]);
	SetMQ2KeyBind("MUTILS_STRAFE_LFT", FALSE, pKeypressHandler->NormalKey[iStrafeLeft]);
	SetMQ2KeyBind("MUTILS_STRAFE_RGT", FALSE, pKeypressHandler->NormalKey[iStrafeRight]);
	SetMQ2KeyBind("MUTILS_AUTORUN",    FALSE, pKeypressHandler->NormalKey[iAutoRun]);
	SetMQ2KeyBind("MUTILS_FWD",        TRUE,  pKeypressHandler->AltKey[iForward]);
	SetMQ2KeyBind("MUTILS_BCK",        TRUE,  pKeypressHandler->AltKey[iBackward]);
	SetMQ2KeyBind("MUTILS_LFT",        TRUE,  pKeypressHandler->AltKey[iTurnLeft]);
	SetMQ2KeyBind("MUTILS_RGT",        TRUE,  pKeypressHandler->AltKey[iTurnRight]);
	SetMQ2KeyBind("MUTILS_STRAFE_LFT", TRUE,  pKeypressHandler->AltKey[iStrafeLeft]);
	SetMQ2KeyBind("MUTILS_STRAFE_RGT", TRUE,  pKeypressHandler->AltKey[iStrafeRight]);
	SetMQ2KeyBind("MUTILS_AUTORUN",    TRUE,  pKeypressHandler->AltKey[iAutoRun]);
	pMU->Keybinds = true;
}

void UndoKeybinds()
{
	if (!pMU || !pMU->Keybinds) return;
	RemoveMQ2KeyBind("MUTILS_FWD");
	RemoveMQ2KeyBind("MUTILS_BCK");
	RemoveMQ2KeyBind("MUTILS_LFT");
	RemoveMQ2KeyBind("MUTILS_RGT");
	RemoveMQ2KeyBind("MUTILS_STRAFE_LFT");
	RemoveMQ2KeyBind("MUTILS_STRAFE_RGT");
	RemoveMQ2KeyBind("MUTILS_AUTORUN");
	pMU->Keybinds = false;
}

inline void FindKeys()
{
	iForward     = FindMappableCommand("forward");
	iBackward    = FindMappableCommand("back");
	iAutoRun     = FindMappableCommand("autorun");
	iStrafeLeft  = FindMappableCommand("strafe_left");
	iStrafeRight = FindMappableCommand("strafe_right");
	iTurnLeft    = FindMappableCommand("left");
	iTurnRight   = FindMappableCommand("right");
	iJumpKey     = FindMappableCommand("jump");
	iDuckKey     = FindMappableCommand("duck");
	iRunWalk     = FindMappableCommand("run_walk");
}

// End keybind handling
// ----------------------------------------
// Offsets & pointers

// ---------------------------------------------------------------------------
// credit: radioactiveman/bunny771/(dom1n1k?) --------------------------------
bool DataCompare(const unsigned char* pucData, const unsigned char* pucMask, const char* pszMask)
{
	for (; *pszMask; ++pszMask, ++pucData, ++pucMask)
		if (*pszMask == 'x' && *pucData != *pucMask) return false;
	return (*pszMask) == NULL;
}

unsigned long FindPattern(unsigned long ulAddress, unsigned long ulLen, unsigned char* pucMask, char* pszMask)
{
	for (unsigned long i = 0; i < ulLen; i++)
	{
		if (DataCompare((unsigned char*)(ulAddress + i), pucMask, pszMask)) return (unsigned long)(ulAddress + i);
	}
	return 0;
}
// ---------------------------------------------------------------------------
// copyright: ieatacid -------------------------------------------------------
unsigned long GetDWordAt(unsigned long ulAddress, unsigned long ulNumBytes)
{
	if (ulAddress)
	{
		ulAddress += ulNumBytes;
		return *(unsigned long*)ulAddress;
	}
	return 0;
}
// ---------------------------------------------------------------------------

inline unsigned char FindPointers()
{
	if ((addrTurnRight   = FindPattern(FixOffset(0x420000), 0x100000, patternTurnRight, maskTurnRight)) == 0)     return 1;
	if ((pulTurnRight    = (unsigned long*)GetDWordAt(addrTurnRight, 1)) == 0)                         return 2;
	if ((pulStrafeLeft   = (unsigned long*)GetDWordAt(addrTurnRight, 7)) == 0)                         return 3;
	if ((pulStrafeRight  = (unsigned long*)GetDWordAt(addrTurnRight, 13)) == 0)                        return 4;
	if ((pulAutoRun      = (unsigned long*)GetDWordAt(addrTurnRight, 27)) == 0)                        return 5;
	if ((pulTurnLeft     = (unsigned long*)GetDWordAt(addrTurnRight, 42)) == 0)                        return 6;
	if ((addrMoveForward = FindPattern(FixOffset(0x420000), 0x100000, patternMoveForward, maskMoveForward)) == 0) return 7;
	if ((pulForward      = (unsigned long*)GetDWordAt(addrMoveForward, 1)) == 0)                       return 8;
	if (pulAutoRun      != (unsigned long*)GetDWordAt(addrMoveForward, 15))                            return 9;
	if ((pulBackward     = (unsigned long*)GetDWordAt(addrMoveForward, 30)) == 0)                      return 10;
	return 0;
}

void __cdecl EQGamestateService(bool Broadcast, unsigned int MSG, void *lpData)
{
#define GameState ((unsigned int)lpData)
	if (MSG==GAMESTATESERVICE_CHANGED)
	{
		if (GameState == GAMESTATE_INGAME)
		{
			sprintf(szCharName, "%s.%s", EQADDR_SERVERNAME, ((PCHARINFO)pCharData)->Name);
			FindKeys();
			if (!pMU->Keybinds)
			{
				DoKeybinds();
			}
			if (!pMU->Loaded)
			{
				pMU->Loaded = true;
			}
			SetupEvents(true);

			// randomly flip arc flag
			if (rand() % 100 > 50) STICK->RandFlag = !STICK->RandFlag;
			// turn walking off
			MOVE->SetWalk(false);
			// keybind bug fix
			pMU->KeyKilled = false;
		}
		else
		{
			if (pMU->Active() || CURCAMP->On)
			{
				sprintf(szMsg, "\ay%s\aw:: GameState change ended previous command.", MODULE_NAME);
				WriteLine(szMsg, V_SILENCE);
			}
			EndPreviousCmd(false); // using true here causes ctd
			CAMP->ResetBoth();
			pMU->MovetoBroke = pMU->StoppedMoveto = false;
			if (GameState == GAMESTATE_CHARSELECT)
			{
				UndoKeybinds();
				pMU->Loaded = false;
				SetupEvents(false, true);
				pMU->BrokeGM = pMU->BrokeSummon = false;
			}
		}
	}
#undef GameState
}

void __cdecl EQZoneService(bool Broadcast, unsigned int MSG, void *lpData)
{
	switch(MSG)
	{
	case ZONESERVICE_BEGINZONE:
		// same as OnBeginZone
		break;
	case ZONESERVICE_ENDZONE:
		// same as OnEndZone
		break;
	case ZONESERVICE_ZONED:
		DebugSpewAlways("ISXEQMovement::OnZoned()");
		EndPreviousCmd(false); // no need to use true here
		CAMP->ResetBoth();
		// keybind bug fix
		pMU->KeyKilled = false;
		pMU->navMesh = LoadMesh();
		break;
	}
}

void __cdecl EQSpawnService(bool Broadcast, unsigned int MSG, void *lpData)
{
	switch(MSG)
	{
#define pSpawn ((PSPAWNINFO)lpData)
	case SPAWNSERVICE_ADDSPAWN:
		if (SET->BreakGM || !pSpawn->SpawnID)
			return;
		if (pSpawn->GM) {
			__time64_t tCurrentTime;
			char szTime[30] = {0};
			struct tm* THE_TIME;
			_time64(&tCurrentTime);
			THE_TIME = _localtime64(&tCurrentTime);
			strftime(szTime, 20, " [%H:%M:%S]", THE_TIME);
			sprintf(szMsg, "\ay%s\aw:: \arWARNING\ax Plugin halted from\ag [GM] %s\ax in zone. -- \aw%s", MODULE_NAME, pSpawn->DisplayedName, szTime);
			WriteLine(szMsg, V_BREAKONGM);
			pMU->BrokeGM = true;
			EndPreviousCmd(ValidIngame());
		}
		break;
	case SPAWNSERVICE_REMOVESPAWN:
		if (!pSpawn->SpawnID) return;

		if (pSpawn->SpawnID == STICK->HoldID)
			STICK->StopHold();

		if (pSpawn->SpawnID == CURCAMP->PcID)
			CAMP->ResetPlayer(true);

		if (pSpawn->GM && SET->BreakGM && pMU && pMU->BrokeGM)
		{
			__time64_t tCurrentTime;
			char szTime[30] = {0};
			struct tm* THE_TIME;
			_time64(&tCurrentTime);
			THE_TIME = _localtime64(&tCurrentTime);
			strftime(szTime, 20, " [%H:%M:%S]", THE_TIME);
			sprintf(szMsg, "\ay%s\aw::\ag [GM] %s\ax has left the zone or turned invisible. Use \ag/stick imsafe\ax to allow command usage. -- \aw%s", MODULE_NAME, pSpawn->DisplayedName, szTime);
			WriteLine(szMsg, V_BREAKONGM);
		}
		break;
#undef pSpawn
#define pGroundItem ((PGROUNDITEM)lpData)
	case SPAWNSERVICE_ADDITEM:
		// same as OnAddGroundItem
		break;
	case SPAWNSERVICE_REMOVEITEM:
		// same as OnRemoveGroundItem
		break;
#undef pGroundItem
	}
}

void __cdecl PulseService(bool Broadcast, unsigned int MSG, void *lpData)
{
	if (MSG==PULSE_PULSE)
	{
		if (!ValidIngame(false)) return;

		if (InHoverState())
		{
			// end root command if active
			MOVE->StopRoot();
			// end movement commands (not camp) when hovering
			if (pMU->Active())
			{
				EndPreviousCmd(false);
				sprintf(szMsg, "\ay%s\aw:: \arYour death has ended the previous command.", MODULE_NAME);
				WriteLine(szMsg, V_SILENCE);
			}
			pMU->FixWalk = true;
			return;
		}
		// if we died, turn walking off when we rez
		if (pMU->FixWalk)
		{
			MOVE->SetWalk(false);
			pMU->FixWalk = false;
		}

		// heading adjustment if needed
		MOVE->AutoHead();
		// MoveUtils.Aggro TLO
		pMU->AggroTLO();
		// check BreakOnSummon and BreakOnGM
		if (pMU->Broken()) return;
		// rootme command
		MOVE->DoRoot();
		// handle mousepause & movepause timers
		PAUSE->HandlePause();
		// if plugin is paused, do not process commands
		if (PAUSE->Waiting()) return;

		// no initial checks preventing main process call
		// check if individual commands are active & ready
		if (STICK->On)
		{
			if (!STICK->Ready()) return;
			MainProcess(CMD_STICK);
		}
		else if (MOVETO->On)
		{
			MainProcess(CMD_MOVETO);
		}
		else if (CIRCLE->On)
		{
			if (CIRCLE->Drunk && CIRCLE->Wait()) return;
			MainProcess(CMD_CIRCLE);
		}
		else if (!PAUSE->UserMouse && (CURCAMP->On || CAMP->DoAlt))
		{
			// this will process camp returns when no other commands active
			MainProcess(CMD_MAKECAMP);
		}
	}
}

// This uses the Services service to connect to ISXEQ services
void __cdecl ServicesService(bool Broadcast, unsigned int MSG, void *lpData)
{
#define Name ((char*)lpData)
	switch(MSG)
	{
	case SERVICES_ADDED:
		if (!stricmp(Name,"EQ Gamestate Service"))
		{
			hEQGamestateService=pISInterface->ConnectService(pExtension,Name,EQGamestateService);
		}
		else if (!stricmp(Name,"EQ Spawn Service"))
		{
			hEQSpawnService=pISInterface->ConnectService(pExtension,Name,EQSpawnService);
		}
		else if (!stricmp(Name,"EQ Zone Service"))
		{
			hEQZoneService=pISInterface->ConnectService(pExtension,Name,EQZoneService);
		}
		else if (!stricmp(Name,"EQ Chat Service"))
		{
			hEQChatService=pISInterface->ConnectService(pExtension,Name,EQChatService);
		}
		break;
	case SERVICES_REMOVED:
		if (!stricmp(Name,"EQ Gamestate Service"))
		{
			if (hEQGamestateService)
			{
				pISInterface->DisconnectService(pExtension,hEQGamestateService);
				hEQGamestateService=0;
			}
		}
		else if (!stricmp(Name,"EQ Spawn Service"))
		{
			if (hEQSpawnService)
			{
				pISInterface->DisconnectService(pExtension,hEQSpawnService);
				hEQSpawnService=0;
			}
		}
		else if (!stricmp(Name,"EQ Zone Service"))
		{
			if (hEQZoneService)
			{
				pISInterface->DisconnectService(pExtension,hEQZoneService);
				hEQZoneService=0;
			}
		}
		else if (!stricmp(Name,"EQ Chat Service"))
		{
			if (hEQChatService)
			{
				pISInterface->DisconnectService(pExtension,hEQChatService);
				hEQChatService=0;
			}
		}
		break;
	}
#undef Name
}

void ISXEQMovement::RegisterTopLevelObjects()
{
	pISInterface->AddTopLevelObject("Stick", TLO_Stick);
	pISInterface->AddTopLevelObject("MakeCamp", TLO_MakeCamp);
	pISInterface->AddTopLevelObject("MoveTo", TLO_MoveTo);
	pISInterface->AddTopLevelObject("Circle", TLO_Circling);
	pISInterface->AddTopLevelObject("Movement", TLO_Movement);
}

void LoadConfig()
{
	char szTempS[MAX_STRING] = {0};
	float szTempF = 0;
	int szTempI = 0;
	bool szTempB = false;
	bool bRewriteConfig = false;  // re-save if bad values were read

	if (unsigned int DefaultGUID=pISInterface->FindSet(ISXEQMovementXML, "Defaults")) {
		if (pISInterface->GetSetting(DefaultGUID, "AllowMove", szTempF))
			if (szTempF >= 10.0f)
				SET->AllowMove = szTempF;
			else
				bRewriteConfig = true;

		pISInterface->GetSetting(DefaultGUID, "AutoPause", SET->AutoPause);
		pISInterface->GetSetting(DefaultGUID, "AutoSave", SET->AutoSave);
		pISInterface->GetSetting(DefaultGUID, "AutoUW", SET->AutoUW);
		pISInterface->GetSetting(DefaultGUID, "BreakKeyboard", SET->BreakKB);
		pISInterface->GetSetting(DefaultGUID, "BreakMouse", SET->BreakMouse);
		pISInterface->GetSetting(DefaultGUID, "BreakOnGM", SET->BreakGM);
		pISInterface->GetSetting(DefaultGUID, "BreakOnSummon", SET->BreakSummon);
		if (pISInterface->GetSetting(DefaultGUID, "DistSummon", szTempF))
			if (szTempF >= 2.0f)
				SET->DistSummon = szTempF;
			else
				bRewriteConfig = true;

		pISInterface->GetSetting(DefaultGUID, "FeignSupport", SET->Feign);
		if (pISInterface->GetSetting(DefaultGUID, "Heading", szTempS, MAX_STRING)) {
			SET->Head = H_FAST;
			if (!strnicmp(szTempS, "true", 5))
				SET->Head = H_TRUE;
			else if (!strnicmp(szTempS, "loose", 6))
				SET->Head = H_LOOSE;
		}
		pISInterface->GetSetting(DefaultGUID, "KeyboardPause", SET->PauseKB);
		if (SET->PauseKB)
			SET->BreakKB = false;

		pISInterface->GetSetting(DefaultGUID, "MousePause", SET->PauseMouse);
		if (SET->PauseMouse)
			SET->BreakMouse = false;

		pISInterface->GetSetting(DefaultGUID, "LockPause", SET->LockPause);
		if (pISInterface->GetSetting(DefaultGUID, "PauseMaxDelay", szTempI))
			PAUSE->MaxDelay(szTempI);

		if (pISInterface->GetSetting(DefaultGUID, "PauseMinDelay", szTempI))
			PAUSE->MinDelay(szTempI);

		pISInterface->GetSetting(DefaultGUID, "SaveByChar", SET->SaveByChar);
		if (pISInterface->GetSetting(DefaultGUID, "TurnRate", szTempF))
			if (szTempF >= 1.0f && szTempF <= 100.0f)
				SET->TurnRate = szTempF;
			else
				bRewriteConfig = true;

		// verbosity flag handling
		if (pISInterface->GetSetting(DefaultGUID, "Verbosity", szTempB))
			if (szTempB)
				uiVerbLevel |= V_VERBOSITY;
			else
				uiVerbLevel &= ~V_VERBOSITY;

		// FullVerbosity is more frequent, detailed output, and differs from Verbosity
		// Setting one does not include the text of the other.
		if (pISInterface->GetSetting(DefaultGUID, "FullVerbosity", szTempB))
			if (szTempB)
				uiVerbLevel |= V_FULLVERBOSITY;
			else
				uiVerbLevel &= ~V_FULLVERBOSITY;

		// Total Silence disables all output except extreme error messages and BreakOnSummon
		if (pISInterface->GetSetting(DefaultGUID, "TotalSilence", szTempB))
			if (szTempB)
				uiVerbLevel = V_SILENCE;

		if (uiVerbLevel != V_SILENCE) {
			pISInterface->GetSetting(DefaultGUID, "VerbosityFlags", uiVerbLevel);
			if (pISInterface->GetSetting(DefaultGUID, "AutoPauseMsg", szTempB))
				if (szTempB)
					uiVerbLevel |= V_AUTOPAUSE;
				else
					uiVerbLevel &= ~V_AUTOPAUSE;

			if (pISInterface->GetSetting(DefaultGUID, "HideHelp", szTempB))
				if (szTempB)
					uiVerbLevel |= V_HIDEHELP;
				else
					uiVerbLevel &= ~V_HIDEHELP;
		}
		pISInterface->GetSetting(DefaultGUID, "WinEQ", SET->WinEQ);
	}
	if (unsigned int StickGUID=pISInterface->FindSet(ISXEQMovementXML, "Stick")) {
		pISInterface->GetSetting(StickGUID, "AlwaysUW", SET_S->UW);
		if (pISInterface->GetSetting(StickGUID, "ArcBehind", szTempF))
			if (szTempF > 5.0f && szTempF < 260.0f)
				SET_S->ArcBehind = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(StickGUID, "ArcNotFront", szTempF))
			if (szTempF > 5.0f && szTempF < 260.0f)
				SET_S->ArcNotFront = szTempF;
			else
				bRewriteConfig = true;

		pISInterface->GetSetting(StickGUID, "AwareNotAggro", SET->Spin);
		pISInterface->GetSetting(StickGUID, "BreakOnGate", SET_S->BreakGate);
		pISInterface->GetSetting(StickGUID, "BreakOnTarget", SET_S->BreakTarget);
		pISInterface->GetSetting(StickGUID, "BreakOnWarp", SET_S->BreakWarp);
		if (SET_S->BreakWarp)
			SET_S->PauseWarp = false;
		pISInterface->GetSetting(StickGUID, "PauseOnWarp", SET_S->PauseWarp);
		if (SET_S->PauseWarp)
			SET_S->BreakWarp = false;
		pISInterface->GetSetting(StickGUID, "DelayStrafe", SET_S->DelayStrafe);
		if (pISInterface->GetSetting(StickGUID, "DistBackup", szTempF))
			if (szTempF >= 1.0f)
				SET_S->DistBack = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(StickGUID, "DistBreak", szTempF))
			if (szTempF >= 1.0f)
				SET_S->DistBreak = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(StickGUID, "DistMod", szTempF))
			if (szTempF >= 0.0f)
				SET_S->DistMod = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(StickGUID, "DistModP", szTempF))
			if (szTempF >= 0.0f)
				SET_S->DistModP = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(StickGUID, "DistSnaproll", szTempF))
			if (szTempF >= 1.0f)
				SET_S->DistSnap = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(StickGUID, "StrafeMaxDelay", szTempI))
			SET_S->MaxDelay(szTempI);

		if (pISInterface->GetSetting(StickGUID, "StrafeMinDelay", szTempI))
			SET_S->MinDelay(szTempI);

		pISInterface->GetSetting(StickGUID, "RandomArc", SET_S->Randomize);
		pISInterface->GetSetting(StickGUID, "UseBackward", SET_S->UseBack);
		pISInterface->GetSetting(StickGUID, "UseFleeing", SET_S->UseFleeing);
		pISInterface->GetSetting(StickGUID, "UseWalk", SET_S->Walk);
	}
	if (unsigned int MakeCampGUID=pISInterface->FindSet(ISXEQMovementXML, "MakeCamp")) {
		if (pISInterface->GetSetting(MakeCampGUID, "CampRadius", szTempF))
			if (szTempF >= 5.0f)
				SET_CAMP->SetRadius(szTempF);
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(MakeCampGUID, "MaxDelay", szTempI))
			SET_CAMP->MaxDelay(szTempI);

		if (pISInterface->GetSetting(MakeCampGUID, "MinDelay", szTempI))
			SET_CAMP->MinDelay(szTempI);

		pISInterface->GetSetting(MakeCampGUID, "RealtimePlayer", SET_CAMP->Realtime);
		pISInterface->GetSetting(MakeCampGUID, "ReturnHaveTarget", SET_CAMP->HaveTarget);
		pISInterface->GetSetting(MakeCampGUID, "ReturnNoAggro", SET_CAMP->NoAggro);
		pISInterface->GetSetting(MakeCampGUID, "ReturnNotLooting", SET_CAMP->NotLoot);
		pISInterface->GetSetting(MakeCampGUID, "UseLeash", SET_CAMP->Leash);
		if (pISInterface->GetSetting(MakeCampGUID, "LeashLength", szTempF))
			if (szTempF >= SET_CAMP->Radius)
				SET_CAMP->SetLeash(szTempF);
			else
				bRewriteConfig = true;

		pISInterface->GetSetting(MakeCampGUID, "UseScatter", SET_CAMP->Scatter);
		pISInterface->GetSetting(MakeCampGUID, "Bearing", SET_CAMP->Bearing);
		if (pISInterface->GetSetting(MakeCampGUID, "ScatDist", szTempF))
			if (szTempF >= 1.0f)
				SET_CAMP->ScatDist = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(MakeCampGUID, "ScatSize", szTempF))
			if (szTempF >= 1.0f)
				SET_CAMP->ScatSize = szTempF;
			else
				bRewriteConfig = true;
	}
	if (unsigned int MoveToGUID=pISInterface->FindSet(ISXEQMovementXML, "MoveTo")) {
		pISInterface->GetSetting(MoveToGUID, "AlwaysUW", SET_M->UW);
		if (pISInterface->GetSetting(MoveToGUID, "ArrivalDist", szTempF))
			if (szTempF >= 1.0f)
				SET_M->Dist = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(MoveToGUID, "ArrivalDistX", szTempF))
			if (szTempF >= 1.0f)
				SET_M->DistX = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(MoveToGUID, "ArrivalDistY", szTempF))
			if (szTempF >= 1.0f)
				SET_M->DistY = szTempF;
			else
				bRewriteConfig = true;

		pISInterface->GetSetting(MoveToGUID, "BreakOnAggro", SET_M->BreakAggro);
		pISInterface->GetSetting(MoveToGUID, "BreakOnHit", SET_M->BreakHit);
		if (pISInterface->GetSetting(MoveToGUID, "DistBackup", szTempF))
			if (szTempF >= 1.0f)
				SET_M->DistBack = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(MoveToGUID, "MoveToMod", szTempF))
			if (szTempF >= 0.0f)
				SET_M->Mod = szTempF;
			else
				bRewriteConfig = true;

		pISInterface->GetSetting(MoveToGUID, "UseBackward", SET_M->UseBack);
		pISInterface->GetSetting(MoveToGUID, "UseWalk", SET_M->Walk);
	}
	if (unsigned int CircleGUID=pISInterface->FindSet(ISXEQMovementXML, "Circle")) {
		pISInterface->GetSetting(CircleGUID, "Backward", SET_C->Backward);
		pISInterface->GetSetting(CircleGUID, "CCW", SET_C->CCW);
		pISInterface->GetSetting(CircleGUID, "Drunken", SET_C->Drunk);
		if (pISInterface->GetSetting(CircleGUID, "RadiusSize", szTempF))
			if (szTempF >= 5.0f)
				SET_C->SetRadius(szTempF);
			else
				bRewriteConfig = true;
	}
	if (unsigned int StuckLogicGUID=pISInterface->FindSet(ISXEQMovementXML, "StuckLogic")) {
		pISInterface->GetSetting(StuckLogicGUID, "StuckLogic", STUCK->On);
		if (pISInterface->GetSetting(StuckLogicGUID, "DistStuck", szTempF))
			if (szTempF >= 0.0f)
				STUCK->Dist = szTempF;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(StuckLogicGUID, "PulseCheck", szTempI))
			if (szTempI > 1)
				STUCK->Check = szTempI;
			else
				bRewriteConfig = true;

		if (pISInterface->GetSetting(StuckLogicGUID, "PulseUnstuck", szTempI))
			if (szTempI > 1)
				STUCK->Unstuck = szTempI;
			else
				bRewriteConfig = true;

		pISInterface->GetSetting(StuckLogicGUID, "TryToJump", STUCK->Jump);
		pISInterface->GetSetting(StuckLogicGUID, "TurnHalf", STUCK->TurnHalf);
	}
	if (unsigned int CharGUID=pISInterface->FindSet(ISXEQMovementXML, szCharName)) {
		// found a character specific block for this character
		szTempB = false;
		pISInterface->GetSetting(CharGUID, "DisregardMe", szTempB);
		if (SET->SaveByChar && !szTempB) {
			if (pISInterface->GetSetting(CharGUID, "AllowMove", szTempF))
				if (szTempF >= 10.0f)
					SET->AllowMove = szTempF;
				else
					bRewriteConfig = true;

			if (pISInterface->GetSetting(CharGUID, "ArcBehind", szTempF))
				if (szTempF > 5.0f && szTempF < 260.0f)
					SET_S->ArcBehind = szTempF;
				else
					bRewriteConfig = true;

			if (pISInterface->GetSetting(CharGUID, "ArcNotFront", szTempF))
				if (szTempF > 5.0f && szTempF < 260.0f)
					SET_S->ArcNotFront = szTempF;
				else
					bRewriteConfig = true;

			pISInterface->GetSetting(CharGUID, "AutoSave", SET->AutoSave);
			pISInterface->GetSetting(CharGUID, "AutoUW", SET->AutoUW);

			pISInterface->GetSetting(CharGUID, "BreakOnGate", SET_S->BreakGate);
			pISInterface->GetSetting(CharGUID, "BreakOnWarp", SET_S->BreakWarp);
			if (SET_S->BreakWarp)
				SET_S->PauseWarp = false;
			pISInterface->GetSetting(CharGUID, "PauseOnWarp", SET_S->PauseWarp);
			if (SET_S->PauseWarp)
				SET_S->BreakWarp = false;
			pISInterface->GetSetting(CharGUID, "LockPause", SET->LockPause);
			if (pISInterface->GetSetting(CharGUID, "DistBreak", szTempF))
				if (szTempF >= 1.0f)
					STICK->DistBreak = szTempF;
				else
					bRewriteConfig = true;

			if (pISInterface->GetSetting(CharGUID, "DistSnaproll", szTempF))
				if (szTempF >= 1.0f)
					SET_S->DistSnap = szTempF;
				else
					bRewriteConfig = true;

			pISInterface->GetSetting(CharGUID, "FeignSupport", SET->Feign);
			if (pISInterface->GetSetting(CharGUID, "Heading", szTempS, MAX_STRING)) {
				SET->Head = H_FAST;
				if (!strnicmp(szTempS, "true", 5))
					SET->Head = H_TRUE;
				else if (!strnicmp(szTempS, "loose", 6))
					SET->Head = H_LOOSE;
			}
			if (pISInterface->GetSetting(CharGUID, "LeashLength", szTempF))
				if (szTempF >= SET_CAMP->Radius)
					SET_CAMP->SetLeash(szTempF);
				else
					bRewriteConfig = true;

			pISInterface->GetSetting(CharGUID, "UseLeash", SET_CAMP->Leash);

			// verbosity flag handling
			if (pISInterface->GetSetting(CharGUID, "Verbosity", szTempB))
				if (szTempB)
					uiVerbLevel |= V_VERBOSITY;
				else
					uiVerbLevel &= ~V_VERBOSITY;

			// FullVerbosity is more frequent, detailed output, and differs from Verbosity
			// Setting one does not include the text of the other.
			if (pISInterface->GetSetting(CharGUID, "FullVerbosity", szTempB))
				if (szTempB)
					uiVerbLevel |= V_FULLVERBOSITY;
				else
					uiVerbLevel &= ~V_FULLVERBOSITY;
	
			// set flags directly, if no entry present use results of above reads
			pISInterface->GetSetting(CharGUID, "VerbosityFlags", uiVerbLevel);
			if (pISInterface->GetSetting(CharGUID, "CampRadius", szTempF))
			if (szTempF >= 5.0f)
				SET_CAMP->SetRadius(szTempF);
			else
				bRewriteConfig = true;

			pISInterface->GetSetting(CharGUID, "RealtimePlayer", SET_CAMP->Realtime);
			pISInterface->GetSetting(CharGUID, "UseScatter", SET_CAMP->Scatter);
			pISInterface->GetSetting(CharGUID, "Bearing", SET_CAMP->Bearing);
			if (pISInterface->GetSetting(CharGUID, "ScatDist", szTempF))
				if (szTempF >= 1.0f)
					SET_CAMP->ScatDist = szTempF;
				else
					bRewriteConfig = true;

			if (pISInterface->GetSetting(CharGUID, "ScatSize", szTempF))
				if (szTempF >= 1.0f)
					SET_CAMP->ScatSize = szTempF;
				else
					bRewriteConfig = true;
		}
	}

	if (bRewriteConfig)
		SaveConfig();

	pMU->NewDefaults();
}

void SaveConfig()
{
	unsigned int DefaultGUID = pISInterface->CreateSet(ISXEQMovementXML, "Defaults");
	if (DefaultGUID == 0)
		DefaultGUID = pISInterface->FindSet(ISXEQMovementXML, "Defaults");

	pISInterface->SetSetting(DefaultGUID, "AllowMove", SET->AllowMove);
	pISInterface->SetSetting(DefaultGUID, "AutoPause", SET->AutoPause);
	pISInterface->SetSetting(DefaultGUID, "AutoPauseMsg", (uiVerbLevel & V_AUTOPAUSE) == V_AUTOPAUSE);
	pISInterface->SetSetting(DefaultGUID, "AutoSave", SET->AutoSave);
	pISInterface->SetSetting(DefaultGUID, "AutoUW", SET->AutoUW);
	pISInterface->SetSetting(DefaultGUID, "BreakKeyboard", SET->BreakKB);
	pISInterface->SetSetting(DefaultGUID, "BreakMouse", SET->BreakMouse);
	pISInterface->SetSetting(DefaultGUID, "BreakOnGM", SET->BreakGM);
	pISInterface->SetSetting(DefaultGUID, "BreakOnSummon", SET->BreakSummon);
	pISInterface->SetSetting(DefaultGUID, "DistSummon", SET->DistSummon);
	pISInterface->SetSetting(DefaultGUID, "FeignSupport", SET->Feign);
	pISInterface->SetSetting(DefaultGUID, "Heading", (SET->Head == H_TRUE) ? "true" : (SET->Head == H_LOOSE ? "loose" : "fast"));
	pISInterface->SetSetting(DefaultGUID, "HideHelp", (uiVerbLevel & V_HIDEHELP) == V_HIDEHELP);
	pISInterface->SetSetting(DefaultGUID, "KeyboardPause", SET->PauseKB);
	pISInterface->SetSetting(DefaultGUID, "MousePause", SET->PauseMouse);
	pISInterface->SetSetting(DefaultGUID, "LockPause", SET->LockPause);
	pISInterface->SetSetting(DefaultGUID, "PauseMinDelay", PAUSE->Min);
	pISInterface->SetSetting(DefaultGUID, "PauseMaxDelay", PAUSE->Max);
	pISInterface->SetSetting(DefaultGUID, "SaveByChar", SET->SaveByChar);
	pISInterface->SetSetting(DefaultGUID, "TurnRate", SET->TurnRate);
	pISInterface->SetSetting(DefaultGUID, "Verbosity", (uiVerbLevel & V_VERBOSITY) == V_VERBOSITY);
	pISInterface->SetSetting(DefaultGUID, "FullVerbosity", (uiVerbLevel & V_FULLVERBOSITY) == V_FULLVERBOSITY);
	pISInterface->SetSetting(DefaultGUID, "TotalSilence", (uiVerbLevel == 0));
	pISInterface->SetSetting(DefaultGUID, "VerbosityFlags", uiVerbLevel);
	pISInterface->SetSetting(DefaultGUID, "WinEQ", SET->WinEQ);

	unsigned int StickGUID = pISInterface->CreateSet(ISXEQMovementXML, "Stick");
	if (StickGUID == 0)
		StickGUID = pISInterface->FindSet(ISXEQMovementXML, "Stick");
	pISInterface->SetSetting(StickGUID, "AlwaysUW", SET_S->UW);
	pISInterface->SetSetting(StickGUID, "AwareNotAggro", SET->Spin);
	pISInterface->SetSetting(StickGUID, "ArcBehind", SET_S->ArcBehind);
	pISInterface->SetSetting(StickGUID, "ArcNotFront", SET_S->ArcNotFront);
	pISInterface->SetSetting(StickGUID, "BreakOnGate", SET_S->BreakGate);
	pISInterface->SetSetting(StickGUID, "BreakOnTarget", SET_S->BreakTarget);
	pISInterface->SetSetting(StickGUID, "BreakOnWarp", SET_S->BreakWarp);
	pISInterface->SetSetting(StickGUID, "PauseOnWarp", SET_S->PauseWarp);
	pISInterface->SetSetting(StickGUID, "DelayStrafe", SET_S->DelayStrafe);
	pISInterface->SetSetting(StickGUID, "DistBackup", SET_S->DistBack);
	pISInterface->SetSetting(StickGUID, "DistBreak", SET_S->DistBreak);
	pISInterface->SetSetting(StickGUID, "DistMod", SET_S->DistMod);
	pISInterface->SetSetting(StickGUID, "DistMod%", SET_S->DistModP);
	pISInterface->SetSetting(StickGUID, "DistSnaproll", SET_S->DistSnap);
	pISInterface->SetSetting(StickGUID, "RandomArc", SET_S->Randomize);
	pISInterface->SetSetting(StickGUID, "StrafeMinDelay", SET_S->Min);
	pISInterface->SetSetting(StickGUID, "StrafeMaxDelay", SET_S->Max);
	pISInterface->SetSetting(StickGUID, "UseBackward", SET_S->UseBack);
	pISInterface->SetSetting(StickGUID, "UseFleeing", SET_S->UseFleeing);
	pISInterface->SetSetting(StickGUID, "UseWalk", SET_S->Walk);

	unsigned int MakeCampGUID = pISInterface->CreateSet(ISXEQMovementXML, "MakeCamp");
	if (MakeCampGUID == 0)
		MakeCampGUID = pISInterface->FindSet(ISXEQMovementXML, "MakeCamp");
	pISInterface->SetSetting(MakeCampGUID, "CampRadius", SET_CAMP->Radius);
	pISInterface->SetSetting(MakeCampGUID, "MinDelay", SET_CAMP->Min);
	pISInterface->SetSetting(MakeCampGUID, "MaxDelay", SET_CAMP->Max);
	pISInterface->SetSetting(MakeCampGUID, "RealtimePlayer", SET_CAMP->Realtime);
	pISInterface->SetSetting(MakeCampGUID, "ReturnHaveTarget", SET_CAMP->HaveTarget);
	pISInterface->SetSetting(MakeCampGUID, "ReturnNoAggro", SET_CAMP->NoAggro);
	pISInterface->SetSetting(MakeCampGUID, "ReturnNotLooting", SET_CAMP->NotLoot);
	pISInterface->SetSetting(MakeCampGUID, "UseLeash", SET_CAMP->Leash);
	pISInterface->SetSetting(MakeCampGUID, "LeashLength", SET_CAMP->Length);
	pISInterface->SetSetting(MakeCampGUID, "UseScatter", SET_CAMP->Scatter);
	pISInterface->SetSetting(MakeCampGUID, "Bearing", SET_CAMP->Bearing);
	pISInterface->SetSetting(MakeCampGUID, "ScatDist", SET_CAMP->ScatDist);
	pISInterface->SetSetting(MakeCampGUID, "ScatSize", SET_CAMP->ScatSize);

	unsigned int MoveToGUID = pISInterface->CreateSet(ISXEQMovementXML, "MoveTo");
	if (MoveToGUID == 0)
		MoveToGUID = pISInterface->FindSet(ISXEQMovementXML, "MoveTo");
	pISInterface->SetSetting(MoveToGUID, "AlwaysUW", SET_M->UW);
	pISInterface->SetSetting(MoveToGUID, "ArrivalDist", SET_M->Dist);
	pISInterface->SetSetting(MoveToGUID, "ArrivalDistX", SET_M->DistX);
	pISInterface->SetSetting(MoveToGUID, "ArrivalDistY", SET_M->DistY);
	pISInterface->SetSetting(MoveToGUID, "BreakOnAggro", SET_M->BreakAggro);
	pISInterface->SetSetting(MoveToGUID, "BreakOnHit", SET_M->BreakHit);
	pISInterface->SetSetting(MoveToGUID, "DistBackup", SET_M->DistBack);
	pISInterface->SetSetting(MoveToGUID, "MoveToMod", SET_M->Mod);
	pISInterface->SetSetting(MoveToGUID, "UseBackward", SET_M->UseBack);
	pISInterface->SetSetting(MoveToGUID, "UseWalk", SET_M->Walk);

	unsigned int CircleGUID = pISInterface->CreateSet(ISXEQMovementXML, "Circle");
	if (CircleGUID == 0)
		CircleGUID = pISInterface->FindSet(ISXEQMovementXML, "Circle");
	pISInterface->SetSetting(CircleGUID, "Backward", SET_C->Backward);
	pISInterface->SetSetting(CircleGUID, "CCW", SET_C->CCW);
	pISInterface->SetSetting(CircleGUID, "Drunken", SET_C->Drunk);
	pISInterface->SetSetting(CircleGUID, "RadiusSize", SET_C->Radius);

	unsigned int StuckLogicGUID = pISInterface->CreateSet(ISXEQMovementXML, "StuckLogic");
	if (StuckLogicGUID == 0)
		StuckLogicGUID = pISInterface->FindSet(ISXEQMovementXML, "StuckLogic");
	pISInterface->SetSetting(StuckLogicGUID, "StuckLogic", STUCK->On);
	pISInterface->SetSetting(StuckLogicGUID, "DistStuck", STUCK->Dist);
	pISInterface->SetSetting(StuckLogicGUID, "PulseCheck", (int)STUCK->Check);
	pISInterface->SetSetting(StuckLogicGUID, "PulseUnstuck", (int)STUCK->Unstuck);
	pISInterface->SetSetting(StuckLogicGUID, "TryToJump", STUCK->Jump);
	pISInterface->SetSetting(StuckLogicGUID, "TurnHalf", STUCK->TurnHalf);

	unsigned int CharGUID = pISInterface->CreateSet(ISXEQMovementXML, szCharName);
	if (CharGUID == 0)
		CharGUID = pISInterface->FindSet(ISXEQMovementXML, szCharName);

	bool szTempB = false;
	pISInterface->GetSetting(CharGUID, "DisregardMe", szTempB);
	if (SET->SaveByChar && !szTempB) {
		pISInterface->SetSetting(CharGUID, "AllowMove", SET->AllowMove);
		pISInterface->SetSetting(CharGUID, "ArcBehind", SET_S->ArcBehind);
		pISInterface->SetSetting(CharGUID, "ArcNotFront", SET_S->ArcNotFront);
		pISInterface->SetSetting(CharGUID, "AutoSave", SET->AutoSave);
		pISInterface->SetSetting(CharGUID, "AutoUW", SET->AutoUW);
		pISInterface->SetSetting(CharGUID, "DistBreak", SET_S->DistBreak);
		pISInterface->SetSetting(CharGUID, "BreakOnGate", SET_S->BreakGate);
		pISInterface->SetSetting(CharGUID, "BreakOnWarp", SET_S->BreakWarp);
		pISInterface->SetSetting(CharGUID, "PauseOnWarp", SET_S->PauseWarp);
		pISInterface->SetSetting(CharGUID, "LockPause", SET->LockPause);
		pISInterface->SetSetting(CharGUID, "DistSnaproll", SET_S->DistSnap);
		pISInterface->SetSetting(CharGUID, "FeignSupport", SET->Feign);
		pISInterface->SetSetting(CharGUID, "Heading", (SET->Head == H_TRUE) ? "true" : (SET->Head == H_LOOSE ? "loose" : "fast"));
		pISInterface->SetSetting(CharGUID, "LeashLength", SET_CAMP->Length);
		pISInterface->SetSetting(CharGUID, "UseLeash", SET_CAMP->Leash);
		pISInterface->SetSetting(CharGUID, "Verbosity", (uiVerbLevel & V_VERBOSITY) == V_VERBOSITY);
		pISInterface->SetSetting(CharGUID, "FullVerbosity", (uiVerbLevel & V_FULLVERBOSITY) == V_FULLVERBOSITY);
		pISInterface->SetSetting(CharGUID, "VerbosityFlags", uiVerbLevel);
		pISInterface->SetSetting(CharGUID, "CampRadius", SET_CAMP->Radius);
		pISInterface->SetSetting(CharGUID, "RealtimePlayer", SET_CAMP->Realtime);
		pISInterface->SetSetting(CharGUID, "UseScatter", SET_CAMP->Scatter);
		pISInterface->SetSetting(CharGUID, "Bearing", SET_CAMP->Bearing);
		pISInterface->SetSetting(CharGUID, "ScatDist", SET_CAMP->ScatDist);
		pISInterface->SetSetting(CharGUID, "ScatSize", SET_CAMP->ScatSize);
	}

	pISInterface->ExportSet(ISXEQMovementXML, XMLFileName);
}

dtNavMesh* LoadMesh() {
	DebugSpewAlways("LoadMesh()");
	static const int NAVMESHSET_MAGIC = 'M'<<24 | 'S'<<16 | 'E'<<8 | 'T';
	static const int NAVMESHSET_VERSION = 1;
	struct NavMeshSetHeader
	{
		int magic;
		int version;
		int numTiles;
		dtNavMeshParams params;
	};
	struct NavMeshTileHeader
	{
		dtTileRef tileRef;
		int dataSize;
	};
//	m_currentPathSize = 0;
//	m_currentPathCursor = 0;

	if (!GetCharInfo()) return NULL;
	char buffer[MAX_PATH];
	sprintf(buffer, "%s\\%s.bin", gszINIPath,GetShortZone(GetCharInfo()->zoneId));
	for(int i = 0; i < MAX_PATH; ++i)
		if(buffer[i] == '\\') buffer[i] = '/';
	DebugSpewAlways("LoadMesh() - opening: %s", buffer);
	FILE* fp = fopen(buffer, "rb");
	DebugSpewAlways("LoadMesh() - file pointer: %x", fp);
	if (!fp) return NULL;
	NavMeshSetHeader header;
	fread(&header, sizeof(NavMeshSetHeader), 1, fp);
	if (header.magic != NAVMESHSET_MAGIC) {
		DebugSpewAlways("LoadMesh() - NAVMESHSET_MAGIC failed");
		fclose(fp);
		return NULL;
	}
	if (header.version != NAVMESHSET_VERSION) {
		DebugSpewAlways("LoadMesh() - NAVMESHSET_VERSION failed");
		fclose(fp);
		return NULL;
	}   
	dtNavMesh* navmesh = new dtNavMesh;
	DebugSpewAlways("LoadMesh() - navmesh->init");
	if (!navmesh || !navmesh->init(&header.params))
	{
		DebugSpewAlways("LoadMesh() - navmesh->init failed");
		fclose(fp);
		return NULL;
	}      
	// Read tiles.
	DebugSpewAlways("LoadMesh() - filling tiles");
	for (int i = 0; i < header.numTiles; ++i) {
		if (0 == (i % 100)) {
			DebugSpewAlways("LoadMesh() - tile #%d", i);
		}
		NavMeshTileHeader tileHeader;
		fread(&tileHeader, sizeof(tileHeader), 1, fp);
		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;
		unsigned char* data = new unsigned char[tileHeader.dataSize];
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		fread(data, tileHeader.dataSize, 1, fp);      
		navmesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef);
	}   
	fclose(fp);
	DebugSpewAlways("LoadMesh() - success");
	return navmesh;
}