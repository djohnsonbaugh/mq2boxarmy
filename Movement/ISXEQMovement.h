/* ISXEQMovement

Plugin conversion by ILuvSEQ

This ISXClient is a conversion and merger of two MQ2 plugins: MQ2MoveUtils and MQ2Navigation

99% of this code is a direct copy of MQ2MoveUtils and MQ2Navigation.  All I've done is convert them to work with ISXEQ and merged
them into a single plugin.

Based on:
MQ2MoveUtils v11.0101
MQ2Navigation
*/

/***** NOTICE ******
Many functions included in this source code are not copyrighted to the developer and are used with permissions 
restricting their release to VIP board members of MacroQuest2.com. These functions have comments above them stating 
to whom the copyright belongs to. If you intend to redistribute this source or binaries compiled from this source 
outside of a direct link to the forum post in which it is released, you must get permission from these authors. Otherwise 
link to the forum post directly to encourage any users of this plugin to donate to the developers at MacroQuest2.com 
as required by the copyright holders of these functions, and desired by the developer. Please show your support!
****************/
#pragma once
#include <isxdk.h>
#include "math.h"
#include <vector>
#include <list>
#include <direct.h>
#include "DetourNavMesh.h"
#include "DetourCommon.h"

// uncomment these lines to enable debugspew spam
//#define DEBUGMAIN
// debugspew for stucklogic
//#define DEBUGSTUCK
// debugspew for trivial messages
//#define DEBUGMISC

// version information
const char*  MODULE_NAME    = "ISXEQMovement";
const double MODULE_VERSION = 1.2;

class ISXEQMovement :
    public ISXInterface
{
public:

    virtual bool Initialize(ISInterface *p_ISInterface);
    virtual void Shutdown();

    void LoadSettings();
    void ConnectServices();
    void RegisterCommands();
    void RegisterAliases();
    void RegisterDataTypes();
    void RegisterTopLevelObjects();
    void RegisterServices();

    void DisconnectServices();
    void UnRegisterCommands();
    void UnRegisterAliases();
    void UnRegisterDataTypes();
    void UnRegisterTopLevelObjects();
    void UnRegisterServices();

};

extern ISInterface *pISInterface;
extern HISXSERVICE hPulseService;
extern HISXSERVICE hMemoryService;
extern HISXSERVICE hHTTPService;
extern HISXSERVICE hTriggerService;
extern HISXSERVICE hSystemService;

extern HISXSERVICE hServicesService;

extern HISXSERVICE hEQChatService;
extern HISXSERVICE hEQUIService;
extern HISXSERVICE hEQGamestateService;
extern HISXSERVICE hEQSpawnService;
extern HISXSERVICE hEQZoneService;

extern ISXEQMovement *pExtension;
#define printf pISInterface->Printf

#define EzDetour(Address, Detour, Trampoline) IS_Detour(pExtension,pISInterface,hMemoryService,(unsigned int)Address,Detour,Trampoline)
#define EzUnDetour(Address) IS_UnDetour(pExtension,pISInterface,hMemoryService,(unsigned int)Address)
#define EzDetourAPI(_Detour_,_DLLName_,_FunctionName_,_FunctionOrdinal_) IS_DetourAPI(pExtension,pISInterface,hMemoryService,_Detour_,_DLLName_,_FunctionName_,_FunctionOrdinal_)
#define EzUnDetourAPI(Address) IS_UnDetourAPI(pExtension,pISInterface,hMemoryService,(unsigned int)Address)

#define EzModify(Address,NewData,Length,Reverse) Memory_Modify(pExtension,pISInterface,hMemoryService,(unsigned int)Address,NewData,Length,Reverse)
#define EzUnModify(Address) Memory_UnModify(pExtension,pISInterface,hMemoryService,(unsigned int)Address)

#define EzHttpRequest(_URL_,_pData_) IS_HttpRequest(pExtension,pISInterface,hHTTPService,_URL_,_pData_)

#define EzAddTrigger(Text,Callback,pUserData) IS_AddTrigger(pExtension,pISInterface,hTriggerService,Text,Callback,pUserData)
#define EzRemoveTrigger(ID) IS_RemoveTrigger(pExtension,pISInterface,hTriggerService,ID)
#define EzCheckTriggers(Text) IS_CheckTriggers(pExtension,pISInterface,hTriggerService,Text)

static LONG EzCrashFilter(_EXCEPTION_POINTERS *pExceptionInfo,const char *szIdentifier,...)
{
	unsigned int Code=pExceptionInfo->ExceptionRecord->ExceptionCode;
	if (Code==EXCEPTION_BREAKPOINT || Code==EXCEPTION_SINGLE_STEP)
		return EXCEPTION_CONTINUE_SEARCH;

	char szOutput[4096];
	szOutput[0]=0;
    va_list vaList;

    va_start( vaList, szIdentifier );
    vsprintf(szOutput,szIdentifier, vaList);

	IS_SystemCrashLog(pExtension,pISInterface,hSystemService,pExceptionInfo,szOutput);

	return EXCEPTION_EXECUTE_HANDLER;
}

extern LSType *pStringType;
extern LSType *pIntType;
extern LSType *pUintType;
extern LSType *pBoolType;
extern LSType *pFloatType;
extern LSType *pTimeType;
extern LSType *pByteType;
extern LSType *pIntPtrType;
extern LSType *pBoolPtrType;
extern LSType *pFloatPtrType;
extern LSType *pBytePtrType;


