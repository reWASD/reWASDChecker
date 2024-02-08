/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Disc Soft FZE LLC. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *
 *  reWASDChecker.cpp
 *  This example shows how any application can check that reWASD service is running and provides some remapping services.
 *
 *--------------------------------------------------------------------------------------------*/

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#define REWASD_PIPE_NAME _T("\\\\.\\pipe\\reWASDService")

//Values for ServiceFlags
//It is recommended for application to check these flags periodically, for example once per minute,
//because user may apply different profiles or switch slots on the fly.
//Also the service itself may be stopped and restarted.
//If service version is too old and does not support some flag then application may treat it as incompatible and disable
//some functionality. i.e. treat it as if the flag is set.

//This flag indicates that turbo or some other function is enabled, which may be considered by game as a cheat.
//This flag was added in service version 1.39.
#define REWASD_SERVICE_FLAG_CHEATS_ENABLED                  0x0001

#define REWASD_SERVICE_FLAG_CHEATS_ENABLED_MAJOR_VERSION    1
#define REWASD_SERVICE_FLAG_CHEATS_ENABLED_MINOR_VERSION    39

//This flag indicates that virtual gamepad is enabled.
//Some application mayb not support virtual gamepads at all.
//But application can also check REWASD_SERVICE_FLAG_MOUSE_TO_GAMEPAD_ENABLED to verify if user uses mouse and add
//restriction only for this case (eg. disable Aim Assist only).
//This flag was added in service version 5.28.
#define REWASD_SERVICE_FLAG_VIRTUAL_GAMEPAD_ENABLED         0x0002

//This flag indicates that mapping of mouse movement to virtual gamepad is enabled,
//and is used with REWASD_SERVICE_FLAG_VIRTUAL_GAMEPAD_ENABLED only.
//For such case some games may not provide Aim Assist.
//This flag was added in service version 5.28.
#define REWASD_SERVICE_FLAG_MOUSE_TO_GAMEPAD_ENABLED        0x0004

#define REWASD_SERVICE_FLAG_VIRTUAL_AND_MOUSE_MAJOR_VERSION 5
#define REWASD_SERVICE_FLAG_VIRTUAL_AND_MOUSE_MINOR_VERSION 28

typedef struct _REWASD_CHECK_VERSION_REQUEST{
    //Must be size of this structure, i.e. 8
    ULONG   Size;
    //Must be 0
    ULONG   Command;
}REWASD_CHECK_VERSION_REQUEST, *PREWASD_CHECK_VERSION_REQUEST;

typedef struct _REWASD_CHECK_VERSION_RESPONSE{
    //The value of this field must be at least size of this structure. i.e. 12
    //Service usually returns more data but it depends on version of reWASD and used for internal purposes,
    //so applications should not depend on it.
    ULONG   Size;
    //Must be 0 to indicate successful operation.
    ULONG   Status;
    //Service has own versioning scheme, not depending on official reWASD versions,
    //but newer version of reWASD always uses service with same or newer version.
    UCHAR   ServiceMajorVersion;
    UCHAR   ServiceMinorVersion;
    //These are flags which mean some operational parameters of currently active profile.
    //Check REWASD_SERVICE_FLAG_... definitions above.
    //New flags may be added in future.
    USHORT  ServiceFlags;
}REWASD_CHECK_VERSION_RESPONSE, *PREWASD_CHECK_VERSION_RESPONSE;

