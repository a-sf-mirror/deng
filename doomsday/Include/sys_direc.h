#ifndef __DOOMSDAY_DIREC_H__
#define __DOOMSDAY_DIREC_H__

#include "dd_types.h"

void	Dir_GetDir(directory_t *dir);
int		Dir_ChDir(directory_t *dir);
void	Dir_FileDir(const char *str, directory_t *dir);
void	Dir_FileName(const char *str, char *name);
void	Dir_MakeDir(const char *path, directory_t *dir);
int		Dir_FileID(const char *str);
void 	Dir_FixSlashes(char *path);
void	Dir_ValidDir(char *str);
boolean Dir_IsEqual(directory_t *a, directory_t *b);
int		Dir_IsAbsolute(const char *str);
void	Dir_MakeAbsolute(char *path);

#endif
