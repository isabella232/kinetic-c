/*
* kinetic-c
* Copyright (C) 2014 Seagate Technology.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "kinetic_admin_client.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_operation.h"
#include "kinetic_builder.h"
#include "kinetic_allocator.h"
#include "kinetic_auth.h"
#include "kinetic_device_info.h"
#include "kinetic_acl.h"

#ifdef TEST
struct ACL *ACLs = NULL;
#endif

KineticClient * KineticAdminClient_Init(KineticClientConfig *config)
{
    return KineticClient_Init(config);
}

void KineticAdminClient_Shutdown(KineticClient * const client)
{
    KineticClient_Shutdown(client);
}

KineticStatus KineticAdminClient_CreateSession(KineticSessionConfig * const config,
    KineticClient * const client, KineticSession** session)
{
    return KineticClient_CreateSession(config, client, session);
}

KineticStatus KineticAdminClient_DestroySession(KineticSession * const session)
{
    return KineticClient_DestroySession(session);
}


KineticStatus KineticAdminClient_SetErasePin(KineticSession * const session,
    ByteArray old_pin, ByteArray new_pin)
{
    KineticStatus status;
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    // Ensure PIN arrays have data if non-empty
    if ((old_pin.len > 0 && old_pin.data == NULL) ||
        (new_pin.len > 0 && new_pin.data == NULL)) {
        return KINETIC_STATUS_MISSING_PIN;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticBuilder_BuildSetPin(operation, old_pin, new_pin, false);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_SecureErase(KineticSession * const session,
    ByteArray pin)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticStatus status;
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) { return status; }

    // Ensure PIN array has data if non-empty
    if (pin.len > 0 && pin.data == NULL) {
        return KINETIC_STATUS_MISSING_PIN;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticBuilder_BuildErase(operation, true, &pin);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_InstantErase(KineticSession * const session,
    ByteArray pin)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticStatus status;
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    // Ensure PIN array has data if non-empty
    if (pin.len > 0 && pin.data == NULL) {
        return KINETIC_STATUS_MISSING_PIN;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticBuilder_BuildErase(operation, false, &pin);
    return KineticController_ExecuteOperation(operation, NULL);
}


KineticStatus KineticAdminClient_SetLockPin(KineticSession * const session,
    ByteArray old_pin, ByteArray new_pin)
{
    KineticStatus status;
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    // Ensure PIN arrays have data if non-empty
    if ((old_pin.len > 0 && old_pin.data == NULL) ||
        (new_pin.len > 0 && new_pin.data == NULL)) {
        return KINETIC_STATUS_MISSING_PIN;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticBuilder_BuildSetPin(operation, old_pin, new_pin, true);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_LockDevice(KineticSession * const session,
    ByteArray pin)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticStatus status;
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    // Ensure PIN array has data if non-empty
    if (pin.len > 0 && pin.data == NULL) {
        return KINETIC_STATUS_MISSING_PIN;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticBuilder_BuildLockUnlock(operation, true, &pin);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_UnlockDevice(KineticSession * const session,
    ByteArray pin)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticStatus status;
    status = KineticAuth_EnsureSslEnabled(&session->config);
    if (status != KINETIC_STATUS_SUCCESS) {return status;}

    // Ensure PIN array has data if non-empty
    if (pin.len > 0 && pin.data == NULL) {
        return KINETIC_STATUS_MISSING_PIN;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticBuilder_BuildLockUnlock(operation, false, &pin);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_GetLog(KineticSession * const session,
                                   KineticLogInfo_Type type,
                                   KineticLogInfo** info,
                                   KineticCompletionClosure* closure)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(info != NULL);

    KineticProto_Command_GetLog_Type protoType =
        KineticLogInfo_Type_to_KineticProto_Command_GetLog_Type(type);
    if (protoType == KINETIC_PROTO_COMMAND_GET_LOG_TYPE_INVALID_TYPE) {
        return KINETIC_STATUS_INVALID_LOG_TYPE;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticBuilder_BuildGetLog(operation, protoType, BYTE_ARRAY_NONE, info);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

KineticStatus KineticAdminClient_GetDeviceSpecificLog(KineticSession * const session,
                                   ByteArray name,
                                   KineticLogInfo** info,
                                   KineticCompletionClosure* closure)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(info != NULL);

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    // Initialize request
    KineticBuilder_BuildGetLog(operation, KINETIC_PROTO_COMMAND_GET_LOG_TYPE_DEVICE, name, info);

    // Execute the operation
    return KineticController_ExecuteOperation(operation, closure);
}

void KineticClient_FreeLogInfo(KineticSession * const session,
                                  KineticLogInfo* info)
{
    assert(session != NULL);
    if (info) { KineticLogInfo_Free(info); }

    /* The session is not currently used, but part of the API to allow
     * a different memory management strategy. */
    (void)session;
}

KineticStatus KineticAdminClient_SetClusterVersion(KineticSession * const session,
    int64_t version)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}

    KineticBuilder_BuildSetClusterVersion(operation, version);
    return KineticController_ExecuteOperation(operation, NULL);
}

KineticStatus KineticAdminClient_SetACL(KineticSession * const session,
        const char *ACLPath) {
    assert(session != NULL);
    assert(session->connection != NULL);
    if (ACLPath == NULL) {
        return KINETIC_STATUS_INVALID_REQUEST;
    }

    #ifndef TEST
    struct ACL *ACLs = NULL;
    #endif
    KineticACLLoadResult acl_res = KineticACL_LoadFromFile(ACLPath, &ACLs);
    if (acl_res != ACL_OK) {
        return KINETIC_STATUS_ACL_ERROR;
    }

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {
        printf("!operation\n");
        return KINETIC_STATUS_MEMORY_ERROR;
    }

    // Initialize request
    KineticBuilder_BuildSetACL(operation, ACLs);
    KineticStatus status = KineticController_ExecuteOperation(operation, NULL);

    // FIXME: confirm ACLs are freed

    return status;
}

KineticStatus KineticAdminClient_UpdateFirmware(KineticSession * const session,
    char const * const fw_path)
{
    assert(session != NULL);
    assert(session->connection != NULL);

    KineticOperation* operation = KineticAllocator_NewOperation(session);
    if (operation == NULL) {return KINETIC_STATUS_MEMORY_ERROR;}
    
    KineticStatus status = KineticBuilder_BuildUpdateFirmware(operation, fw_path);
    if (status != KINETIC_STATUS_SUCCESS) {
        return status;
    }
    
    return KineticController_ExecuteOperation(operation, NULL);
}
