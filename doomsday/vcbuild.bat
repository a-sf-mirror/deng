@ECHO OFF
IF "%1" == "" GOTO Help
IF "%1" == "help" GOTO Help
GOTO SetPaths
:Help
cls
ECHO $Id$
ECHO.
ECHO This build script compiles the Doomsday Engine and the associated 
ECHO libraries.
ECHO.
ECHO Microsoft Visual C++ Toolkit is the minimum requirement.  If you
ECHO want to compile the engine itself, you will also need the Platform
ECHO SDK, which can be downloaded from Microsoft's website.  The DirectX
ECHO SDK is also required for compiling the engine.  This means you can
ECHO compile everything using tools that can be downloaded for free, but
ECHO all the SDKs will add up to several hundred megabytes. 
ECHO.
ECHO On the bright side, you only need the Visual C++ Toolkit in order to 
ECHO compile a game DLL.  The game DLLs' only dependency is Doomsday.lib,
ECHO which is included in the Doomsday source code package.
ECHO.
IF EXIST vcconfig.bat GOTO FoundConfig
:MissingConfig
ECHO IMPORTANT:
ECHO In order to use this batch file you'll first need to configure the
ECHO include paths to any dependancies of the code you wish to compile.
ECHO For example if you want to compile Doomsday.exe (Engine) you'll need
ECHO to set up the path to your Platform SDK (amongst others).
ECHO.
ECHO Open the file "vcconfig-example.bat", edit the paths appropriately
ECHO and then save your changes into a new file called:
ECHO.
ECHO   vcconfig.bat
ECHO.
GOTO TheRealEnd
:FoundConfig
ECHO Throughout this batch file there are targets that can be given as
ECHO arguments on the command line.  For example, to compile jDoom you
ECHO would use the "jdoom" target:
ECHO.
ECHO   vcbuild jdoom
ECHO.
ECHO The "all" target compiles everything in the appropriate order:
ECHO.
ECHO   vcbuild all
ECHO.
ECHO Build Tip:
ECHO To output the messages produced during compilation to a log file,
ECHO append the build target with the redirection character ">" followed
ECHO by a filename:
ECHO.
ECHO   vcbuild doomsday^>buildlog.txt
GOTO TheRealEnd

:SetPaths
:: -- Set up paths.
IF NOT EXIST vcconfig.bat GOTO MissingConfig
CALL vcconfig.bat

:: -- Compiler and linker options.
SET DEFINES=/D "ZLIB_DLL" /D "WIN32_GAMMA" /D "NORANGECHECKING" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS"
SET DLLDEFINES=/D "_USRDLL" /D "_WINDLL" %DEFINES%
SET INCS=/I "%FMOD_INC%" /I "%EAX_INC%" /I "%SDL_INC%" /I "%SDLNET_INC%" /I "%DX_INC%" /I "%PLATFORM_INC%" /I "./Include"
SET LIBS=/LIBPATH:"./Lib" /LIBPATH:"%DX_LIB%" /LIBPATH:"%EAX_LIB%" /LIBPATH:"%SDL_LIB%" /LIBPATH:"%SDLNET_LIB%" /LIBPATH:"%FMOD_LIB%" /LIBPATH:"%PLATFORM_LIB%" /LIBPATH:"./%BIN_DIR%"
SET FLAGS=/Ob1 /Oi /Ot /Oy /GF /FD /EHsc /MT /GS /Gy /Fo"./Obj/Release/" /Fd"./Obj/Release/" /W3 /Gz /Gs
SET LFLAGS=/INCREMENTAL:NO /SUBSYSTEM:WINDOWS /MACHINE:I386 


:: -- Make sure the output directories exist.
IF NOT EXIST %BIN_DIR% md %BIN_DIR%
IF NOT EXIST %OBJ_DIR% md %OBJ_DIR%


