/*
 * Process Hacker -
 *   PE viewer
 *
 * Copyright (C) 2010 wj32
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <peview.h>
#include <objbase.h>

PPH_STRING PvFileName = NULL;

static BOOLEAN NTAPI PvCommandLineCallback(
    _In_opt_ PPH_COMMAND_LINE_OPTION Option,
    _In_opt_ PPH_STRING Value,
    _In_opt_ PVOID Context
    )
{
    if (!Option)
        PhSwapReference(&PvFileName, Value);

    return TRUE;
}

INT WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ PWSTR lpCmdLine,
    _In_ INT nCmdShow
    )
{
    static PH_COMMAND_LINE_OPTION options[] =
    {
        { 0, L"h", NoArgumentType }
    };
    PPH_STRING commandLine;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if (!NT_SUCCESS(PhInitializePhLib()))
        return 1;

    // Create a mutant for the installer.
    {
        HANDLE mutantHandle;
        PPH_STRING objectName;
        OBJECT_ATTRIBUTES objectAttributes;
        UNICODE_STRING objectNameUs;
        PH_FORMAT format[2];

        PhInitFormatS(&format[0], L"PeViewerMutant_");
        PhInitFormatU(&format[1], HandleToUlong(NtCurrentProcessId()));

        objectName = PhFormat(format, 2, 16);
        PhStringRefToUnicodeString(&objectName->sr, &objectNameUs);

        InitializeObjectAttributes(
            &objectAttributes,
            &objectNameUs,
            OBJ_CASE_INSENSITIVE,
            PhGetNamespaceHandle(),
            NULL
            );

        NtCreateMutant(
            &mutantHandle,
            MUTANT_QUERY_STATE,
            &objectAttributes,
            TRUE
            );

        PhDereferenceObject(objectName);
    }

    PhGuiSupportInitialization();
    PhSettingsInitialization();
    PeInitializeSettings();
    PvPropInitialization();
    PhTreeNewInitialization();

    PhApplicationName = L"PE Viewer";

    if (!NT_SUCCESS(PhGetProcessCommandLine(NtCurrentProcess(), &commandLine)))
        return 1;

    PhParseCommandLine(
        &commandLine->sr,
        options,
        sizeof(options) / sizeof(PH_COMMAND_LINE_OPTION),
        PH_COMMAND_LINE_IGNORE_FIRST_PART,
        PvCommandLineCallback,
        NULL
        );
    PhDereferenceObject(commandLine);

    if (!PvFileName)
    {
        static PH_FILETYPE_FILTER filters[] =
        {
            { L"Supported files (*.exe;*.dll;*.com;*.ocx;*.sys;*.scr;*.cpl;*.ax;*.acm;*.lib;*.winmd;*.efi;*.pdb)", L"*.exe;*.dll;*.com;*.ocx;*.sys;*.scr;*.cpl;*.ax;*.acm;*.lib;*.winmd;*.efi;*.pdb" },
            { L"All files (*.*)", L"*.*" }
        };
        PVOID fileDialog;

        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        fileDialog = PhCreateOpenFileDialog();
        PhSetFileDialogFilter(fileDialog, filters, ARRAYSIZE(filters));

        if (PhShowFileDialog(NULL, fileDialog))
        {
            PvFileName = PhGetFileDialogFileName(fileDialog);
        }

        PhFreeFileDialog(fileDialog);
    }

    if (!PvFileName)
        return 1;

    if (PhEndsWithString2(PvFileName, L".lnk", TRUE))
    {
        PPH_STRING targetFileName;

        targetFileName = PvResolveShortcutTarget(PvFileName);

        if (targetFileName)
            PhMoveReference(&PvFileName, targetFileName);
    }

    if (PhEndsWithString2(PvFileName, L".lib", TRUE))
        PvLibProperties();
    else if (PhEndsWithString2(PvFileName, L".pdb", TRUE))
        PvPdbProperties();
    else
    {
        NTSTATUS status;

        status = PhLoadMappedImageEx(
            PvFileName->Buffer, 
            NULL, 
            TRUE,
            &PvMappedImage
            );

        if (NT_SUCCESS(status))
        {
            switch (PvMappedImage.Signature)
            {
            case IMAGE_DOS_SIGNATURE:
                PvPeProperties();
                break;
            case IMAGE_ELF_SIGNATURE:
                PvExlfProperties();
                break;
            default:
                status = STATUS_IMAGE_SUBSYSTEM_NOT_PRESENT;
                break;
            }
        }

        if (NT_SUCCESS(status))
            PhUnloadMappedImage(&PvMappedImage);
        else
            PhShowStatus(NULL, L"Unable to load the file.", status, 0);
    }

    PeSaveSettings();

    return 0;
}