// ------------------------------------------------------------------------------
// constants - the unsigned chars are aesthetics but we use different values
// so that nothing can be called incorrectly and erroneously match a switch case
// ------------------------------------------------------------------------------
// check calling command
const unsigned char CMD_STICK       = 1;
const unsigned char CMD_MOVETO      = 2;
const unsigned char CMD_CIRCLE      = 3;
const unsigned char CMD_MAKECAMP    = 4;
const unsigned char CMD_FOLLOW		= 5;
const unsigned char CMD_PLAY		= 6;
const unsigned char CMD_RECORD		= 7;
// walk handling
const unsigned char MU_WALKON       = 10;
const unsigned char MU_WALKOFF      = 11;
const unsigned char MU_WALKIGNORE   = 12;
// reset altcamp or make new altcamp
const unsigned char SET_ALT         = 20;
const unsigned char RESET_ALT       = 21;
// direction to move
const unsigned char GO_FORWARD      = 30;
const unsigned char GO_BACKWARD     = 31;
const unsigned char GO_LEFT         = 32;
const unsigned char GO_RIGHT        = 33;
const unsigned char KILL_STRAFE     = 34;
const unsigned char KILL_FB         = 35;
const unsigned char APPLY_TO_ALL    = 36;
// help output
const unsigned char HELP_SETTINGS   = 50;
// error messages
const unsigned char ERR_STICKSELF   = 60;
const unsigned char ERR_STICKNONE   = 61;
const unsigned char ERR_BADMOVETO   = 62;
const unsigned char ERR_BADMAKECAMP = 63;
const unsigned char ERR_BADCIRCLE   = 64;
const unsigned char ERR_BADSPAWN    = 65;
const unsigned char ERR_BADDELAY    = 66;
// debug output
const unsigned char DBG_MAIN        = 200;
const unsigned char DBG_STUCK       = 201;
const unsigned char DBG_MISC        = 202;
const unsigned char DBG_DISABLE     = 203;

// ------------------------------------------
// formulas & randomization
const float CIRCLE_QUARTER          = 90.0f;
const float CIRCLE_HALF             = 180.0f;
const float CIRCLE_MAX              = 360.0f;
const float HEADING_QUARTER         = 128.0f;
const float HEADING_HALF            = 256.0f;
const float HEADING_MAX             = 512.0f;
const int   BEHIND_ARC              = 45;
const int   FRONT_ARC               = 240;
const int   NOT_FRONT_ARC           = 135;
const int   PIN_ARC_MIN             = 112;
const int   PIN_ARC_MAX             = 144;
// initialization
const float H_INACTIVE              = 10000.0f;
const char  H_FAST                  = 0;
const char  H_LOOSE                 = 1;
const char  H_TRUE                  = 2;
const char  T_INACTIVE              = 0;
const char  T_WAITING               = 1;
const char  T_READY                 = 2;
// stucklogic ring size
const int   MAXRINGSIZE             = 32; // MovingAvg max pulses to average

// Navigation constants
const int MAX_POLYS = 4028;
const int WAYPOINT_PROGRESSION_DISTANCE = 5; // minimum distance to a waypoint for moving to the next waypoint
const int ENDPOINT_STOP_DISTANCE = 15; // stoping distance at the final way point
const int PATHFINDING_DELAY = 2000; // how often to update the path (milliseconds)

// ------------------------------------------
// class instances
class CMUSettings*     SET      = NULL;
class CMUActive*       pMU      = NULL;
class CMUCharacter*    ME       = NULL;
class CMUMovement*     MOVE     = NULL;
class CStickCmd*       STICK    = NULL;
class CMoveToCmd*      MOVETO   = NULL;
class CCircleCmd*      CIRCLE   = NULL;
class CStuckLogic*     STUCK    = NULL;
class CCampHandler*    CAMP     = NULL;
class CAltCamp*        ALTCAMP  = NULL;
class CCampCmd*        CURCAMP  = NULL;
class CPauseHandler*   PAUSE    = NULL;
class CMULoc*          SUMMON   = NULL;
class CCircleSettings* SET_C    = NULL;
class CMoveToSettings* SET_M    = NULL;
class CCampSettings*   SET_CAMP = NULL;
class CStickSettings*  SET_S    = NULL;

// ---------------------------------
// verbosity bit flags
enum VERBLEVEL {
	V_SILENCE       = 0,
	V_AUTOPAUSE     = 1,
	V_MOVEPAUSE     = 2,
	V_MOUSEPAUSE    = 4,
	V_FEIGN         = 8,
	V_HIDEHELP      = 16,
	V_STICKV        = 32,
	V_STICKFV       = 64,
	V_MOVETOV       = 128,
	V_MOVETOFV      = 256,
	V_MAKECAMPV     = 512,
	V_MAKECAMPFV    = 1024,
	V_CIRCLEV       = 2048,
	V_CIRCLEFV      = 4096,
	V_SETTINGS      = 8192,
	V_SAVED         = 16384,
	V_BREAKONWARP   = 32768,
	V_BREAKONAGGRO  = 65536,
	V_BREAKONHIT    = 131072,
	V_BREAKONSUMMON = 262144,
	V_BREAKONGM     = 524288,
	V_BREAKONGATE   = 1048576,
	V_STICKALWAYS   = 2097152,
	V_ERRORS        = 4194304,
	V_RANDOMIZE     = 8388608,
	V_PAUSED        = 16777216,
	V_VERBOSITY     = 2720, // normal verbosity msgs
	V_FULLVERBOSITY = 11736390, // full verbosity msgs
	V_EVERYTHING    = 33554431, // all messages on (dont add verb + fullverb in)
};
unsigned int uiVerbLevel   = V_EVERYTHING;
unsigned int uiRetainFlags = V_EVERYTHING; // stores flags for when totalsilence toggle

// -----------------------
// strings

char szMsg[MAX_STRING]         = {0};                // use for generic msg output
char szDebugName[MAX_STRING]   = {0};                // debug file name
char szCharName[MAX_STRING]    = {0};                // stores char name for INI read/write
const char szOn[10]            = "\agON\ax";         // used in outputs
const char szOff[10]           = "\arOFF\ax";        // used in outputs
const char szArriveMove[50]    = "/moveto location"; // output moveto arrival
const char szArriveCamp[50]    = "camp from /makecamp return"; // output camp return arrival
const char szArriveAlt[50]     = "camp from /makecamp altreturn"; // output altcamp return arrival