:: -- Iterate through all command line arguments.
:Looper
IF "%1" == "" GOTO TheEnd
GOTO %1


:: *** Cleanup and build all targets.
:All
CALL vcbuild cleanup res doomsday dpdehread dpmapload dropengl drd3d dsa3d dscompat jdoom jheretic jhexen
GOTO Done


:: *** Clean the output directories.
:Cleanup
rd/s/q %BIN_DIR%
rd/s/q %OBJ_DIR%
md %BIN_DIR%
md %OBJ_DIR%
GOTO Done


:: *** Resources (dialogs for Doomsday and drD3D)
:: Requires rc.exe and cvtres.exe from the Platform SDK.
:Res
rc Src\Doomsday.rc
cvtres /OUT:doomsday_res.obj /MACHINE:X86 Src\Doomsday.res
rc Src\drD3D\drD3D.rc
IF NOT EXIST %OBJ_DIR%\drD3D md %OBJ_DIR%\drD3D
cvtres /OUT:drD3D_res.obj /MACHINE:X86 Src\drD3D\drD3D.res
GOTO Done


:: *** Doomsday.exe
:Doomsday
ECHO Compiling Doomsday.exe (Engine)...
cl %FLAGS% %INCS% %DEFINES% /D "__DOOMSDAY__"  doomsday_res.obj Src\def_read.c Src\def_main.c Src\def_data.c Src\ui_panel.c Src\ui_mpi.c Src\ui_main.c Src\tab_video.c Src\tab_tables.c Src\m_vector.c Src\m_string.c Src\m_nodepile.c Src\m_misc.c Src\m_huffman.c Src\Common\m_fixed.c Src\m_filehash.c Src\m_bams.c Src\m_args.c Src\s_wav.c Src\s_sfx.c Src\s_mus.c Src\s_main.c Src\s_logic.c Src\s_environ.c Src\s_cache.c Src\gl_tga.c Src\gl_tex.c Src\gl_png.c Src\gl_pcx.c Src\gl_main.c Src\gl_hq2x.c Src\gl_font.c Src\gl_draw.c Src\rend_bias.c Src\rend_sprite.c Src\rend_sky.c Src\rend_shadow.c Src\rend_particle.c Src\rend_model.c Src\rend_main.c Src\rend_list.c Src\rend_halo.c Src\rend_fakeradio.c Src\rend_dyn.c Src\rend_decor.c Src\rend_clip.c Src\r_world.c Src\r_util.c Src\r_things.c Src\r_sky.c Src\r_shadow.c Src\r_model.c Src\r_main.c Src\r_extres.c Src\r_draw.c Src\r_data.c Src\r_lgrid.c Src\p_tick.c Src\p_think.c Src\p_sight.c Src\p_polyob.c Src\p_particle.c Src\p_mobj.c Src\p_maputil.c Src\p_intercept.c Src\p_data.c Src\p_dmu.c Src\p_arch.c Src\p_bmap.c Src\sv_sound.c Src\sv_pool.c Src\sv_missile.c Src\sv_main.c Src\sv_frame.c Src\cl_world.c Src\cl_sound.c Src\cl_player.c Src\cl_mobj.c Src\cl_main.c Src\cl_frame.c Src\net_ping.c Src\net_msg.c Src\net_main.c Src\net_event.c Src\net_demo.c Src\net_buf.c Src\sys_sfxd_loader.c Src\sys_sfxd_dummy.c Src\sys_sfxd_ds.c Src\sys_musd_win.c Src\sys_musd_fmod.c Src\sys_timer.c Src\sys_system.c Src\sys_stwin.c Src\sys_sock.c Src\Unix\sys_network.c Src\sys_mixer.c Src\sys_master.c Src\sys_input.c Src\sys_findfile.c Src\sys_filein.c Src\sys_direc.c Src\sys_console.c Src\con_start.c Src\con_main.c Src\con_config.c Src\con_bind.c Src\con_bar.c Src\con_action.c Src\dd_zone.c Src\dd_zip.c Src\dd_winit.c Src\dd_wad.c Src\dd_plugin.c Src\dd_pinit.c Src\dd_main.c Src\dd_loop.c Src\dd_input.c Src\dd_help.c Src\dd_dgl.c    /link /OUT:"./%BIN_DIR%/Doomsday.exe" /DEF:"./Src/Doomsday.def" %LFLAGS% %LIBS% sdl_net.lib sdl.lib wsock32.lib libpng.lib libz.lib fmodvc.lib lzss.lib dinput.lib dsound.lib eaxguid.lib dxguid.lib winmm.lib libpng.lib libz.lib LZSS.lib gdi32.lib ole32.lib user32.lib
GOTO Done


