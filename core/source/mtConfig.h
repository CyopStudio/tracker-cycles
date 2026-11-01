#ifndef SOURCE_MTCONFIG_H_
#define SOURCE_MTCONFIG_H_

class Platform;

void initConfig(Platform* platform);
void updateConfig(bool force);

void forceSaveConfig();
void saveConfig();
void saveConfigAsap();
void readConfig();

void resetConfig();
void checkConfig();
void recoverConfigFromOldVerison();

#endif /* SOURCE_MTCONFIG_H_ */