// ----------------------------------------
// events

unsigned int Event_AggroNorm   = NULL;
unsigned int Event_MissNorm    = NULL;
unsigned int Event_AggroAbbrev = NULL;
unsigned int Event_MissAbbrev  = NULL;
unsigned int Event_MissNumOnly = NULL;
unsigned int Event_Gates       = NULL;

// ----------------------------------------
// key IDs & pointers

int iAutoRun     = NULL;
unsigned long* pulAutoRun     = NULL;
int iForward     = NULL;
unsigned long* pulForward     = NULL;
int iBackward    = NULL;
unsigned long* pulBackward    = NULL;
int iTurnLeft    = NULL;
unsigned long* pulTurnLeft    = NULL;
int iTurnRight   = NULL;
unsigned long* pulTurnRight   = NULL;
int iStrafeLeft  = NULL;
unsigned long* pulStrafeLeft  = NULL;
int iStrafeRight = NULL;
unsigned long* pulStrafeRight = NULL;

int iJumpKey     = NULL;
int iRunWalk     = NULL;
int iDuckKey     = NULL;

char* szFailedLoad[] = {
	"No Error",            // 0
	"TurnRight Address",   // 1
	"TurnRight",           // 2
	"StafeLeft",           // 3
	"StrafeRight",         // 4
	"AutoRun",             // 5
	"TurnLeft",            // 6
	"MoveForward Address", // 7
	"Forward",             // 8
	"AutoRun Mismatch",    // 9
	"Backward"             // 10
};
unsigned long addrTurnRight     = NULL;
PBYTE patternTurnRight          = (PBYTE)"\xA3\x00\x00\x00\x00\x89\x1D\x00\x00\x00\x00\x89\x1D\x00\x00\x00"
	"\x00\x3B\x00\x0F\x84\x00\x00\x00\x00\xF6\x05\x00\x00\x00\x00\x00\x74\x00"
	"\x89\x1D\x00\x00\x00\x00\x89\x1D";
char maskTurnRight[]            = "x????xx????xx????x?xx????xx?????x?xx????xx";
// A3 ? ? ? ? 89 1D ? ? ? ? 89 1D ? ? ? ? 3B ? 0F 84 ? ? ? ? F6 05 ? ? ? ? ? 74 ? 89 1D ? ? ? ? 89 1D
unsigned long addrMoveForward   = NULL;
PBYTE patternMoveForward        = (PBYTE)"\xA3\x00\x00\x00\x00\x3B\x00\x0F\x84\x00\x00\x00\x00\xF6\x05\x00\x00\x00"
	"\x00\x00\x74\x00\x89\x1D";
char maskMoveForward[]          = "x????x?xx????xx?????x?xx";
// A3 ? ? ? ? 3B ? 0F 84 ? ? ? ? F6 05 ? ? ? ? ? 74 ? 89 1D
bool bOffsetOverride            = false;

// Function declarations
void UndoKeybinds();
void SetupEvents(bool bAddEvent, bool bForceRemove);
int HandleCommand(int argc, char *argv[]);
int CalcOurAngle(int argc, char *argv[]);
int RootCmd(int argc, char *argv[]);
void SaveConfig();
void LoadConfig();

// ----------------------------------------
// ************* CLASSES *****************
// ----------------------------------------

// ----------------------
// inherit-only classes

class CMULoc
{
	// locations & dist comparisons
public:
	float Y;
	float X;
	float Z;
	float CurDist;
	float DifDist;

	CMULoc()
	{
		Y       = 0.0f;
		X       = 0.0f;
		Z       = 0.0f;
		CurDist = 0.0f;
		DifDist = 0.0f;
	};
};

class CMUDelay
{
	// delay & time calculations
public:
	int Min;
	int Max;

	void TimeStop();
	void TimeStart();
	char TimeStatus();
	void Validate();
	void MinDelay(int iNew);
	void MaxDelay(int iNew);

	CMUDelay()
	{
		Min    = 0;
		Max    = 0;
		Resume = T_INACTIVE;
	};

protected:
	int        Resume; // calculated resume time
	SYSTEMTIME Began;  // timer start

	int ElapsedMS();
};

// ----------------------------------------
// character functions
class CMUCharacter
{
public:
	bool IsBard();
	bool InCombat();
	bool IsMe(PSPAWNINFO pCheck);
};

// ----------------------------------------------------------
// configuration classes - store default & INI-saved settings

class CStuckLogic : public CMULoc
{
public:
	bool         On;       // INI: stucklogic active or off
	bool         Jump;     // INI: if true, try to jump when stuck
	bool         TurnHalf; // INI: if true, reset heading and try other dir if gone halfway without freeing
	unsigned int Check;    // INI: # of pulses to average distance for stuck awareness
	unsigned int Unstuck;  // INI: if StuckDec == this, consider unstuck
	float        Dist;     // INI: dist needed to move else considered stuck, compared against pulse average
	float        TurnSize; // Turn increment value for left/right (sign flipped by TurnHalf)
	unsigned int StuckInc; // increments each pulse we haven't moved beyond Dist until we reach Check value which == we are stuck
	unsigned int StuckDec; // increments each pulse we've moved again after being stuck, when Unstuck = StuckDec, we force unstuck

	void Reset();

	CStuckLogic()
	{
		// plugin defaults established here
		On       = true;
		Jump     = false;
		TurnHalf = true;
		TurnSize = 10.0f;
		Dist     = 0.1f;
		Check    = 6;
		Unstuck  = 10;
		StuckInc = 0;
		StuckDec = 0;
		CurDist  = 1.0f; // baseline to not trigger when first starting movement
		// Y X Z DifDist already initialized by CMULoc() inherit
	};
};

