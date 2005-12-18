#ifndef __DCONSOLE_H__
#define __DCONSOLE_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

void            G_ConsoleRegistration();
void            D_ConsoleBg(int *width, int *height);

#endif
