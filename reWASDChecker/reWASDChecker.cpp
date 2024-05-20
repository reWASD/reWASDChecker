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

#define REWASD_PIPE_NAME _T("\\\\.\\pipe\\{C65657D6-B9FA-40E9-B95E-6F3BDED9EFE4}")

//Values for ServiceFlags
//It is recommended for application to check these flags periodically, for example once per minute,
//because user may apply different profiles or switch slots on the fly.
//Also the service itself may be stopped and restarted.

//This flag indicates that turbo or some other function is enabled, which may be considered by game as a cheat.
//This flag was added in service version 1.39.
#define REWASD_SERVICE_FLAG_CHEATS_ENABLED                  0x0001

//This flag indicates that virtual gamepad is enabled.
//Some application may not support virtual gamepads at all.
//But application can also check REWASD_SERVICE_FLAG_MOUSE_TO_GAMEPAD_ENABLED to verify if user uses mouse and add
//restriction only for this case (eg. disable Aim Assist only).
#define REWASD_SERVICE_FLAG_VIRTUAL_GAMEPAD_ENABLED         0x0002

//This flag indicates that mapping of mouse movement to virtual gamepad is enabled,
//and is used with REWASD_SERVICE_FLAG_VIRTUAL_GAMEPAD_ENABLED only.
//For such case some games may not provide Aim Assist.
#define REWASD_SERVICE_FLAG_MOUSE_TO_GAMEPAD_ENABLED        0x0004

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

    //Check service version
    REWASD_CHECK_VERSION_REQUEST CheckVersionRequest = {sizeof(CheckVersionRequest), 0};
    UCHAR OutBuffer[256];
    DWORD BytesRead = 0;

    //Upper 8 bits of Size are used as command Tag starting from reWASD 7.3
    CheckVersionRequest.Size |= 1UL << 24;//set tag to any non-zero value

    //Clear output buffer
    memset(OutBuffer, 0, sizeof(OutBuffer));

    if (!CallNamedPipe(REWASD_PIPE_NAME,
                       &CheckVersionRequest,
                       sizeof(CheckVersionRequest),
                       OutBuffer,
                       sizeof(OutBuffer),
                       &BytesRead,
                       NMPWAIT_NOWAIT))
    {
        err = GetLastError();

        printf("Unable to call reWASD service, error %d!\n", err);

        return err;
    }

    PREWASD_CHECK_VERSION_RESPONSE CheckVersionResponse = (PREWASD_CHECK_VERSION_RESPONSE)OutBuffer;

    //Remove command tag from upper 8 bits
    CheckVersionResponse->Size &= 0xFFFFFF;

    if (BytesRead < sizeof(REWASD_CHECK_VERSION_RESPONSE) ||
        CheckVersionResponse->Size < sizeof(REWASD_CHECK_VERSION_RESPONSE) ||
        CheckVersionResponse->Status != 0)
    {
        printf("reWASD service response is invalid!\n");

        return ERROR_INVALID_DATA;
    }

    UCHAR MajorVersion = CheckVersionResponse->ServiceMajorVersion;
    UCHAR MinorVersion = CheckVersionResponse->ServiceMinorVersion;

    printf("reWASD service version %u.%02u.\n", MajorVersion, MinorVersion);

    printf("reWASD service flags 0x%04X.\n", CheckVersionResponse->ServiceFlags);

    if (CheckVersionResponse->ServiceFlags & REWASD_SERVICE_FLAG_CHEATS_ENABLED)
    {
        printf("Some profile with turbo or combo (possible cheats) is active.\n");
    }
    else
    {
        printf("No profile with cheats is active.\n");
    }

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

    return ERROR_SUCCESS;
}