class CStickSettings : public CMUDelay
{
public:
	bool  BreakGate;    // INI: stick breaks if "mob_name Gates." message
	bool  BreakTarget;  // INI: stick breaks if target switched
	bool  BreakWarp;    // INI: stick breaks if target warps out of DistBreak
	bool  PauseWarp;    // INI: stick pauses if target warps out of DistBreak
	bool  Randomize;    // INI: randomize strafe arcs during stick
	bool  DelayStrafe;  // INI: strafe sticks use a delay timer with TimedStrafe()
	bool  UseBack;      // INI: use backwards walking when close to target
	bool  UseFleeing;   // INI: 'front' will not strafe if target is fleeing
	bool  Walk;         // INI: stick walks when strafing
	bool  UW;           // look angle up or down at target (/stick uw)
	float ArcBehind;    // INI: arc size for stick behind
	float ArcNotFront;  // INI: arc size for stick !front
	float DistBack;     // INI: within this dist UseBack handles positioning if enabled
	float DistBreak;    // INI: target warps this distance, BreakWarp triggers if enabled
	float DistMod;      // INI: raw modifier to Dist (CStickCmd) (best used if plugin is auto-setting dist)
	float DistModP;     // INI: % modifier to Dist (CStickCmd) (best used if plugin is auto-setting dist)
	float DistSnap;     // INI: default distance from target to snaproll

	CStickSettings()
	{
		// plugin defaults, established here
		Min         = 1500; // inherit: CMUDelay (for strafe)
		Max         = 3000; // inherit: CMUDelay (for strafe)
		BreakGate   = true;
		BreakTarget = false;
		BreakWarp   = true;
		PauseWarp   = false;
		Randomize   = false;
		DelayStrafe = true;
		UseBack     = true;
		UseFleeing  = true;
		Walk        = false;
		UW          = false;
		ArcBehind   = BEHIND_ARC;
		ArcNotFront = NOT_FRONT_ARC;
		DistBack    = 10.0f;
		DistBreak   = 250.0f;
		DistMod     = 0.0f;
		DistModP    = 1.0f;
		DistSnap    = 10.0f;
	};
};

class CCampSettings : public CMUDelay
{
public:
	bool  HaveTarget; // INI: if true, auto camp return even with a target
	bool  NoAggro;    // INI: if true, auto camp return only if not aggro
	bool  NotLoot;    // INI: if true, auto camp return only if not looting
	bool  Scatter;    // INI: camp return scattering active or off
	bool  Realtime;   // INI: makecamp player updates pc anchor Y/X while returning
	bool  Leash;      // INI: camp leashing active or off
	float Bearing;    // INI: bearing for camp return scattering
	float Length;     // INI: length of leash checked against anchor
	float Radius;     // INI: default camp radius size
	float ScatSize;   // INI: camp return scatter radius size
	float ScatDist;   // INI: dist from anchor for camp return scattering

	CCampSettings()
	{
		Min        = 500;  // inherit: CMUDelay
		Max        = 1500; // inherit: CMUDelay
		HaveTarget = false;
		NoAggro    = false;
		NotLoot    = false;
		Scatter    = false;
		Realtime   = false;
		Leash      = false;
		Bearing    = 0.0f;
		Length     = 50.0f;
		Radius     = 40.0f;
		ScatSize   = 10.0f;
		ScatDist   = 10.0f;
	};

	void SetRadius(float fNew);
	void SetLeash(float fNew);

protected:
	void ValidateSizes();
};

class CMoveToSettings
{
public:
	bool  BreakAggro; // INI: break moveto if aggro gained
	bool  BreakHit;   // INI: break moveto if attacked (blech event)
	bool  UseBack;    // INI: use backwards walking when initially close to destination
	bool  UW;         // INI: moveto uses UW face angle adjustments
	bool  Walk;       // INI: moveto walks if close to arrivaldist
	float Dist;       // INI: how close to moveto location is considered acceptable
	float DistBack;   // INI: within this dist UseBack handles positioning if enabled
	float DistY;      // INI: how close to moveto Y location is acceptable for precisey
	float DistX;      // INI: how close to moveto X location is acceptable for precisex
	float Mod;        // INI: Dist percent modifier

	CMoveToSettings()
	{
		BreakAggro = false;
		BreakHit   = false;
		UseBack    = false;
		UW         = false;
		Walk       = true;
		Dist       = 10.0f;
		DistBack   = 30.0f;
		DistY      = 10.0f;
		DistX      = 10.0f;
		Mod        = 0.0f;
	};
};

class CCircleSettings
{
public:
	bool  Backward; // INI: always backwards
	bool  CCW;      // INI: always counter-clockwise
	bool  Drunk;    // INI: always drunken
	float CMod;     // INI: default radius percent modifer
	float Radius;   // INI: default radius size

	CCircleSettings()
	{
		Backward = false;
		CCW      = false;
		Drunk    = false;
		CMod     = 0.0f;
		Radius   = 30.0f;
	};

	void SetRadius(float fNew);
};

class CMUSettings
{
public:
	bool  AutoSave;    // INI: autosave ini file when using 'toggle' or 'set'
	bool  AutoPause;   // INI: pause automatically when casting/stunned/self targeted/sitting
	bool  AutoUW;      // INI: automatically use 'uw' when underwater
	bool  BreakGM;     // INI: command breaks if visible GM enters zone
	bool  BreakSummon; // INI: command breaks if you move too far in a single pulse (summoned)
	bool  BreakKB;     // INI: break command if movement key pressed
	bool  PauseKB;     // INI: pause command if movement key pressed
	bool  BreakMouse;  // INI: break command if mouselook active
	bool  PauseMouse;  // INI: pause command if mouselook active
	bool  Feign;       // INI: do not stand if currently FD
	bool  LockPause;   // INI: pause will not reset until unpaused
	bool  SaveByChar;  // INI: save some settings for individual characters
	bool  Spin;        // INI: if true, stick front ignores requiring being on HoTT
	bool  WinEQ;       // INI: use old-style movement
	float AllowMove;   // INI: distance to allow forward movement while turning, CanLooseMove()
	float DistSummon;  // INI: distance your character moves in a single pulse to trigger BreakSummon
	float TurnRate;    // INI: rate at which to turn using loose heading (14 is default)
	int   Head;        // INI: heading adjustment type (0 = fast [H_FAST], 1 = loose [H_LOOSE], 2 = true [H_TRUE])