int main()
{
    DWORD err;

    //Try to open communication pipe with reWASD service.
    HANDLE hPipe = INVALID_HANDLE_VALUE;

    ULONG NumRetries = 3;

    while (NumRetries--)
    {
        hPipe = CreateFile(REWASD_PIPE_NAME,
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL,           // default security attributes
                           OPEN_EXISTING,
                           0,
                           NULL);

        // Break if the pipe handle is valid.
        if (hPipe != INVALID_HANDLE_VALUE)
        {
            break;
        }

        err = GetLastError();

        if (err == ERROR_ACCESS_DENIED)
        {
            break;
        }

        if (NumRetries)
        {
            Sleep(10);
        }
    }

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        //Unable to open pipe - seems service is not running
        printf("Unable to connect to reWASD service, error %d!\n", err);

        return err;
    }

    printf("Connected to reWASD service.\n");

    //At this point application knows that reWASD service is running and may decide to inform the user
    //that remapper is active and some functionality may be disabled.
    //In this case no more checks are needed.
    //But it is highly recommended to also query reWASD service version and check which functionality is used to make better decision
    //and not try to restrict the user who may need remapper for legitimate purposes (disabled users etc).

    //Change pipe to message-read mode.
    DWORD dwMode = PIPE_READMODE_MESSAGE;

    if (!SetNamedPipeHandleState(hPipe,
                                 &dwMode, // new pipe mode
                                 NULL,    // don't set maximum bytes
                                 NULL))   // don't set maximum time
    {
        err = GetLastError();

        printf("SetNamedPipeHandleState failed, error %d!\n", err);

        CloseHandle(hPipe);

        return err;
    }

    //Check service version
    REWASD_CHECK_VERSION_REQUEST CheckVersionRequest = {sizeof(CheckVersionRequest), 0};
    UCHAR OutBuffer[256];
    DWORD BytesRead = 0;

    //Clear output buffer
    memset(OutBuffer, 0, sizeof(OutBuffer));

    if (!TransactNamedPipe(hPipe,
                          &CheckVersionRequest,
                          sizeof(CheckVersionRequest),
                          OutBuffer,
                          sizeof(OutBuffer),
                          &BytesRead,
                          NULL))
    {
        err = GetLastError();

        printf("TransactNamedPipe failed, error %d!\n", err);

        CloseHandle(hPipe);

        return err;
    }

    PREWASD_CHECK_VERSION_RESPONSE CheckVersionResponse = (PREWASD_CHECK_VERSION_RESPONSE)OutBuffer;

    if (BytesRead < sizeof(REWASD_CHECK_VERSION_RESPONSE) ||
        CheckVersionResponse->Size < sizeof(REWASD_CHECK_VERSION_RESPONSE) ||
        CheckVersionResponse->Status != 0)
    {
        printf("Service version response is invalid!\n");

        CloseHandle(hPipe);

        return err;
    }

    UCHAR MajorVersion = CheckVersionResponse->ServiceMajorVersion;
    UCHAR MinorVersion = CheckVersionResponse->ServiceMinorVersion;

    printf("Service version %u.%02u.\n", MajorVersion, MinorVersion);

    printf("Service flags 0x%04X.\n", CheckVersionResponse->ServiceFlags);

    if (MajorVersion < REWASD_SERVICE_FLAG_CHEATS_ENABLED_MAJOR_VERSION ||
        (MajorVersion == REWASD_SERVICE_FLAG_CHEATS_ENABLED_MAJOR_VERSION &&
         MinorVersion < REWASD_SERVICE_FLAG_CHEATS_ENABLED_MINOR_VERSION))
    {
        printf("Profile with turbo or combo (possible cheats) could be present. Unable to check in this version.\n");
    }
    else
    {
        if (CheckVersionResponse->ServiceFlags & REWASD_SERVICE_FLAG_CHEATS_ENABLED)
        {
            printf("Some profile with turbo or combo (possible cheats) is active.\n");
        }
        else
        {
            printf("No profile with cheats is active.\n");
        }
    }

    if (MajorVersion < REWASD_SERVICE_FLAG_VIRTUAL_AND_MOUSE_MAJOR_VERSION ||
        (MajorVersion == REWASD_SERVICE_FLAG_VIRTUAL_AND_MOUSE_MAJOR_VERSION &&
        MinorVersion < REWASD_SERVICE_FLAG_VIRTUAL_AND_MOUSE_MINOR_VERSION))
    {
        printf("Virtual gamepad and mouse mapping could be present. Unable to check in this version.\n");
    }
    else
    {
        if (CheckVersionResponse->ServiceFlags & REWASD_SERVICE_FLAG_VIRTUAL_GAMEPAD_ENABLED)
        {
            //This is the case when user has created virtual gamepad of some kind.
            printf("Some profile with virtual gamepad is active -\n");

            if (CheckVersionResponse->ServiceFlags & REWASD_SERVICE_FLAG_MOUSE_TO_GAMEPAD_ENABLED)
            {
                ///This flag indicates that mapping of mouse movement to virtual gamepad is enabled.
                printf("   mouse movement mapping is present.\n");
            }
            else
            {
                printf("   mouse movement mapping is not present.\n");
            }
        }
        else
        {
            printf("No profile with virtual gamepad is active.\n");
        }
    }

    CloseHandle(hPipe);

    return 0;
}
