@echo off
setlocal
set "ROOMCAT_ROOT=%~dp0"
set "ROOMCAT_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%ROOMCAT_VSWHERE%" exit /b 1
for /f "usebackq delims=" %%i in (`"%ROOMCAT_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "ROOMCAT_VS=%%i"
if not defined ROOMCAT_VS exit /b 1
call "%ROOMCAT_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist "%ROOMCAT_ROOT%build" mkdir "%ROOMCAT_ROOT%build"
if not defined ROOMCAT_PYTHON set "ROOMCAT_PYTHON=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if not exist "%ROOMCAT_PYTHON%" set "ROOMCAT_PYTHON=python"
"%ROOMCAT_PYTHON%" "%ROOMCAT_ROOT%tools\check_native_api.py"
if errorlevel 1 exit /b 1
"%ROOMCAT_PYTHON%" "%ROOMCAT_ROOT%tools\build_sidebar_swf.py"
if errorlevel 1 exit /b 1
"%ROOMCAT_PYTHON%" "%ROOMCAT_ROOT%tests\test_sidebar_swf.py"
if errorlevel 1 exit /b 1
"%ROOMCAT_PYTHON%" "%ROOMCAT_ROOT%tools\build_trait_catalog.py"
if errorlevel 1 exit /b 1
"%ROOMCAT_PYTHON%" "%ROOMCAT_ROOT%tests\test_trait_catalog.py"
if errorlevel 1 exit /b 1
"%ROOMCAT_PYTHON%" "%ROOMCAT_ROOT%tools\build_marker_icons.py"
if errorlevel 1 exit /b 1
"%ROOMCAT_PYTHON%" "%ROOMCAT_ROOT%tools\build_ui_theme.py"
if errorlevel 1 exit /b 1
"%ROOMCAT_PYTHON%" "%ROOMCAT_ROOT%tests\test_ui_theme.py"
if errorlevel 1 exit /b 1
pushd "%ROOMCAT_ROOT%build"
cl /nologo /MT /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /I"%ROOMCAT_ROOT%vendor\mew-ui-api\src\native" /c "%ROOMCAT_ROOT%vendor\mew-ui-api\src\native\mew_ui_api.c" /Fo:mew_ui_api.obj
if errorlevel 1 goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD /DIMGUI_USE_WCHAR32 /I"%ROOMCAT_ROOT%vendor\imgui" /I"%ROOMCAT_ROOT%vendor\mew-ui-api\src\native" /LD "%ROOMCAT_ROOT%src\mod.cpp" "%ROOMCAT_ROOT%src\ui_skin_data.cpp" "%ROOMCAT_ROOT%src\game_reader.cpp" "%ROOMCAT_ROOT%src\room_labels.cpp" "%ROOMCAT_ROOT%src\cat_details.cpp" "%ROOMCAT_ROOT%src\native_sidebar.cpp" "%ROOMCAT_ROOT%src\cat_moves.cpp" "%ROOMCAT_ROOT%src\native_moves.cpp" "%ROOMCAT_ROOT%src\cat_actions.cpp" "%ROOMCAT_ROOT%src\native_actions.cpp" "%ROOMCAT_ROOT%src\gamepad_input.cpp" "%ROOMCAT_ROOT%src\newborn_plan.cpp" "%ROOMCAT_ROOT%src\adult_plan.cpp" "%ROOMCAT_ROOT%src\newborn_batch.cpp" mew_ui_api.obj "%ROOMCAT_ROOT%vendor\imgui\imgui.cpp" "%ROOMCAT_ROOT%vendor\imgui\imgui_draw.cpp" "%ROOMCAT_ROOT%vendor\imgui\imgui_tables.cpp" "%ROOMCAT_ROOT%vendor\imgui\imgui_widgets.cpp" "%ROOMCAT_ROOT%vendor\imgui\backends\imgui_impl_win32.cpp" "%ROOMCAT_ROOT%vendor\imgui\backends\imgui_impl_opengl3.cpp" /Fe:RoomCatList.dll /link opengl32.lib gdi32.lib user32.lib imm32.lib dwmapi.lib
if errorlevel 1 goto :failed
cl /nologo /O2 /MT /utf-8 /W3 /D_CRT_SECURE_NO_WARNINGS /LD "%ROOMCAT_ROOT%vendor\mewjector\version.c" /Fe:version.dll /link /DEF:"%ROOMCAT_ROOT%vendor\mewjector\version.def" user32.lib psapi.lib
if errorlevel 1 goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 "%ROOMCAT_ROOT%tests\reader_tests.cpp" "%ROOMCAT_ROOT%tests\comfort_reader_tests.cpp" "%ROOMCAT_ROOT%tests\room_scope_tests.cpp" "%ROOMCAT_ROOT%tests\family_reader_tests.cpp" "%ROOMCAT_ROOT%tests\native_args_tests.cpp" "%ROOMCAT_ROOT%src\game_reader.cpp" "%ROOMCAT_ROOT%src\room_labels.cpp" "%ROOMCAT_ROOT%src\cat_details.cpp" /Fe:reader_tests.exe
if errorlevel 1 goto :failed
reader_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 "%ROOMCAT_ROOT%tests\cat_details_tests.cpp" game_reader.obj room_labels.obj cat_details.obj /Fe:cat_details_tests.exe
if errorlevel 1 goto :failed
cat_details_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 "%ROOMCAT_ROOT%tests\cat_sort_tests.cpp" game_reader.obj room_labels.obj cat_details.obj /Fe:cat_sort_tests.exe
if errorlevel 1 goto :failed
cat_sort_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 "%ROOMCAT_ROOT%tests\cat_moves_tests.cpp" cat_moves.obj /Fe:cat_moves_tests.exe
if errorlevel 1 goto :failed
cat_moves_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 /I"%ROOMCAT_ROOT%vendor\mew-ui-api\src\native" "%ROOMCAT_ROOT%tests\native_move_tests.cpp" cat_moves.obj game_reader.obj room_labels.obj cat_details.obj mew_ui_api.obj /Fe:native_move_tests.exe /link user32.lib
if errorlevel 1 goto :failed
native_move_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 "%ROOMCAT_ROOT%tests\cat_actions_tests.cpp" cat_actions.obj /Fe:cat_actions_tests.exe
if errorlevel 1 goto :failed
cat_actions_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 /I"%ROOMCAT_ROOT%vendor\mew-ui-api\src\native" "%ROOMCAT_ROOT%tests\native_action_tests.cpp" cat_actions.obj cat_moves.obj game_reader.obj room_labels.obj cat_details.obj mew_ui_api.obj /Fe:native_action_tests.exe /link user32.lib
if errorlevel 1 goto :failed
native_action_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 "%ROOMCAT_ROOT%tests\gamepad_router_tests.cpp" /Fe:gamepad_router_tests.exe
if errorlevel 1 goto :failed
gamepad_router_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 "%ROOMCAT_ROOT%tests\mouse_router_tests.cpp" /Fe:mouse_router_tests.exe
if errorlevel 1 goto :failed
mouse_router_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 /DIMGUI_USE_WCHAR32 /I"%ROOMCAT_ROOT%vendor\imgui" "%ROOMCAT_ROOT%tests\panel_input_tests.cpp" "%ROOMCAT_ROOT%tests\parent_panel_tests.cpp" "%ROOMCAT_ROOT%tests\newborn_plan_tests.cpp" "%ROOMCAT_ROOT%tests\adult_plan_tests.cpp" "%ROOMCAT_ROOT%tests\newborn_batch_tests.cpp" "%ROOMCAT_ROOT%tests\screening_panel_tests.cpp" "%ROOMCAT_ROOT%tests\screening_choices_tests.cpp" "%ROOMCAT_ROOT%tests\screening_rules_tests.cpp" "%ROOMCAT_ROOT%tests\screening_settings_panel_tests.cpp" "%ROOMCAT_ROOT%tests\language_panel_tests.cpp" "%ROOMCAT_ROOT%tests\ui_theme_preview.cpp" "%ROOMCAT_ROOT%tests\ui_test_host.cpp" room_labels.obj cat_details.obj cat_moves.obj cat_actions.obj newborn_plan.obj adult_plan.obj newborn_batch.obj ui_skin_data.obj imgui.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj /Fe:panel_input_tests.exe /link /OPT:REF
if errorlevel 1 goto :failed
panel_input_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 /DIMGUI_USE_WCHAR32 /I"%ROOMCAT_ROOT%vendor\imgui" "%ROOMCAT_ROOT%tests\gamepad_panel_tests.cpp" "%ROOMCAT_ROOT%tests\ui_test_host.cpp" room_labels.obj cat_details.obj cat_moves.obj cat_actions.obj newborn_plan.obj adult_plan.obj newborn_batch.obj ui_skin_data.obj imgui.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj /Fe:gamepad_panel_tests.exe /link /OPT:REF
if errorlevel 1 goto :failed
gamepad_panel_tests.exe
if not "%errorlevel%"=="0" goto :failed
cl /nologo /std:c++17 /EHsc /MT /utf-8 /O2 /W3 /DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD /DIMGUI_USE_WCHAR32 /I"%ROOMCAT_ROOT%vendor\imgui" "%ROOMCAT_ROOT%tests\font_cache_tests.cpp" imgui.obj imgui_draw.obj imgui_tables.obj imgui_widgets.obj /Fe:font_cache_tests.exe /link user32.lib
if errorlevel 1 goto :failed
font_cache_tests.exe
if not "%errorlevel%"=="0" goto :failed
popd
exit /b 0
:failed
popd
exit /b 1