	CMUSettings()
	{
		SET_CAMP = new CCampSettings();
		SET_S    = new CStickSettings();
		SET_M    = new CMoveToSettings();
		SET_C    = new CCircleSettings();

		AutoSave    = true;
		AutoPause   = true;
		AutoUW      = false;
		BreakGM     = true;
		BreakSummon = false;
		BreakKB     = true;
		PauseKB     = false;
		BreakMouse  = false;
		PauseMouse  = false;
		Feign       = false;
		LockPause   = false;
		SaveByChar  = true;
		Spin        = false;
		WinEQ       = false;
		AllowMove   = 32.0f;
		DistSummon  = 8.0f;
		TurnRate    = 14.0f;
		Head        = H_TRUE;
	};

	~CMUSettings()
	{
		delete SET_CAMP;
		SET_CAMP = NULL;
		delete SET_S;
		SET_S    = NULL;
		delete SET_M;
		SET_M    = NULL;
		delete SET_C;
		SET_C    = NULL;
	};
};
// ---------------------------------------------------
// command classes - instanced for individual cmd use

class CCircleCmd : public CMULoc, public CMUDelay, public CCircleSettings
{
public:
	bool On; // circling active or off

	bool Wait();
	void AtMe();
	void AtLoc(float fY, float fX);

	CCircleCmd() // ooo: CMULoc(), CMUDelay(), CCircleSettings(), CCircleCmd()
	{
		Min = 600;      // inherit: CMUDelay
		Max = 900;      // inherit: CMUDelay
		On  = false;
		UserDefaults(); // copy defaults from INI settings
		TimeStart();
	};

protected:
	void UserDefaults();
	int GetDrunk(int iNum);
};

class CMoveToCmd : public CMULoc, public CMoveToSettings
{
public:
	bool  On;       // moveto active or off (MOVETO->On)
	bool  UsingPath; // is moveto using a path of locations to get somewhere instead of a single loc
	bool  PreciseY; // moveto arrivaldist is only checked against Y
	bool  PreciseX; // moveto arrivaldist is only checked against X

	bool DidAggro();
	float currentPath[MAX_POLYS*3];
	int currentPathCursor;
	int currentPathSize;

	CMoveToCmd()
	{
		On       = false;
		UsingPath= false;
		PreciseY = false;
		PreciseX = false;
		UserDefaults();

		currentPathCursor = 0;
		currentPathSize = 0;
	};

	void Activate(float fY, float fX, float fZ);
	int FindPath(float X, float Y, float Z, float* pPath);

protected:
	void UserDefaults();
};

class CCampCmd : public CMULoc, public CCampSettings
{
public:
	bool          On;                  // camp active or off
	bool          Pc;                  // makecamp player on or off
	bool          RedoStick;           // set during camp return
	bool          RedoCircle;          // set during camp return
	unsigned long PcID;                // stores id of makecamp player
	eSpawnType    PcType;              // stores spawn type of makecamp player
	char          PcName[MAX_STRING];  // stores makecamp player displayedname

	CCampCmd()
	{
		On         = false;
		Pc         = false;
		RedoStick  = false;
		RedoCircle = false;
		PcID       = 0;
		PcType     = NONE;
		memset(&PcName, 0, MAX_STRING);
		UserDefaults();
	};

protected:
	void UserDefaults()
	{
		Min        = SET_CAMP->Min;
		Max        = SET_CAMP->Max;
		HaveTarget = SET_CAMP->HaveTarget;
		NoAggro    = SET_CAMP->NoAggro;
		NotLoot    = SET_CAMP->NotLoot;
		Scatter    = SET_CAMP->Scatter;
		Realtime   = SET_CAMP->Realtime;
		Leash      = SET_CAMP->Leash;
		Bearing    = SET_CAMP->Bearing;
		Length     = SET_CAMP->Length;
		Radius     = SET_CAMP->Radius;
		ScatSize   = SET_CAMP->ScatSize;
		ScatDist   = SET_CAMP->ScatDist;
	};
};

class CAltCamp : public CMULoc
{
public:
	bool  On;
	float Radius;

	void Update(CCampCmd* Cur);

	CAltCamp()
	{
		On     = false;
		Radius = 0.0f;
	};
};

class CCampHandler : public CMUDelay, public CMULoc
{
public:
	bool Auto;      // state awareness (is camp return auto or manual)
	bool DoAlt;     // true when manually forcing a return to altcamp
	bool DoReturn;  // true when manually forcing a return to curcamp
	bool Returning; // true when any return is active (both auto and manual)

	void ResetBoth();
	void ResetCamp(bool bOutput);
	void ResetPlayer(bool bOutput);
	void NewCamp(bool bOutput);
	void Activate(float fY, float fX);
	void ActivatePC(PSPAWNINFO pCPlayer);
	void VarReset();

	CCampHandler()
	{
		ALTCAMP   = new CAltCamp();
		CURCAMP   = new CCampCmd();
		Min       = SET_CAMP->Min;
		Max       = SET_CAMP->Max;
		VarReset();
	};

	~CCampHandler()
	{
		delete ALTCAMP;
		ALTCAMP = NULL;
		delete CURCAMP;
		CURCAMP = NULL;
	};

protected:
	void Output();
	void OutputPC();
};