:: *** dpDehRead.dll
:dpDehRead
ECHO Compiling dpDehRead.dll (Dehacked Reader Plugin)...
md %OBJ_DIR%\dpDehRead
cl /O2 /Ob1 %INCS% %DLLDEFINES% /D "DPDEHREAD_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dpDehRead/" /Fd"./%OBJ_DIR%/dpDehRead/" /W3 /Gz   Src/dpDehRead/dehmain.c    /link /OUT:"./%BIN_DIR%/dpDehRead.dll" %LFLAGS% /DLL /IMPLIB:"./%BIN_DIR%/dpDehRead.lib" %LIBS% ./%BIN_DIR%/Doomsday.lib
GOTO Done


:: *** dpMapLoad.dll
:dpMapLoad
ECHO Compiling dpMapLoad.dll ((gl)BSP Node Builder Plugin)...
md %OBJ_DIR%\dpMapLoad
cl /O2 /Ob1 %INCS% %DLLDEFINES% /D "GLBSP_PLUGIN" /D "DPMAPLOAD_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dpMapLoad/" /Fd"./%OBJ_DIR%/dpMapLoad/" /W3 /Gz Src\dpMapLoad\glBSP\analyze.c Src\dpMapLoad\glBSP\wad.c Src\dpMapLoad\glBSP\util.c Src\dpMapLoad\glBSP\system.c Src\dpMapLoad\glBSP\seg.c Src\dpMapLoad\glBSP\reject.c Src\dpMapLoad\glBSP\node.c Src\dpMapLoad\glBSP\level.c Src\dpMapLoad\glBSP\glbsp.c Src\dpMapLoad\glBSP\blockmap.c Src\dpMapLoad\maploader.c   /link /OUT:"./%BIN_DIR%/dpMapLoad.dll" %LFLAGS% /DLL /IMPLIB:"./%BIN_DIR%/dpMapLoad.lib" %LIBS% ./%BIN_DIR%/Doomsday.lib 
GOTO Done


:: *** drD3D.dll
:drD3D
ECHO Compiling drD3D.dll (Direct3D 8.1 driver)...
md %OBJ_DIR%\drD3D
cl /O2 /Ob1 /I "./Include/drD3D" %INCS% %DLLDEFINES% /D "drD3D_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/drD3D/" /Fd"./%OBJ_DIR%/drD3D/" /W3 /Gz drD3D_res.obj Src\drD3D\window.cpp Src\drD3D\viewport.cpp Src\drD3D\texture.cpp Src\drD3D\state.cpp Src\drD3D\matrix.cpp Src\drD3D\main.cpp Src\drD3D\draw.cpp Src\drD3D\d3dinit.cpp Src\drD3D\config.cpp Src\drD3D\box.cpp   /link /OUT:"./%BIN_DIR%/drD3D.dll" %LFLAGS% /DLL /DEF:".\Src\drD3D\drD3D.def" /IMPLIB:"./%BIN_DIR%/drD3D.lib" /LIBPATH:"%LIBCI_LIB%" %LIBS% ./%BIN_DIR%/doomsday.lib d3d8.lib d3dx8.lib user32.lib gdi32.lib ole32.lib uuid.lib advapi32.lib
GOTO Done


