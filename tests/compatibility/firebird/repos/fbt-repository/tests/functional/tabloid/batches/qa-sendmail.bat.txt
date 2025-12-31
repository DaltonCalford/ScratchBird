@echo off
setlocal enabledelayedexpansion enableextensions

set cfg=%~dpn0.cfg
set msg_subj=%1
set msg_body_file=%2

echo !date! !time!. %~f0.

if .!msg_subj!.==.. (
    echo.
    echo Missed input argument #1: Subject of message.
    echo.
    goto final
)
echo Sending message with subject:
echo !msg_subj!
echo Parsing config file !cfg!.

set err_setenv=0

::::::::::::::::::::::::::::::::
:::: R E A D    C O N G I G ::::
::::::::::::::::::::::::::::::::

call :readcfg !cfg! !err_setenv!


@rem mail_from_user=firebird-qa
@rem mail_from_pswd=******
@rem mail_from_home=yandex.ru
@rem mail_targ_addr=someone@company.com

set tmp=%~dpn0.tmp
set log=%~dpn0.log


(
    echo From: ^<!mail_from_user!@!mail_from_home!^>
    echo To: ^<!mail_targ_addr!^>

    set left_char=!msg_subj:~0,1!
    set righ_char=!msg_subj:~-1!
    @rem REMOVE LEADING AND TRAILING DOUBLE QUOTES FROM SUBJECT
    @rem ######################################################
    set result=!left_char:"=!
    if .!result!.==.. (
       set result=!righ_char:"=!
       if .!result!.==.. (
          set msg_subj=!msg_subj:~1,-1!
       )
    )

    echo Subject: !msg_subj!
    echo.
    if .!msg_body_file!.==.. (
       echo Created !date! !time! on host '%COMPUTERNAME%'.
       echo Input argument #2, 'msg_body_file', - was not specified.
    ) else (
       type !msg_body_file!
    )
) >!tmp!

@rem do NOT use this:  --mail-rcpt "!mail_targ_addr!"  -- considered often as spam!!
@rem decided to use sending mail to own address and create forward rule on mail-box

set run_cmd=!curl_bin! --verbose --url "!mail_from_smtp!" ^
    --ssl-reqd --mail-from "!mail_from_user!@!mail_from_home!" ^
    --mail-rcpt "!mail_from_user!@!mail_from_home!" ^
    --upload-file "!tmp!" ^
    --user "!mail_from_user!@yandex.ru:!mail_from_pswd!" ^
    --insecure ^
    1^>!log! 2^>^&1

@rem echo cUrl command:
@rem echo !run_cmd!

echo Running cUrl - sending mail. . .

cmd /c !run_cmd!

findstr /m /i /r /c:"250 .* ok" !log! >nul
if NOT errorlevel 1 (
    findstr /m /i /c:"SPAM" !log! >nul
    if NOT errorlevel 1 (
        echo ### FAILURE ### Message REJECTED as SPAM.
    ) else (
        echo.
        echo Mail sending log CONTAINS "250 OK" and NOT contain "SPAM" phrase.
        echo Check mailbox: !mail_from_user!@!mail_from_home!
        echo.
        echo +++++++++++++++
        echo ^|^|^| SUCCESS ^|^|^|
        echo +++++++++++++++
    )
) else (
    echo ### FAILURE ### Can not find phrase '250 .* ok' in the mail sending log.
)
(
    echo Used file for upload:
    echo =====================
    type !tmp!
    echo =====================
    echo.
    echo Used command: 
    echo !run_cmd!
) >>!log!
del !tmp!

echo Final log can be seen here: !log!
goto final

@rem -+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-+=-

:readcfg
    set cfg=%1
    set err_setenv=0
    @rem ::: NB ::: Space + TAB should be inside `^[ ]` pattern!
    @rem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    for /F "tokens=*" %%a in ('findstr /i /r /c:"^[ 	]*[a-z,0-9]" %cfg%') do (
      if "%%a" neq "" (

        @rem Detect whether value of parameter contain quotes or no. If yes than this
        @rem value should NOT be changed by removing its whitespaces.

          for /F "tokens=1-2 delims==" %%i in ("%%a") do (
            @rem echo Parsed: param="%%i" _tmp_val_="%%j"
            set _tmp_par_=%%i
            call :trim _tmp_par_ !_tmp_par_!

            if "%%j"=="" (
              set err_setenv=1
              echo. && echo ### NO VALUE found for parameter "%%i" ### && echo.
            ) else (
              for /F "tokens=1" %%p in ("!_tmp_par_!") do (
                set _tmp_val_=%%j
                call :trim _tmp_val_ !_tmp_val_!
                set %%p=!_tmp_val_!
              )
            )
          )
      )
    )
    set _tmp_par_=
    set _tmp_val_=
    set %~2 = %err_setenv%
    @rem if .%err_setenv%.==.1. goto err_setenv

goto:eof

:sho
    setlocal
    set msg=%1
    set msg=!msg:`="!
    set log=%2
    if .!log!.==.. (
        echo Internal func sho: missed argument for log file.
        @rem ::: 25.06.2018 do NOT use here reference to  %1, use here
        @rem ::: only variable that stores its value: !msg!
        @rem ::: Execution control can jump here even for correct
        @rem ::: input msg if it contains closing parenthesis ")"
        echo Arg. #1 = ^|!msg!^|
        goto final
    ) 

    set left_char=!msg:~0,1!
    set righ_char=!msg:~-1!

    @rem REMOVE LEADING AND TRAILING DOUBLE QUOTES:
    @rem ##########################################
    set result=!left_char:"=!
    if .!result!.==.. (
       set result=!righ_char:"=!
       if .!result!.==.. (
          set msg=!msg:~1,-1!
       )
    )

    set dts=!time!
    set dts=!dts: =!
    set dts=10!dts:,=.!
    set dts=!dts:~-11!
    set msg=!dts!. !msg!
    echo !msg!
    echo !msg!>>!log!

endlocal & goto:eof

:bulksho
    setlocal
    set tmp=%1
    set log=%2
    for /f "tokens=*" %%a in (!tmp!) do (
       set line=%%a
       set line=!line:"=`!
       call :sho "!line!" !log!
    )
    del !tmp!
endlocal & goto:eof
     
:haltHelper
()
exit /b

:trim
    setLocal
    @rem EnableDelayedExpansion
    set Params=%*
    for /f "tokens=1*" %%a in ("!Params!") do endLocal & set %1=%%b
goto:eof



:final
    echo.
    echo Bye-bye from %~f0

