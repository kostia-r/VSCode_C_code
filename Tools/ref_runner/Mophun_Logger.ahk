#NoEnv
#Persistent
#SingleInstance, Off
SetBatchLines, -1
SetTitleMatchMode, 2

; AutoHotkey v1 script.
; Args:
;   %1% = output trace log path
;   %2% = optional stop flag path

targetTitle := "Mophun Trace Window"

if 0 >= 1
    logfile = %1%
else
    logfile := A_ScriptDir "\logs\mophun_stream_log.txt"

if 0 >= 2
    stopfile = %2%
else
    stopfile := ""

SplitPath, logfile, , logdir
FileCreateDir, %logdir%
FileDelete, %logfile%

lastText := ""

SetTimer, CheckStop, 200
SetTimer, DumpConsole, 200
return

CheckStop:
    if (stopfile != "") {
        if FileExist(stopfile) {
            ExitApp
        }
    }
return

DumpConsole:
    WinGet, pid, PID, %targetTitle%
    if (!pid)
        return

    DllCall("FreeConsole")
    if !DllCall("AttachConsole", "UInt", pid)
        return

    hOut := DllCall("GetStdHandle", "Int", -11, "Ptr")
    if (hOut = -1) {
        DllCall("FreeConsole")
        return
    }

    VarSetCapacity(info, 22, 0)
    if !DllCall("GetConsoleScreenBufferInfo", "Ptr", hOut, "Ptr", &info) {
        DllCall("FreeConsole")
        return
    }

    width  := NumGet(info, 0, "Short")
    height := NumGet(info, 2, "Short")
    total := width * height

    VarSetCapacity(buf, total * 2, 0)
    VarSetCapacity(charsRead, 4, 0)

    coord := 0

    ok := DllCall("ReadConsoleOutputCharacterW"
        , "Ptr", hOut
        , "Ptr", &buf
        , "UInt", total
        , "UInt", coord
        , "Ptr", &charsRead)

    DllCall("FreeConsole")

    if (!ok)
        return

    raw := StrGet(&buf, total, "UTF-16")

    currentText := ""
    Loop, %height%
    {
        y := A_Index - 1
        line := SubStr(raw, y * width + 1, width)
        line := RTrim(line, " `t")
        if (line != "")
            currentText .= line "`n"
    }

    if (currentText = "")
        return

    if (lastText = "") {
        FileAppend, %currentText%, %logfile%, UTF-8
        lastText := currentText
        return
    }

    newPart := GetNewPart(lastText, currentText)

    if (newPart != "") {
        FileAppend, %newPart%, %logfile%, UTF-8
    }

    lastText := currentText
return

GetNewPart(oldText, newText) {
    oldLen := StrLen(oldText)
    newLen := StrLen(newText)

    max := oldLen < newLen ? oldLen : newLen

    Loop, %max%
    {
        n := max - A_Index + 1

        oldTail := SubStr(oldText, oldLen - n + 1, n)
        newHead := SubStr(newText, 1, n)

        if (oldTail = newHead) {
            return SubStr(newText, n + 1)
        }
    }

    return "`n--- BUFFER GAP / RESET ---`n" newText
}
