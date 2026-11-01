#ifndef SOURCE_POWERCONTROL_H_
#define SOURCE_POWERCONTROL_H_

class PlatformImpl;

void TactSwitchInit();
void TactSwitchRead(PlatformImpl* platformImpl);
void checkPowerState(PlatformImpl* platformImpl);
bool getTactSwitchState();

#endif /* SOURCE_POWERCONTROL_H_ */