:: *** drOpenGL.dll
:drOpenGL
ECHO Compiling drOpenGL.dll (OpenGL driver)...
md %OBJ_DIR%\drOpenGL
cl /O2 /Ob1 /I "%GL_INC%" /I "./Include/drOpenGL" %INCS% %DLLDEFINES% /D "drOpenGL_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/drOpenGL/" /Fd"./%OBJ_DIR%/drOpenGL/" /W3 /Gz Src\drOpenGL\texture.c Src\drOpenGL\main.c Src\drOpenGL\ext.c Src\drOpenGL\draw.c Src\drOpenGL\common.c   /link /OUT:"./%BIN_DIR%/drOpenGL.dll" %LFLAGS% /DLL /DEF:".\Src\drOpenGL\drOpenGL.def" /IMPLIB:"./%BIN_DIR%/drOpenGL.lib" %LIBS% ./%BIN_DIR%/doomsday.lib opengl32.lib glu32.lib kernel32.lib user32.lib gdi32.lib
GOTO Done


:: *** dsA3D.dll
:dsA3D
ECHO Compiling dsA3D.dll (A3D SoundFX driver)...
md %OBJ_DIR%\dsA3D
cl /O2 /Ob1 %INCS% /I "%A3D_INC%" %DLLDEFINES% /D "DSA3D_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dsA3D" /Fd"./%OBJ_DIR%/dsA3D" /W3 /Gz Src\dsA3D\driver_a3d.cpp   /link /OUT:"./%BIN_DIR%/dsA3D.dll" %LFLAGS% /DLL /DEF:".\Src\dsA3D\dsA3D.def" /IMPLIB:"./%BIN_DIR%/dsA3D.lib" %LIBS% /LIBPATH:"%A3D_LIB%" ./%BIN_DIR%/doomsday.lib ia3dutil.lib ole32.lib 
GOTO Done


:: *** dsCompat.dll
:dsCompat
ECHO Compiling dsCompat.dll (DirectSound(3D) SoundFX driver)...
md %OBJ_DIR%\dsCompat
cl /O2 /Ob1 %INCS% %DLLDEFINES% /D "DSCOMPAT_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/dsCompat" /Fd"./%OBJ_DIR%/dsCompat" /W3 /Gz Src\dsCompat\driver_sndcmp.cpp   /link /OUT:"./%BIN_DIR%/dsCompat.dll" %LFLAGS% /DLL /DEF:".\Src\dsCompat\dsCompat.def" /IMPLIB:"./%BIN_DIR%/dsCompat.lib" %LIBS% eax.lib eaxguid.lib dsound.lib dxguid.lib ./%BIN_DIR%/doomsday.lib 
GOTO Done


