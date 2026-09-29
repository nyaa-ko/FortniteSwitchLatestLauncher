#include <switch.h>
#include <string>
#include <cstdio>
#include <unistd.h>
#include "../include/json.hpp"
#include "UI.h"
#include "AccountManager.h"

using json=nlohmann::json;
static constexpr u64 GAME_TITLE_ID=0x010025400AECE000ULL;
static constexpr const char* COMMAND_LINE_PATH="sdmc:/atmosphere/contents/010025400AECE000/romfs/UECommandLine.txt";
static constexpr const char* NORMAL_COMMAND_LINE="../../../FortniteGame/FortniteGame.uproject -skippatchcheck";

static void EnsureCommandLine(){ FILE*f=fopen(COMMAND_LINE_PATH,"r"); if(f){fclose(f);return;} f=fopen(COMMAND_LINE_PATH,"w"); if(f){fputs(NORMAL_COMMAND_LINE,f);fclose(f);} }
static void RestoreCommandLine(){ FILE*f=fopen(COMMAND_LINE_PATH,"w"); if(f){fputs(NORMAL_COMMAND_LINE,f);fclose(f);} }
static std::string ActiveLabel(const json& db){json a=AccountManager::GetActive(db);return a.empty()?"NONE":a.value("label","ACCOUNT");}

static void WaitBack(MidnightUI::Screen&s,PadState&pad){while(appletMainLoop()){padUpdate(&pad);u64 k=padGetButtonsDown(&pad);if(k&(HidNpadButton_B|HidNpadButton_A))return;s.Begin();s.Header("STATUS");MidnightUI::Rect(s.fb,s.stride,70,140,1140,250,MidnightUI::PANEL);MidnightUI::Text(s.fb,s.stride,100,190,"OPERATION COMPLETE",MidnightUI::TEXT,4);MidnightUI::Text(s.fb,s.stride,100,250,"PRESS A OR B TO RETURN",MidnightUI::MUTED,3);s.Footer(true);s.End();}}

static void Accounts(PadState&pad){int sel=0; MidnightUI::Screen s; while(appletMainLoop()){
 json db=AccountManager::Load();int n=(int)db["accounts"].size();if(n==0)sel=0;else if(sel>=n)sel=n-1;
 s.Begin();s.Header("ACCOUNTS","PROFILES");
 if(n==0){MidnightUI::Text(s.fb,s.stride,80,150,"NO ACCOUNTS SAVED",MidnightUI::TEXT,4);MidnightUI::Text(s.fb,s.stride,80,205,"USE X IN THE ORIGINAL PROFILE BUILD TO ADD ONE.",MidnightUI::MUTED,2);} else {for(int i=0;i<n;i++){auto&a=db["accounts"][i];std::string v=(a.value("id","")==db.value("activeAccount",""))?"ACTIVE":"";s.Row(i,i==sel,a.value("label","ACCOUNT"),v);}}
 MidnightUI::Text(s.fb,s.stride,730,135,"PROFILE",MidnightUI::MUTED,2);MidnightUI::Rect(s.fb,s.stride,730,165,470,220,MidnightUI::PANEL);if(n){auto&a=db["accounts"][sel];MidnightUI::Text(s.fb,s.stride,765,205,a.value("label","ACCOUNT"),MidnightUI::TEXT,4);std::string c=a.value("color","red");MidnightUI::Color cc=MidnightUI::RED;if(c=="green")cc=MidnightUI::GREEN;if(c=="yellow")cc=MidnightUI::YELLOW;MidnightUI::Rect(s.fb,s.stride,765,270,42,42,cc);MidnightUI::Text(s.fb,s.stride,830,280,c,MidnightUI::MUTED,2);}
 s.Footer(true);s.End();padUpdate(&pad);u64 k=padGetButtonsDown(&pad);if(k&HidNpadButton_B)return;if(k&HidNpadButton_Up&&n)sel=(sel+n-1)%n;if(k&HidNpadButton_Down&&n)sel=(sel+1)%n;if(k&HidNpadButton_A&&n)AccountManager::SetActive(db["accounts"][sel].value("id",""));if(k&HidNpadButton_R&&n){AccountManager::CycleColor(db["accounts"][sel].value("id",""));}}
}