class CSnaproll : public CMULoc
{
public:
	float Head;    // heading to snaproll
	float Bearing; // bearing of target to snaproll to

	CSnaproll()
	{
		Head    = 0.0f;
		Bearing = HEADING_HALF;
	};
};

class CStickCmd : public CMULoc, public CStickSettings
{
public:
	CSnaproll* Snap;  // instance a snaproll
	// active stick settings
	bool  SetDist;    // has dist to mob has been set
	float Dist;       // defaults to melee range or set to 10 by /stick 10 for example
	float RandMin;    // min arc for strafe (if randomize enabled)
	float RandMax;    // max arc for strafe (if randomize enabled)
	bool  RandFlag;   // flag to randomly randomize min or max
	// active stick type
	bool  MoveBack;   // maintains distance (if too close, back up)
	bool  Behind;     // uses rear arc of target
	bool  BehindOnce; // moves to rear arc of target then position is ignored
	bool  Front;      // uses front arc of target
	bool  NotFront;   // uses anywhere but the front arc of target (/stick !front)
	bool  Pin;        // uses left or right arc of the target
	bool  Snaproll;   // snaprolls to a fixed loc (polarmath) instead of strafing
	bool  Hold;       // maintains sticking to previous target if it changes
	bool  Healer;     // healer type (non-constant facing)
	bool  Always;     // restick when new target acquired (wont work with hold) (/stick __ always)
	bool  HaveTarget; // true when Always true and have a target
	bool  Strafe;     // true when using a cmd that will strafe other than 'front'
	bool  On;         // stick active or off, set this only through TurnOn()
	// spawn information
	unsigned long LastID;    // compare target change OnPulse
	unsigned long HoldID;    // spawn id for stick hold
	eSpawnType    LastType;  // if target changes to a corpse, stop sticking
	eSpawnType    HoldType;  // if stick hold changes to a corpse, stop sticking

	void TurnOn();
	void StopHold();
	void FirstAlways();
	void NewSnaproll();
	void ResetLoc();
	bool Ready();
	void DoRandomize();

	CStickCmd()
	{
		Snap        = new CSnaproll();
		SetDist     = false;
		Dist        = 0.0f;
		RandMin     = 0.0f;
		RandMax     = 0.0f;
		RandFlag    = true;
		MoveBack    = false;
		Behind      = false;
		BehindOnce  = false;
		Front       = false;
		NotFront    = false;
		Pin         = false;
		Snaproll    = false;
		Hold        = false;
		Healer      = false;
		Always      = false;
		HaveTarget  = false;
		Strafe      = false;
		On          = false;
		LastID      = 0;
		HoldID      = 0;
		LastType    = NONE;
		HoldType    = NONE;

		AlwaysReady = true;
		UserDefaults();
	};

	~CStickCmd()
	{
		delete Snap;
		Snap = NULL;
	};

protected:
	void UserDefaults();
	void SetRandArc(int iArcType);
	bool AlwaysStatus();

	bool AlwaysReady;
};

// ------------------------------------------
class CPauseHandler : public CMUDelay
{
public:
	bool PausedCmd; // pauses via command (not overwritten until 'unpause' or off/new cmd)
	bool PausedMU;  // pauses all operations (used by many operations)
	bool UserKB;    // true while movement keybinds are pressed
	bool UserMouse; // true while mouselook held

	bool Waiting();
	void HandlePause();
	void PauseTimers();

	void MouseFree();
	bool PauseNeeded();
	bool MouseCheck();
	void Reset();

	CPauseHandler()
	{
		Min         = 500;
		Max         = 5000;
		PausedCmd   = PausedMU  = false;
		UserKB      = UserMouse = false;
		HandleMouse = HandleKB  = false;
	};
protected:
	bool HandleMouse;
	bool HandleKB;
};

class CMUActive
{
	// this class instances everything to be used during plugin
	// runtime under pMU. cmd instances are deleted and reinstanced
	// per use, copying their defaults from SET, inline params applied after
public:
	bool  Aggro;         // ${MoveUtils.Aggro}
	bool  Bard;          // true if a bard (used by autopause)
	bool  BrokeGM;       // true if BreakOnGM fired
	bool  BrokeSummon;   // true if BreakOnSummon fired
	bool  CmdFwd;        // awareness if plugin has attempted to move forward
	bool  CmdStrafe;     // awareness if plugin has attempted to strafe
	bool  FixWalk;       // true if we are dead, unwalk in the event we rez
	bool  Keybinds;      // loaded keybinds or not
	bool  KeyKilled;     // keybind has stopped movement
	bool  Loaded;        // auto ini load
	bool  LockPause;     // pause locked until unpause
	bool  MovetoBroke;   // Moveto BreakOnHit or BreakOnAggro fired
	bool  Rooted;        // rootme command active
	bool  StoppedMoveto; // MoveTo.Stopped, last cmd arrived success
	int   Head;          // current heading type (0=fast, 1=loose, 2=true)
	dtNavMesh* navMesh;

	void NewStick()
	{
		CURCAMP->RedoStick = false;
		delete STICK;
		STICK = new CStickCmd();
	};

	void NewMoveTo()
	{
		CAMP->VarReset();
		delete MOVETO;
		MOVETO = new CMoveToCmd();
	};

	void NewCircle()
	{
		CURCAMP->RedoCircle = false;
		delete CIRCLE;
		CIRCLE = new CCircleCmd();
	};

	void NewCmds()
	{
		NewStick();
		NewMoveTo();
		NewCircle();
	};

	void NewSummon()
	{
		delete SUMMON;
		SUMMON = new CMULoc();
	};

	void NewDefaults()
	{
		NewCmds();
		NewSummon();
		STUCK->Reset();
		CAMP->ResetBoth();
		Defaults();
	};

