#ifndef MZD_NIGHTMODE_H
#define MZD_NIGHTMODE_H

#define NM_NO_VALUE -1
#define NM_DAY_MODE 0
#define NM_NIGHT_MODE 1

/** Sets up a connection. Has to be called before mzd_is_night_mode_set returns values. **/
void mzd_nightmode_start();

/** Returns NM_NIGHT_MODE if night mode is set, NM_DAY_MODE if it's not and NM_NO_VALUE if there was an error. **/
int mzd_is_night_mode_set();

/** Tears down the connection and frees stuff **/
void mzd_nightmode_stop();

#endif