static void CommandLine(PadState&pad){int sel=0;MidnightUI::Screen s;while(appletMainLoop()){s.Begin();s.Header("COMMAND LINE","SYSTEM");s.Row(0,sel==0,"NORMAL","READY");s.Row(1,sel==1,"RESTORE NORMAL","WRITE");s.Row(2,sel==2,"SHOW PATH","");MidnightUI::Rect(s.fb,s.stride,730,130,470,300,MidnightUI::PANEL);MidnightUI::Text(s.fb,s.stride,760,165,"CURRENT PROFILE",MidnightUI::MUTED,2);MidnightUI::Text(s.fb,s.stride,760,210,"NORMAL",MidnightUI::TEXT,4);MidnightUI::Text(s.fb,s.stride,760,275,"SAFE DEFAULT",MidnightUI::MUTED,2);MidnightUI::Text(s.fb,s.stride,760,330,"UECOMMANDLINE.TXT",MidnightUI::MUTED,2);s.Footer(true);s.End();padUpdate(&pad);u64 k=padGetButtonsDown(&pad);if(k&HidNpadButton_B)return;if(k&HidNpadButton_Up)sel=(sel+2)%3;if(k&HidNpadButton_Down)sel=(sel+1)%3;if(k&HidNpadButton_A){if(sel==1){RestoreCommandLine();WaitBack(s,pad);}else if(sel==2){WaitBack(s,pad);}}}}

static void PatchInfo(PadState&pad){MidnightUI::Screen s;while(appletMainLoop()){s.Begin();s.Header("PATCH","SYSTEM");MidnightUI::Rect(s.fb,s.stride,70,130,1140,360,MidnightUI::PANEL);MidnightUI::Text(s.fb,s.stride,105,170,"PATCH MANAGEMENT",MidnightUI::TEXT,4);MidnightUI::Text(s.fb,s.stride,105,235,"PATCH FILES ARE NOT MODIFIED BY THIS BUILD.",MidnightUI::MUTED,2);MidnightUI::Text(s.fb,s.stride,105,275,"FORTNITEBAN",MidnightUI::TEXT,3);MidnightUI::Text(s.fb,s.stride,105,325,"STATUS: EXTERNAL",MidnightUI::YELLOW,3);MidnightUI::Text(s.fb,s.stride,105,385,"USE YOUR NORMAL FILE MANAGEMENT METHOD.",MidnightUI::MUTED,2);s.Footer(true);s.End();padUpdate(&pad);if(padGetButtonsDown(&pad)&HidNpadButton_B)return;}}

int main(){gfxInitDefault();padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);AccountManager::EnsureDirectory();EnsureCommandLine();int sel=0;MidnightUI::Screen s;
 while(appletMainLoop()){
  json db=AccountManager::Load();s.Begin();s.Header("HOME","MIDNIGHT");s.Row(0,sel==0,"ACCOUNTS",std::to_string(db["accounts"].size()));s.Row(1,sel==1,"LAUNCH","FORTNITE");s.Row(2,sel==2,"COMMAND LINE","NORMAL");s.Row(3,sel==3,"PATCH","EXTERNAL");s.Row(4,sel==4,"SETTINGS","");s.Row(5,sel==5,"EXIT","");
  MidnightUI::Rect(s.fb,s.stride,730,130,470,320,MidnightUI::PANEL);MidnightUI::Text(s.fb,s.stride,765,165,"ACTIVE ACCOUNT",MidnightUI::MUTED,2);MidnightUI::Text(s.fb,s.stride,765,210,ActiveLabel(db),MidnightUI::TEXT,4);MidnightUI::Text(s.fb,s.stride,765,285,"A SELECT",MidnightUI::MUTED,2);MidnightUI::Text(s.fb,s.stride,765,325,"B BACK",MidnightUI::MUTED,2);MidnightUI::Text(s.fb,s.stride,765,365,"UP / DOWN MOVE",MidnightUI::MUTED,2);s.Footer(false);s.End();padUpdate(&pad);u64 k=padGetButtonsDown(&pad);if(k&HidNpadButton_Plus)break;if(k&HidNpadButton_Up)sel=(sel+5)%6;if(k&HidNpadButton_Down)sel=(sel+1)%6;if(k&HidNpadButton_A){switch(sel){case 0:Accounts(pad);break;case 1:appletRequestLaunchApplication(GAME_TITLE_ID,nullptr);break;case 2:CommandLine(pad);break;case 3:PatchInfo(pad);break;case 5:return 0;default:break;}} }
 gfxExit();return 0;}
