#include "gridfall/core/MouseInput.hpp"
#include <raylib.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
namespace {
float gNextDx=0.0f,gNextDy=0.0f;
int gGetDeltaCalls=0,gConsumeRelativeCalls=0,gResetRelativeCalls=0;
int gDisableCalls=0,gEnableCalls=0,gShowCalls=0;
bool gCanActivateRelativeMode=true,gRelativeModeActive=false;
int gRelativeModeChecks=0;
}
extern "C" Vector2 GetMouseDelta(void){++gGetDeltaCalls;return{};}
extern "C" Vector2 GridfallConsumeRelativeMouseDelta(void){
 ++gConsumeRelativeCalls;const Vector2 result{gNextDx,gNextDy};gNextDx=0;gNextDy=0;return result;
}
extern "C" void GridfallResetRelativeMouseDelta(void){++gResetRelativeCalls;gNextDx=0;gNextDy=0;}
extern "C" int GridfallRelativeMouseModeActive(void){++gRelativeModeChecks;return gRelativeModeActive?1:0;}
extern "C" void DisableCursor(void){++gDisableCalls;gRelativeModeActive=gCanActivateRelativeMode;}
extern "C" void EnableCursor(void){++gEnableCalls;gRelativeModeActive=false;ShowCursor();}
extern "C" void ShowCursor(void){++gShowCalls;}
#define CHECK(condition) do { \
 if(!(condition)){std::fprintf(stderr,"Relative-input CHECK failed at %s:%d: %s\n",__FILE__,__LINE__,#condition);return EXIT_FAILURE;} \
} while(false)
int main(){
 using namespace gridfall;
 MouseInput input;CHECK(input.initialize());CHECK(input.backendName()!=nullptr);
 gNextDx=17;gNextDy=-9;const MouseDelta ignored=input.consumeDelta();
 CHECK(ignored.x==0&&ignored.y==0&&gConsumeRelativeCalls==0);
 CHECK(input.setCaptured(true));CHECK(gDisableCalls==1&&gResetRelativeCalls==1);
 gNextDx=2400;gNextDy=-1800;const MouseDelta stale=input.consumeDelta();
 CHECK(stale.x==0&&stale.y==0);
 gNextDx=12;gNextDy=-6;const MouseDelta first=input.consumeDelta();
 CHECK(first.x==12&&first.y==-6);
 struct Motion{const char* name;float dx,dy;int yawSign,pitchSign;};
 constexpr std::array<Motion,8> motions{{
  {"right",24,0,1,0},{"left",-24,0,-1,0},{"up",0,-24,0,1},{"down",0,24,0,-1},
  {"up-right",18,-18,1,1},{"up-left",-18,-18,-1,1},{"down-right",18,18,1,-1},{"down-left",-18,18,-1,-1}}};
 for(const Motion&m:motions){
  gNextDx=m.dx;gNextDy=m.dy;const MouseDelta d=input.consumeDelta();
  CHECK(std::fabs(d.x-m.dx)<.001f&&std::fabs(d.y-m.dy)<.001f);
  MouseLookController camera;camera.reset(0,0);camera.update(d.x,d.y);
  const int ys=camera.yaw()>0?1:camera.yaw()<0?-1:0,ps=camera.pitch()>0?1:camera.pitch()<0?-1:0;
  if(ys!=m.yawSign||ps!=m.pitchSign){std::fprintf(stderr,"Direction mapping failed for %s\n",m.name);return EXIT_FAILURE;}
 }
 const MouseDelta stationary=input.consumeDelta();CHECK(stationary.x==0&&stationary.y==0);
 CHECK(gConsumeRelativeCalls==11&&gGetDeltaCalls==0);
 CHECK(!input.setCaptured(false));CHECK(gEnableCalls==1&&gShowCalls==1&&gResetRelativeCalls==2);
 gNextDx=-999;gNextDy=777;const MouseDelta released=input.consumeDelta();
 CHECK(released.x==0&&released.y==0&&gConsumeRelativeCalls==11);
 CHECK(input.setCaptured(true));CHECK(gDisableCalls==2&&gResetRelativeCalls==3);
 gNextDx=-1600;gNextDy=1200;const MouseDelta staleRecapture=input.consumeDelta();
 CHECK(staleRecapture.x==0&&staleRecapture.y==0);
 gNextDx=5;gNextDy=-3;const MouseDelta afterRecapture=input.consumeDelta();
 CHECK(afterRecapture.x==5&&afterRecapture.y==-3);
 input.shutdown();CHECK(gEnableCalls==2&&gShowCalls==2&&gResetRelativeCalls==4&&gGetDeltaCalls==0);
 MouseInput rejected;CHECK(rejected.initialize());gCanActivateRelativeMode=false;
 const int consumeBefore=gConsumeRelativeCalls;CHECK(!rejected.setCaptured(true));
 CHECK(gRelativeModeChecks>=3&&!gRelativeModeActive);
 gNextDx=800;gNextDy=-600;const MouseDelta rejectedDelta=rejected.consumeDelta();
 CHECK(rejectedDelta.x==0&&rejectedDelta.y==0&&gConsumeRelativeCalls==consumeBefore);
 CHECK(gDisableCalls==3&&gEnableCalls==3);rejected.shutdown();gCanActivateRelativeMode=true;
 std::puts("SDL relative-input adapter passed: signed deltas, 8 directions, one-time reads, capture lifecycle and fail-closed rejection.");
 return EXIT_SUCCESS;
}