:: *** jDoom.dll
:jDoom
ECHO Compiling jDoom.dll (jDoom Game Library)...
md %OBJ_DIR%\jDoom
cl /O2 /Ob1 /I "./Include/jDoom" /I "./Include/Common" /I "./Include" /D "__JDOOM__" %DLLDEFINES% /D "JDOOM_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jDoom/" /Fd"./%OBJ_DIR%/jDoom/" /W3 /Gz  Src\Common\d_netsv.c Src\Common\d_netcl.c Src\Common\d_net.c Src\Common\dmu_lib.c Src\Common\m_multi.c Src\Common\mn_menu.c Src\Common\m_ctrl.c Src\Common\g_dglinit.c Src\jDoom\d_api.c Src\jDoom\d_refresh.c Src\jDoom\d_main.c Src\jDoom\d_console.c Src\Common\p_xgsec.c Src\Common\p_xgsave.c Src\Common\p_xgline.c Src\Common\p_xgfile.c Src\Common\p_view.c Src\jDoom\p_user.c Src\Common\p_tick.c Src\jDoom\p_telept.c Src\jDoom\p_switch.c Src\Common\p_svtexarc.c Src\Common\p_start.c Src\Common\p_mapsetup.c Src\jDoom\p_spec.c Src\jDoom\p_sound.c Src\jDoom\p_setup.c Src\Common\p_saveg.c Src\jDoom\p_pspr.c Src\jDoom\p_plats.c Src\jDoom\p_oldsvg.c Src\jDoom\p_mobj.c Src\jDoom\p_maputl.c Src\jDoom\p_map.c Src\jDoom\p_lights.c Src\jDoom\p_inter.c Src\jDoom\p_floor.c Src\jDoom\p_enemy.c Src\jDoom\p_doors.c Src\jDoom\p_ceilng.c Src\Common\p_actor.c Src\Common\x_hair.c Src\jDoom\wi_stuff.c Src\jDoom\st_stuff.c Src\Common\st_lib.c Src\Common\hu_stuff.c Src\Common\hu_pspr.c Src\Common\hu_msg.c Src\Common\hu_lib.c Src\Common\f_infine.c Src\Common\am_map.c Src\jDoom\tables.c Src\Common\r_common.c Src\jDoom\m_random.c Src\Common\m_fixed.c Src\jDoom\m_cheat.c Src\Common\g_update.c Src\Common\g_game.c Src\Common\g_controls.c Src\jDoom\d_items.c Src\jDoom\D_Action.c Src\jDoom\AcFnLink.c   /link  /OUT:"./%BIN_DIR%/jDoom.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:".\Src\jDoom\jDoom.def" /IMPLIB:"./%BIN_DIR%/jDoom.lib" ./%BIN_DIR%/doomsday.lib lzss.lib
GOTO Done


:: *** jHeretic.dll
:jHeretic
ECHO Compiling jHeretic.dll (jHeretic Game Library)...
md %OBJ_DIR%\jHeretic
cl /O2 /Ob1 /I "./Include/jHeretic" /I "./Include/Common" /I "./Include" /D "__JHERETIC__" %DLLDEFINES% /D "JHERETIC_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jHeretic/" /Fd"./%OBJ_DIR%/jHeretic/" /W3 /Gz Src\Common\d_netsv.c Src\Common\d_netcl.c Src\Common\d_net.c Src\Common\dmu_lib.c Src\Common\m_multi.c Src\Common\mn_menu.c Src\Common\m_ctrl.c Src\Common\g_dglinit.c Src\jHeretic\h_api.c Src\jHeretic\H_Refresh.c Src\jHeretic\H_Main.c Src\jHeretic\H_Console.c Src\Common\p_xgsec.c Src\Common\p_xgsave.c Src\Common\p_xgline.c Src\Common\p_xgfile.c Src\Common\p_view.c Src\jHeretic\P_user.c Src\Common\p_tick.c Src\jHeretic\P_telept.c Src\jHeretic\P_switch.c Src\Common\p_svtexarc.c Src\Common\p_start.c Src\Common\p_mapsetup.c Src\jHeretic\P_spec.c Src\jHeretic\p_sound.c Src\jHeretic\P_setup.c Src\Common\p_saveg.c Src\jHeretic\P_pspr.c Src\jHeretic\P_plats.c Src\jHeretic\P_oldsvg.c Src\jHeretic\P_mobj.c Src\jHeretic\P_maputl.c Src\jHeretic\P_map.c Src\jHeretic\P_lights.c Src\jHeretic\P_inter.c Src\jHeretic\P_floor.c Src\jHeretic\P_enemy.c Src\jHeretic\P_doors.c Src\jHeretic\P_ceilng.c Src\Common\p_actor.c Src\Common\x_hair.c Src\Common\r_common.c Src\Common\m_fixed.c Src\Common\hu_pspr.c Src\Common\g_update.c Src\Common\g_game.c Src\Common\g_controls.c Src\jHeretic\H_Action.c Src\Common\f_infine.c Src\jHeretic\Tables.c Src\jHeretic\st_stuff.c Src\Common\hu_stuff.c Src\jHeretic\m_random.c Src\jHeretic\In_lude.c Src\Common\st_lib.c Src\Common\hu_lib.c Src\Common\hu_msg.c Src\Common\am_map.c Src\jHeretic\AcFnLink.c   /link /OUT:"./%BIN_DIR%/jHeretic.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:".\Src\jHeretic\jHeretic.def" /IMPLIB:"./%BIN_DIR%/jHeretic.lib" ./%BIN_DIR%/doomsday.lib lzss.lib 
GOTO Done


