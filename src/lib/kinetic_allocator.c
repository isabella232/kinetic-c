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

#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include "kinetic_memory.h"
#include "kinetic_resourcewaiter.h"
#include "kinetic_resourcewaiter_types.h"
#include <stdlib.h>
#include <pthread.h>


KineticSession* KineticAllocator_NewSession(struct bus * b, KineticSessionConfig* config)
{
    (void)b; // TODO: combine session w/connection, which will use this variable
    
    // Allocate a new session
    KineticSession* session = KineticCalloc(1, sizeof(KineticSession));
    if (session == NULL) {
        LOG0("Failed allocating a new session!");
        return NULL;
    }

    // Copy the supplied config into the session config
    session->config = *config;
    memcpy(session->config.keyData, config->hmacKey.data, config->hmacKey.len);
    // Update pointer to copy of key data
    session->config.hmacKey.data = session->config.keyData;
    strncpy(session->config.host, config->host, sizeof(session->config.host));
    session->timeoutSeconds = config->timeoutSeconds;

    return session;
}

void KineticAllocator_FreeSession(KineticSession* session)
{
    if (session != NULL) {
        KineticFree(session);
    }
}

KineticConnection* KineticAllocator_NewConnection(struct bus * b, KineticSession* const session)
{
    KineticConnection* connection = KineticCalloc(1, sizeof(KineticConnection));
    if (connection == NULL) {
        LOG0("Failed allocating new Connection!");
        return NULL;
    }
    KineticResourceWaiter_Init(&connection->connectionReady);
    connection->pSession = session;
    connection->messageBus = b;
    connection->socket = -1;  // start with an invalid file descriptor
    return connection;
}

void KineticAllocator_FreeConnection(KineticConnection* connection)
{
    KINETIC_ASSERT(connection != NULL);
    KineticResourceWaiter_Destroy(&connection->connectionReady);
    KineticFree(connection);
}

KineticResponse * KineticAllocator_NewKineticResponse(size_t const valueLength)
{
    KineticResponse * response = KineticCalloc(1, sizeof(*response) + valueLength);
    if (response == NULL) {
        LOG0("Failed allocating new response!");
        return NULL;
    }
    return response;
}

void KineticAllocator_FreeKineticResponse(KineticResponse * response)
{
    KINETIC_ASSERT(response != NULL);

    if (response->command != NULL) {
        protobuf_c_message_free_unpacked(&response->command->base, NULL);
    }
    if (response->proto != NULL) {
        protobuf_c_message_free_unpacked(&response->proto->base, NULL);
    }
    KineticFree(response);
}

KineticOperation* KineticAllocator_NewOperation(KineticSession* const session)
{
    KINETIC_ASSERT(session != NULL);

    LOGF3("Allocating new operation on session %p", (void*)session);
    KineticOperation* newOperation =
        (KineticOperation*)KineticCalloc(1, sizeof(KineticOperation));
    if (newOperation == NULL) {
        LOGF0("Failed allocating new operation on session %p", (void*)session);
        return NULL;
    }
    KineticOperation_Init(newOperation, session);
    newOperation->request = (KineticRequest*)KineticCalloc(1, sizeof(KineticRequest));
    if (newOperation->request == NULL) {
        LOGF0("Failed allocating new PDU on session %p", (void*)session);
        KineticFree(newOperation);
        return NULL;
    }
    KineticRequest_Init(newOperation->request, session);
    return newOperation;
}

void KineticAllocator_FreeOperation(KineticOperation* operation)
{
    KINETIC_ASSERT(operation != NULL);
    LOGF3("Freeing operation %p on session %p", (void*)operation, (void*)operation->session);
    if (operation->request != NULL) {
        KineticFree(operation->request);
        operation->request = NULL;
    }
    if (operation->response != NULL) {
        KineticAllocator_FreeKineticResponse(operation->response);
        operation->response = NULL;
    }
    KineticFree(operation);
}

void KineticAllocator_FreeP2PProtobuf(KineticProto_Command_P2POperation* proto_p2pOp)
{
    if (proto_p2pOp != NULL) {
        if (proto_p2pOp->peer != NULL) {
            free(proto_p2pOp->peer);
            proto_p2pOp->peer = NULL;
        }
        if (proto_p2pOp->operation != NULL) {
            for(size_t i = 0; i < proto_p2pOp->n_operation; i++) {
                if (proto_p2pOp->operation[i] != NULL) {
                    if (proto_p2pOp->operation[i]->p2pop != NULL) {
                        KineticAllocator_FreeP2PProtobuf(proto_p2pOp->operation[i]->p2pop);
                        proto_p2pOp->operation[i]->p2pop = NULL;
                    }
                    if (proto_p2pOp->operation[i]->status != NULL) {
                        free(proto_p2pOp->operation[i]->status);
                        proto_p2pOp->operation[i]->status = NULL;
                    }
                    free(proto_p2pOp->operation[i]);
                    proto_p2pOp->operation[i] = NULL;
                }
            }
            free(proto_p2pOp->operation);
            proto_p2pOp->operation = NULL;
        }
        free(proto_p2pOp);
    }
}