	bool Active()
	{
		if (STICK->On || MOVETO->On || CIRCLE->On || CAMP->Returning ) // || FOLLOW->On || PLAY->On || RECORD->On)
		{
			return true;
		}
		return false;
	};

	void AggroTLO();

	bool Broken()
	{
		if (BrokeGM || BrokeSummon) return true;
		return false;
	};

	CMUActive()
	{
		STICK  = new CStickCmd();
		MOVETO = new CMoveToCmd();
		CIRCLE = new CCircleCmd();
		CAMP   = new CCampHandler();
		PAUSE  = new CPauseHandler();
		STUCK  = new CStuckLogic();
		SUMMON = new CMULoc();

		Aggro         = false;
		Bard          = false;
		BrokeGM       = false;
		BrokeSummon   = false;
		CmdFwd        = false;
		CmdStrafe     = false;
		FixWalk       = false;
		Keybinds      = false;
		KeyKilled     = false;
		Loaded        = false;
		LockPause     = false;
		MovetoBroke   = false;
		Rooted        = false;
		StoppedMoveto = false;
		Head          = H_TRUE;
		Defaults();
	};

	~CMUActive()
	{
		delete STICK;
		STICK  = NULL;
		delete MOVETO;
		MOVETO = NULL;
		delete CIRCLE;
		CIRCLE = NULL;
		delete CAMP;
		CAMP   = NULL;
		delete PAUSE;
		PAUSE  = NULL;
		delete STUCK;
		STUCK  = NULL;
		delete SUMMON;
		SUMMON = NULL;
	};

	void Defaults()
	{
		Head      = SET->Head;
		LockPause = SET->LockPause;
	};
};

// --------------------------------
// Begin Custom Top-Level Objects

class IS_MakeCampType : public LSTypeDefinition
{
public:
	static enum MakeCampMembers
	{
		Status           = 1,
		Leash            = 2,
		AnchorY          = 3,
		AnchorX          = 4,
		LeashLength      = 5,
		CampRadius       = 6,
		MinDelay         = 7,
		MaxDelay         = 8,
		Returning        = 9,
		AltAnchorY       = 10,
		AltAnchorX       = 11,
		CampDist         = 12,
		AltCampDist      = 13,
		AltRadius        = 14,
		Scatter          = 15,
		ReturnNoAggro    = 16,
		ReturnNotLooting = 17,
		ReturnHaveTarget = 18,
		Bearing          = 19,
		ScatDist         = 20,
		ScatSize         = 21,
	};

	IS_MakeCampType():LSTypeDefinition("makecamp")
	{
		TypeMember(Status);
		TypeMember(Leash);
		TypeMember(AnchorY);
		TypeMember(AnchorX);
		TypeMember(LeashLength);
		TypeMember(CampRadius);
		TypeMember(MinDelay);
		TypeMember(MaxDelay);
		TypeMember(Returning);
		TypeMember(AltAnchorY);
		TypeMember(AltAnchorX);
		TypeMember(CampDist);
		TypeMember(AltCampDist);
		TypeMember(AltRadius);
		TypeMember(Scatter);
		TypeMember(ReturnNoAggro);
		TypeMember(ReturnNotLooting);
		TypeMember(ReturnHaveTarget);
		TypeMember(Bearing);
		TypeMember(ScatDist);
		TypeMember(ScatSize);
	};
	bool GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest);

	bool ToString(LSVARPTR VarPtr, char* Destination)
	{
		strcpy(Destination, "OFF");
		if (PAUSE->PausedMU || PAUSE->PausedCmd)
		{
			strcpy(Destination, "PAUSED");
		}
		else if (CURCAMP->On)
		{
			strcpy(Destination, "ON");
		}
		return true;
	}
};

class IS_StickType : public LSTypeDefinition
{
public:
	enum StickMembers
	{
		Status          = 1,
		Active          = 2,
		Distance        = 3,
		MoveBehind      = 4,
		MoveBack        = 5,
		Loose           = 6,
		Paused          = 7,
		Behind          = 8,
		Stopped         = 9,
		Pin             = 10,
		StickTarget     = 11,
		StickTargetName = 12,
		DistMod         = 13,
		DistModPercent  = 14,
		Always          = 15,
	};

	IS_StickType():LSTypeDefinition("stick")
	{
		TypeMember(Status);
		TypeMember(Active);
		TypeMember(Distance);
		TypeMember(MoveBehind);
		TypeMember(MoveBack);
		TypeMember(Loose);
		TypeMember(Paused);
		TypeMember(Behind);
		TypeMember(Stopped);
		TypeMember(Pin);
		TypeMember(StickTarget);
		TypeMember(StickTargetName);
		TypeMember(DistMod);
		TypeMember(DistModPercent);
		TypeMember(Always);
	}

	bool GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest);

	bool ToString(LSVARPTR VarPtr, char* Destination)
	{
		strcpy(Destination, "OFF");
		if (PAUSE->PausedMU || PAUSE->PausedCmd)
		{
			strcpy(Destination, "PAUSED");
		}
		else if (STICK->On)
		{
			strcpy(Destination, "ON");
		}

		return true;
	}
};

class IS_MoveToType : public LSTypeDefinition
{
public:
	enum MoveToMembers
	{
		Moving       = 1,
		Stopped      = 2,
		CampStopped  = 3,
		UseWalk      = 4,
		ArrivalDist  = 5,
		ArrivalDistY = 6,
		ArrivalDistX = 7,
		Broken       = 8,
	};

	IS_MoveToType():LSTypeDefinition("moveto")
	{
		TypeMember(Moving);
		TypeMember(Stopped);
		TypeMember(CampStopped);
		TypeMember(UseWalk);
		TypeMember(ArrivalDist);
		TypeMember(ArrivalDistY);
		TypeMember(ArrivalDistX);
		TypeMember(Broken);
	}

