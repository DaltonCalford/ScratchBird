@echo off
setlocal enabledelayedexpansion enableextensions

@rem git config --global user.email "you@example.com"
@rem git config --global user.name "Your Name"
set GITCMD=C:\mix\Git\bin\git.exe 

set qa_root=%~dp0
set file_ext=^<pytest_file^>
set git_text=Updated "!file_ext!": adjust expected stdout/stderr to current FB version.
if .%1.==.. goto syntax

set pytest_file=%1

if NOT .%2.==.. (
    set /a i=0
    @rem echo all inp arguments: ^|"%*"^|
    for /f "tokens=1* delims= " %%a in ("%*") do (
        set customer_comment=%%b
    )
    @rem echo customer  comment: ^|!customer_comment!^|

    set git_text=Added/Updated "!file_ext!": !customer_comment!
)

set joblog=%~dpn0.log
set tmplog=%~dpn0.tmp

del !joblog! 2>nul
del !tmplog! 2>nul

for /f %%a in ("!pytest_file!") do (
    @rem set file_ext=%%~nxa
    set file_ext=%%a
    set text_chk=!file_ext:%qa_root%=!
    if .!text_chk!.==.!file_ext!. (
        @rem .fbt was specified WITHOUT path
        set file_ext=%cd%\!file_ext!
    ) 
    set file_ext=!file_ext:%qa_root%=!

)
set git_text=!git_text:^<pytest_file^>=%file_ext%!

(
    echo Log for: %~f0 !pytest_file!
    echo Created !date! !time! on host '%COMPUTERNAME%'
    echo.
    echo Comment is: git_text=!git_text!
) >>!joblog!

set msg=!date! !time! Processing command: !GITCMD! add !pytest_file!
echo !msg!
echo !msg!>>!joblog!

@rem ############################
@rem ###    g i t    a d d    ###
@rem ############################
!GITCMD! add !pytest_file! 1>!tmplog! 2>&1
set /a elevel=!errorlevel!
echo elevel=!elevel!
echo elevel=!elevel!>>!joblog!

type !tmplog!
type !tmplog! >>!joblog!

if !elevel! GTR 0 (
   echo ERROR OCCURED. Check log:
   echo -------------------------
   type !tmplog!
   echo -------------------------
   del !tmplog!
   goto :final
)

!GITCMD! status !pytest_file! 1>>!joblog! 2>&1

set msg=!date! !time! Processing command: !GITCMD! commit -m "!git_text!" -- !pytest_file!
echo !msg!
echo !msg!>>!joblog!

@rem ##################################
@rem ###    g i t    c o m m i t    ###
@rem ##################################
!GITCMD! commit -m "!git_text!" -- !pytest_file! 1>!tmplog! 2>&1
set elevel=!errorlevel!
echo elevel=!elevel!
echo elevel=!elevel!>>!joblog!
type !tmplog! >>!joblog!
if !elevel! GTR 0 (
   echo ERROR OCCURED. Check log:
   echo -------------------------
   type !tmplog!
   echo -------------------------
   del !tmplog!
   goto :final
)


set msg=!date! !time! Processing command: !GITCMD! push 
echo !msg!
echo !msg!>>!joblog!

@rem ##############################
@rem ###    g i t    p u s h    ###
@rem ##############################
!GITCMD! push 1>!tmplog! 2>&1
set elevel=!errorlevel!
echo elevel=!elevel!
echo elevel=!elevel!>>!joblog!
type !tmplog! >>!joblog!
if !elevel! GTR 0 (
   echo ERROR OCCURED. Check log:
   echo -------------------------
   type !tmplog!
   echo -------------------------
   del !tmplog!
   goto :final
)

del !tmplog! 2>nul

echo ------ OVERALL LOG: --------
type !joblog!
echo ----------------------------

set msg=!date! !time! Check result of commits here:
echo !msg!
!GITCMD! config --get remote.origin.url
(
    echo !msg!
    !GITCMD! config --get remote.origin.url
) >> !joblog!

@rem #######################################
@rem ###    s e n d i n g     m a i l    ###
@rem #######################################
call %~dp0qa-sendmail.bat "!git_text!" !joblog!

@rem https://github.com/FirebirdSQL/fbt-repository.git

goto final

:syntax
    echo.
    echo Syntax: 
    echo 1. %~f0 ^<pytest_file^>
    echo.
    echo.    Commit will be done with comment:
    echo.    !git_text!
    echo.
    echo.
    echo 2. %~f0 ^<pytest_file^> some very clever comment here
    echo.
    echo.    Commit will be done with comment:
    echo.    some very clever comment here
    echo.
    pause
    goto final
:final
    echo Bye-bye from %~f0