:: *** jHexen.dll
:jHexen
ECHO Compiling jHexen.dll (jHexen Game Library)...
md %OBJ_DIR%\jHexen
cl /O2 /Ob1 /I "Include/jHexen" /I "Include/Common" /I "Include" /D "__JHEXEN__" %DLLDEFINES% /D "JHEXEN_EXPORTS" /GF /FD /EHsc /MT /Gy /Fo"./%OBJ_DIR%/jHexen/" /Fd"./%OBJ_DIR%/jHexen/" /W3 /Gz Src\Common\d_netsv.c Src\Common\d_netcl.c Src\Common\d_net.c Src\Common\dmu_lib.c Src\Common\m_multi.c Src\Common\Mn_menu.c Src\Common\m_ctrl.c Src\Common\g_update.c Src\Common\g_dglinit.c Src\jHexen\x_api.c Src\jHexen\HRefresh.c Src\jHexen\HConsole.c Src\jHexen\H2_main.c Src\jHexen\H2_Actn.c Src\Common\p_view.c Src\Common\p_tick.c Src\Common\p_svtexarc.c Src\Common\p_start.c Src\Common\p_mapsetup.c Src\Common\p_actor.c Src\jHexen\P_user.c Src\jHexen\P_things.c Src\jHexen\P_telept.c Src\jHexen\P_switch.c Src\jHexen\P_spec.c Src\jHexen\P_sight.c Src\jHexen\P_setup.c Src\jHexen\P_pspr.c Src\jHexen\P_plats.c Src\jHexen\P_mobj.c Src\jHexen\P_maputl.c Src\jHexen\P_map.c Src\jHexen\P_lights.c Src\jHexen\P_inter.c Src\jHexen\P_floor.c Src\jHexen\P_enemy.c Src\jHexen\P_doors.c Src\jHexen\P_ceilng.c Src\jHexen\P_anim.c Src\jHexen\P_acs.c Src\Common\x_hair.c Src\jHexen\s_sound.c Src\Common\r_common.c Src\Common\m_fixed.c Src\Common\hu_pspr.c Src\Common\g_game.c Src\Common\g_controls.c Src\Common\f_infine.c Src\jHexen\Tables.c Src\jHexen\sv_save.c Src\jHexen\Sn_sonix.c Src\jHexen\Sc_man.c Src\Common\hu_stuff.c Src\Common\hu_msg.c Src\Common\hu_lib.c Src\Common\st_lib.c Src\jHexen\Po_man.c Src\jHexen\M_misc.c Src\jHexen\In_lude.c Src\jHexen\st_stuff.c Src\Common\am_map.c Src\jHexen\AcFnLink.c Src\jHexen\A_action.c    /link /OUT:"./%BIN_DIR%/jHexen.dll" %LFLAGS% /LIBPATH:"./Lib" /DLL /DEF:".\Src\jHexen\jHexen.def" /IMPLIB:"./%BIN_DIR%/jHexen.lib" ./%BIN_DIR%/doomsday.lib lzss.lib
GOTO Done


:: -- Move to the next argument.
:Done
SHIFT
GOTO Looper


:TheEnd
ECHO All Done!


:TheRealEnd