	bool GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest);

	bool ToString(LSVARPTR VarPtr, char* Destination)
	{
		strcpy(Destination, "OFF");
		if (PAUSE->PausedMU || PAUSE->PausedCmd)
		{
			strcpy(Destination, "PAUSED");
		}
		else if (MOVETO->On)
		{
			strcpy(Destination, "ON");
		}

		return true;
	}
};

class IS_CircleType : public LSTypeDefinition
{
public:
	enum CircleMembers
	{
		Status    = 1,
		CircleY   = 2,
		CircleX   = 3,
		Drunken   = 4,
		Rotation  = 5,
		Direction = 6,
		Clockwise = 7,
		Backwards = 8,
		Radius    = 9,
	};

	IS_CircleType():LSTypeDefinition("circle")
	{
		TypeMember(Status);
		TypeMember(CircleY);
		TypeMember(CircleX);
		TypeMember(Drunken);
		TypeMember(Rotation);
		TypeMember(Direction);
		TypeMember(Clockwise);
		TypeMember(Backwards);
		TypeMember(Radius);
	}

	bool GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest);

	bool ToString(LSVARPTR VarPtr, char* Destination)
	{
		strcpy(Destination, "OFF");
		if (PAUSE->PausedMU || PAUSE->PausedCmd)
		{
			strcpy(Destination, "PAUSED");
		}
		else if (CIRCLE->On)
		{
			strcpy(Destination, "ON");
		}
		return true;
	}
};

class IS_MovementType : public LSTypeDefinition
{
public:
	enum MovementMembers
	{
		Command       = 1,
		Stuck         = 2,
		Summoned      = 3,
		StuckLogic    = 4,
		Verbosity     = 5,
		FullVerbosity = 6,
		TotalSilence  = 7,
		Aggro         = 8,
		PauseMinDelay = 9,
		PauseMaxDelay = 10,
		PulseCheck    = 11,
		PulseUnstuck  = 12,
		TryToJump     = 13,
		DistStuck     = 14,
		Version       = 15,
		MovePause     = 16,
		GM            = 17,
		MeshLoaded    = 18,
	};

	IS_MovementType():LSTypeDefinition("Movement")
	{
		TypeMember(Command);
		TypeMember(Stuck);
		TypeMember(Summoned);
		TypeMember(StuckLogic);
		TypeMember(Verbosity);
		TypeMember(FullVerbosity);
		TypeMember(TotalSilence);
		TypeMember(Aggro);
		TypeMember(PauseMinDelay);
		TypeMember(PauseMaxDelay);
		TypeMember(PulseCheck);
		TypeMember(PulseUnstuck);
		TypeMember(TryToJump);
		TypeMember(DistStuck);
		TypeMember(Version);
		TypeMember(MovePause);
		TypeMember(GM);
		TypeMember(MeshLoaded);
	}

	bool GetMember(LSOBJECTDATA ObjectData, PCHAR Member, int argc, char *argv[], LSOBJECT &Dest);

	bool ToString(LSVARPTR VarPtr, char* Destination)
	{
		strcpy(Destination, "NONE");
		if (STICK->On) {
			strcpy(Destination, "STICK");
		} else if (CIRCLE->On) {
			strcpy(Destination, "CIRCLE");
		} else if (MOVETO->On) {
			strcpy(Destination, "MOVETO");
		} else if (CURCAMP->On) {
			strcpy(Destination, "MAKECAMP");
		}

		return true;
	}
};

class CMUMovement
{
public:
	float ChangeHead; // heading changes set to this
	float RootHead;   // lock rooted head to this

	void AutoHead();
	void NewHead(float fNewHead);
	void NewFace(double dNewFace);
	void StopHeading();
	float SaneHead(float fHeading);
	void DoRoot();
	void StopRoot();
	float AngDist(float fH1, float fH2);
	bool CanMove(float fHead, float fY, float fX);
	void SetWalk(bool bWalkOn);
	void DoStand();
	void Walk(unsigned char ucDirection);
	void TryMove(unsigned char ucDirection, unsigned char ucWalk, float fHead, float fY, float fX);
	void DoMove(unsigned char ucDirection, bool bTurnOn = true, unsigned char ucWalk = MU_WALKOFF); // main movement function
	void StopMove(unsigned char ucDirection);
	void StickStrafe(unsigned char ucDirection);

	CMUMovement()
	{
		ChangeHead = H_INACTIVE;
		RootHead   = 0.0f;
	};

private:
	void TimedStrafe(unsigned char ucDirection);
	void TurnHead(float fHeading);
	void FastTurn(float fNewHead);
	void LooseTurn(float fNewHead);
	void TrueTurn(float fNewHead);
	void TrueMoveOn(unsigned char ucDirection);
	void TrueMoveOff(unsigned char ucDirection);
	void SimMoveOn(unsigned char ucDirection);
	void SimMoveOff(unsigned char ucDirection);
};

static inline FLOAT GetDistance3D(PSPAWNINFO pChar, PSPAWNINFO pSpawn)
{
    FLOAT dX = pChar->X - pSpawn->X;
    FLOAT dY = pChar->Y - pSpawn->Y;
    FLOAT dZ = pChar->Z - pSpawn->Z;
    return sqrtf(dX*dX + dY*dY + dZ*dZ);
}

static inline FLOAT GetDistance3D(PSPAWNINFO pChar, CMULoc *pMoveTo)
{
    FLOAT dX = pChar->X - pMoveTo->X;
    FLOAT dY = pChar->Y - pMoveTo->Y;
    FLOAT dZ = pChar->Z - pMoveTo->Z;
    return sqrtf(dX*dX + dY*dY + dZ*dZ);
}

static inline bool validNumber(char *szTemp)
{
	return isdigit(szTemp[0]) || szTemp[0] == '-' || szTemp[0] == '.';
}